/*
    Windows ConPTY backend for qmltermwidget.

    Copyright 2025 cool-retro-term contributors

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "ConPtyProcess.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QtDebug>

// ─── ConPtyReadThread ───────────────────────────────────────────────

ConPtyReadThread::ConPtyReadThread(HANDLE readHandle, QObject *parent)
    : QThread(parent)
    , m_readHandle(readHandle)
    , m_running(true)
{
}

void ConPtyReadThread::run()
{
    const DWORD bufSize = 4096;
    char buffer[bufSize];

    while (m_running) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(m_readHandle, buffer, bufSize, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            // Pipe broken or closed — process likely exited
            break;
        }
        emit dataReady(QByteArray(buffer, bytesRead));
    }
}

void ConPtyReadThread::stop()
{
    m_running = false;
}

// ─── ConPtyWaitThread ───────────────────────────────────────────────

ConPtyWaitThread::ConPtyWaitThread(HANDLE processHandle, QObject *parent)
    : QThread(parent)
    , m_processHandle(processHandle)
{
}

void ConPtyWaitThread::run()
{
    WaitForSingleObject(m_processHandle, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(m_processHandle, &code);
    emit processExited(static_cast<int>(code));
}

// ─── ConPtyProcess ──────────────────────────────────────────────────

ConPtyProcess::ConPtyProcess(QObject *parent)
    : QObject(parent)
    , m_hPC(INVALID_HANDLE_VALUE)
    , m_hInputWrite(INVALID_HANDLE_VALUE)
    , m_hOutputRead(INVALID_HANDLE_VALUE)
    , m_hInputRead(INVALID_HANDLE_VALUE)
    , m_hOutputWrite(INVALID_HANDLE_VALUE)
    , m_hProcess(INVALID_HANDLE_VALUE)
    , m_hThread(INVALID_HANDLE_VALUE)
    , m_processId(0)
    , m_attrList(nullptr)
    , m_readThread(nullptr)
    , m_waitThread(nullptr)
    , m_running(false)
    , m_exitCode(-1)
{
}

ConPtyProcess::~ConPtyProcess()
{
    if (m_running)
        terminate();
    cleanup();
}

bool ConPtyProcess::start(const QString &program,
                          const QStringList &arguments,
                          const QStringList &environment,
                          const QString &workingDirectory,
                          int rows, int cols)
{
    if (m_running) {
        qWarning() << "ConPtyProcess::start - process already running";
        return false;
    }

    // ── Step 1: Create pipes ──
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE; // ConPTY manages inheritance itself

    if (!CreatePipe(&m_hInputRead, &m_hInputWrite, &sa, 0)) {
        qWarning() << "ConPtyProcess: failed to create input pipe:" << GetLastError();
        return false;
    }
    if (!CreatePipe(&m_hOutputRead, &m_hOutputWrite, &sa, 0)) {
        qWarning() << "ConPtyProcess: failed to create output pipe:" << GetLastError();
        cleanup();
        return false;
    }

    // ── Step 2: Create pseudo console ──
    COORD size;
    size.X = static_cast<SHORT>(cols);
    size.Y = static_cast<SHORT>(rows);

    HRESULT hr = CreatePseudoConsole(size, m_hInputRead, m_hOutputWrite, 0, &m_hPC);
    if (FAILED(hr)) {
        qWarning() << "ConPtyProcess: CreatePseudoConsole failed:" << hr;
        cleanup();
        return false;
    }

    // ── Step 3: Initialize startup info with pseudo console attribute ──
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    m_attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attrListSize));
    if (!m_attrList) {
        qWarning() << "ConPtyProcess: failed to allocate attribute list";
        cleanup();
        return false;
    }
    if (!InitializeProcThreadAttributeList(m_attrList, 1, 0, &attrListSize)) {
        qWarning() << "ConPtyProcess: InitializeProcThreadAttributeList failed:" << GetLastError();
        cleanup();
        return false;
    }
    if (!UpdateProcThreadAttribute(m_attrList, 0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   m_hPC, sizeof(HPCON), nullptr, nullptr)) {
        qWarning() << "ConPtyProcess: UpdateProcThreadAttribute failed:" << GetLastError();
        cleanup();
        return false;
    }

    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(si);
    si.lpAttributeList = m_attrList;

    // ── Step 4: Build command line ──
    // Konsole convention: arguments[0] is program name, rest are args
    QString cmdLine;
    cmdLine += QLatin1Char('"') + program + QLatin1Char('"');
    for (int i = 1; i < arguments.size(); ++i) {
        cmdLine += QLatin1Char(' ');
        // Quote arguments containing spaces
        if (arguments[i].contains(QLatin1Char(' ')))
            cmdLine += QLatin1Char('"') + arguments[i] + QLatin1Char('"');
        else
            cmdLine += arguments[i];
    }

    // ── Step 5: Build environment block ──
    // Start with current environment and add/override
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &pair : environment) {
        int eqPos = pair.indexOf(QLatin1Char('='));
        if (eqPos > 0) {
            env.insert(pair.left(eqPos), pair.mid(eqPos + 1));
        }
    }

    // Build null-separated, double-null-terminated Unicode environment block
    QStringList envList = env.toStringList();
    QString envBlock;
    for (const QString &e : envList) {
        envBlock += e;
        envBlock += QChar(0);
    }
    envBlock += QChar(0);

    // ── Step 6: Create process ──
    PROCESS_INFORMATION pi = {};
    std::wstring cmdLineW = cmdLine.toStdWString();
    std::wstring workDirW = workingDirectory.isEmpty() ? std::wstring() : workingDirectory.toStdWString();
    std::wstring envBlockW(reinterpret_cast<const wchar_t*>(envBlock.utf16()), envBlock.size());

    BOOL ok = CreateProcessW(
        nullptr,                                    // lpApplicationName
        cmdLineW.data(),                            // lpCommandLine (mutable)
        nullptr,                                    // lpProcessAttributes
        nullptr,                                    // lpThreadAttributes
        FALSE,                                      // bInheritHandles
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
        envBlockW.data(),                           // lpEnvironment
        workDirW.empty() ? nullptr : workDirW.c_str(), // lpCurrentDirectory
        &si.StartupInfo,                            // lpStartupInfo
        &pi                                         // lpProcessInformation
    );

    if (!ok) {
        qWarning() << "ConPtyProcess: CreateProcessW failed:" << GetLastError()
                    << "cmd:" << cmdLine;
        cleanup();
        return false;
    }

    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;
    m_processId = pi.dwProcessId;

    // Close child-side pipe handles (ConPTY owns them now)
    CloseHandle(m_hInputRead);
    m_hInputRead = INVALID_HANDLE_VALUE;
    CloseHandle(m_hOutputWrite);
    m_hOutputWrite = INVALID_HANDLE_VALUE;

    m_running = true;

    // ── Step 7: Start background threads ──
    m_readThread = new ConPtyReadThread(m_hOutputRead, this);
    connect(m_readThread, &ConPtyReadThread::dataReady,
            this, &ConPtyProcess::onDataReady, Qt::QueuedConnection);
    m_readThread->start();

    m_waitThread = new ConPtyWaitThread(m_hProcess, this);
    connect(m_waitThread, &ConPtyWaitThread::processExited,
            this, &ConPtyProcess::onProcessExited, Qt::QueuedConnection);
    m_waitThread->start();

    qDebug() << "ConPtyProcess: started PID" << m_processId << "cmd:" << cmdLine;
    return true;
}

void ConPtyProcess::write(const char *data, int length)
{
    if (!m_running || m_hInputWrite == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    WriteFile(m_hInputWrite, data, static_cast<DWORD>(length), &written, nullptr);
}

void ConPtyProcess::resize(int rows, int cols)
{
    if (m_hPC == INVALID_HANDLE_VALUE)
        return;

    COORD size;
    size.X = static_cast<SHORT>(cols);
    size.Y = static_cast<SHORT>(rows);
    HRESULT hr = ResizePseudoConsole(m_hPC, size);
    if (FAILED(hr))
        qWarning() << "ConPtyProcess: ResizePseudoConsole failed:" << hr;
}

void ConPtyProcess::terminate()
{
    if (!m_running)
        return;

    // Closing the pseudo console sends an exit signal to the child
    if (m_hPC != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(m_hPC);
        m_hPC = INVALID_HANDLE_VALUE;
    }

    // Wait briefly for graceful exit
    if (m_hProcess != INVALID_HANDLE_VALUE) {
        if (WaitForSingleObject(m_hProcess, 2000) == WAIT_TIMEOUT) {
            kill();
        }
    }
}

void ConPtyProcess::kill()
{
    if (m_hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(m_hProcess, 1);
    }
}

DWORD ConPtyProcess::pid() const
{
    return m_processId;
}

bool ConPtyProcess::isRunning() const
{
    return m_running;
}

int ConPtyProcess::exitCode() const
{
    return m_exitCode;
}

void ConPtyProcess::onDataReady(const QByteArray &data)
{
    emit dataReceived(data.constData(), data.size());
}

void ConPtyProcess::onProcessExited(int exitCode)
{
    m_exitCode = exitCode;
    m_running = false;
    emit processExited(exitCode);
}

void ConPtyProcess::cleanup()
{
    // Stop read thread
    if (m_readThread) {
        m_readThread->stop();
        // Close the read handle to unblock ReadFile
        if (m_hOutputRead != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hOutputRead);
            m_hOutputRead = INVALID_HANDLE_VALUE;
        }
        m_readThread->wait(3000);
        delete m_readThread;
        m_readThread = nullptr;
    }

    // Wait for wait thread
    if (m_waitThread) {
        m_waitThread->wait(3000);
        delete m_waitThread;
        m_waitThread = nullptr;
    }

    // Close pseudo console
    if (m_hPC != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(m_hPC);
        m_hPC = INVALID_HANDLE_VALUE;
    }

    // Close pipe handles
    if (m_hInputWrite != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hInputWrite);
        m_hInputWrite = INVALID_HANDLE_VALUE;
    }
    if (m_hInputRead != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hInputRead);
        m_hInputRead = INVALID_HANDLE_VALUE;
    }
    if (m_hOutputRead != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hOutputRead);
        m_hOutputRead = INVALID_HANDLE_VALUE;
    }
    if (m_hOutputWrite != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hOutputWrite);
        m_hOutputWrite = INVALID_HANDLE_VALUE;
    }

    // Close process handles
    if (m_hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hThread);
        m_hThread = INVALID_HANDLE_VALUE;
    }
    if (m_hProcess != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hProcess);
        m_hProcess = INVALID_HANDLE_VALUE;
    }

    // Free attribute list
    if (m_attrList) {
        DeleteProcThreadAttributeList(m_attrList);
        HeapFree(GetProcessHeap(), 0, m_attrList);
        m_attrList = nullptr;
    }

    m_processId = 0;
}

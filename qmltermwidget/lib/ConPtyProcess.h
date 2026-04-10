/*
    Windows ConPTY backend for qmltermwidget.

    Copyright 2025 cool-retro-term contributors

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.
*/

#ifndef CONPTYPROCESS_H
#define CONPTYPROCESS_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QStringList>
#include <QSize>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

/**
 * Background thread that performs blocking ReadFile() on the ConPTY
 * output pipe and forwards data to the main thread via signal.
 */
class ConPtyReadThread : public QThread
{
    Q_OBJECT
public:
    explicit ConPtyReadThread(HANDLE readHandle, QObject *parent = nullptr);
    void run() override;
    void stop();

signals:
    void dataReady(const QByteArray &data);

private:
    HANDLE m_readHandle;
    volatile bool m_running;
};

/**
 * Background thread that waits for the child process to exit
 * and emits a signal with the exit code.
 */
class ConPtyWaitThread : public QThread
{
    Q_OBJECT
public:
    explicit ConPtyWaitThread(HANDLE processHandle, QObject *parent = nullptr);
    void run() override;

signals:
    void processExited(int exitCode);

private:
    HANDLE m_processHandle;
};

/**
 * ConPtyProcess provides a Windows ConPTY-based terminal backend.
 *
 * It creates a pseudo console via CreatePseudoConsole(), launches a child
 * process attached to it, and provides read/write access to the terminal
 * I/O streams.
 *
 * This class replaces the KPty/KPtyDevice/KPtyProcess stack on Windows.
 */
class ConPtyProcess : public QObject
{
    Q_OBJECT

public:
    explicit ConPtyProcess(QObject *parent = nullptr);
    ~ConPtyProcess() override;

    /**
     * Start a process inside a ConPTY.
     *
     * @param program The executable path
     * @param arguments Command-line arguments (first element is program name, as per Konsole convention)
     * @param environment List of "KEY=VALUE" strings to add to environment
     * @param workingDirectory Initial working directory
     * @param rows Initial terminal height
     * @param cols Initial terminal width
     * @return true on success
     */
    bool start(const QString &program,
               const QStringList &arguments,
               const QStringList &environment,
               const QString &workingDirectory,
               int rows, int cols);

    /**
     * Write data to the process stdin (via ConPTY input pipe).
     */
    void write(const char *data, int length);

    /**
     * Resize the pseudo console.
     */
    void resize(int rows, int cols);

    /**
     * Gracefully terminate the process by closing the pseudo console.
     */
    void terminate();

    /**
     * Forcefully kill the process.
     */
    void kill();

    /**
     * @return the Windows process ID, or 0 if not running.
     */
    DWORD pid() const;

    /**
     * @return true if the child process is still running.
     */
    bool isRunning() const;

    /**
     * @return the exit code of the child process (valid after processExited).
     */
    int exitCode() const;

signals:
    /**
     * Emitted when data is received from the process stdout (via ConPTY output pipe).
     */
    void dataReceived(const char *data, int length);

    /**
     * Emitted when the child process exits.
     */
    void processExited(int exitCode);

private slots:
    void onDataReady(const QByteArray &data);
    void onProcessExited(int exitCode);

private:
    void cleanup();

    // ConPTY handle (HPCON)
    HPCON m_hPC;

    // Pipe handles: we write to m_hInput, read from m_hOutput
    HANDLE m_hInputWrite;   // our end: write to this to send data to process
    HANDLE m_hOutputRead;   // our end: read from this to get data from process
    HANDLE m_hInputRead;    // child's end (closed after CreateProcess)
    HANDLE m_hOutputWrite;  // child's end (closed after CreateProcess)

    // Process handles
    HANDLE m_hProcess;
    HANDLE m_hThread;
    DWORD m_processId;

    // Startup info with attribute list
    LPPROC_THREAD_ATTRIBUTE_LIST m_attrList;

    // Background threads
    ConPtyReadThread *m_readThread;
    ConPtyWaitThread *m_waitThread;

    // State
    bool m_running;
    int m_exitCode;

    // Buffer for dataReceived signal forwarding
    QByteArray m_pendingData;
};

#endif // CONPTYPROCESS_H

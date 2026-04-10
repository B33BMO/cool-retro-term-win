/*
 * This file is a part of QTerminal - http://gitorious.org/qterminal
 *
 * This file was un-linked from KDE and modified
 * by Maxim Bourmistrov <maxim@unixconn.com>
 *
 */

/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "Pty.h"

#if !defined(Q_OS_WIN)
// System (Unix only)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <termios.h>
#include <csignal>

#include "kpty.h"
#include "kptydevice.h"
#endif

// Qt
#include <QStringList>
#include <QtDebug>

using namespace Konsole;

// ═══════════════════════════════════════════════════════════════════
//  Windows ConPTY implementation
// ═══════════════════════════════════════════════════════════════════
#if defined(Q_OS_WIN)

Pty::Pty(QObject* parent)
    : QObject(parent)
    , _conPty(new ConPtyProcess(this))
{
    init();
}

Pty::~Pty()
{
}

void Pty::init()
{
    _windowColumns = 0;
    _windowLines = 0;
    _eraseChar = 0;
    _xonXoff = true;
    _utf8 = true;

    connect(_conPty, &ConPtyProcess::dataReceived,
            this, &Pty::receivedData);
    connect(_conPty, &ConPtyProcess::processExited,
            this, &Pty::onProcessExited);
}

void Pty::onProcessExited(int exitCode)
{
    emit finished(exitCode, QProcess::NormalExit);
}

void Pty::setWindowSize(int lines, int cols)
{
    _windowColumns = cols;
    _windowLines = lines;
    _conPty->resize(lines, cols);
}

QSize Pty::windowSize() const
{
    return {_windowColumns, _windowLines};
}

void Pty::setFlowControlEnabled(bool enable)
{
    _xonXoff = enable;
    // ConPTY handles flow control internally
}

bool Pty::flowControlEnabled() const
{
    return _xonXoff;
}

void Pty::setUtf8Mode(bool enable)
{
    _utf8 = enable;
    // ConPTY uses UTF-16 internally; UTF-8 is the default for modern Windows
}

void Pty::setErase(char erase)
{
    _eraseChar = erase;
    // ConPTY handles erase character internally
}

char Pty::erase() const
{
    return _eraseChar;
}

void Pty::addEnvironmentVariables(const QStringList& environment)
{
    _environment = environment;
}

int Pty::start(const QString& program,
               const QStringList& programArguments,
               const QStringList& environment,
               ulong winid,
               bool addToUtmp)
{
    Q_UNUSED(addToUtmp);

    // Build environment list with our additions
    QStringList env = environment;
    env << QString("WINDOWID=%1").arg(winid);
    env << QStringLiteral("COLORTERM=truecolor");
    env << QStringLiteral("TERM=xterm-256color");

    // Ensure minimum window size
    int rows = _windowLines > 0 ? _windowLines : 24;
    int cols = _windowColumns > 0 ? _windowColumns : 80;

    bool ok = _conPty->start(program, programArguments, env,
                             _workingDirectory, rows, cols);
    return ok ? 0 : -1;
}

void Pty::setEmptyPTYProperties()
{
    // No-op on Windows
}

void Pty::setWriteable(bool writeable)
{
    Q_UNUSED(writeable);
    // No-op on Windows (no TTY permissions)
}

void Pty::sendData(const char* data, int length)
{
    if (!length)
        return;
    _conPty->write(data, length);
}

void Pty::dataReceived()
{
    // Not used on Windows; ConPtyProcess emits dataReceived directly
}

void Pty::lockPty(bool lock)
{
    Q_UNUSED(lock);
    // TODO: Support for locking the Pty
}

int Pty::foregroundProcessGroup() const
{
    // On Windows, return the ConPTY process ID
    // A more accurate implementation could use ConsoleAPI to get
    // the foreground process, but this suffices for now
    return static_cast<int>(_conPty->pid());
}

qint64 Pty::processId() const
{
    return static_cast<qint64>(_conPty->pid());
}

bool Pty::isRunning() const
{
    return _conPty->isRunning();
}

void Pty::setWorkingDirectory(const QString &dir)
{
    _workingDirectory = dir;
}

// ═══════════════════════════════════════════════════════════════════
//  Unix PTY implementation (original code)
// ═══════════════════════════════════════════════════════════════════
#else  // !Q_OS_WIN

void Pty::setWindowSize(int lines, int cols)
{
  _windowColumns = cols;
  _windowLines = lines;

  if (pty()->masterFd() >= 0)
    pty()->setWinSize(lines, cols);
}
QSize Pty::windowSize() const
{
    return {_windowColumns,_windowLines};
}

void Pty::setFlowControlEnabled(bool enable)
{
  _xonXoff = enable;

  if (pty()->masterFd() >= 0)
  {
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!enable)
      ttmode.c_iflag &= ~(IXOFF | IXON);
    else
      ttmode.c_iflag |= (IXOFF | IXON);
    if (!pty()->tcSetAttr(&ttmode))
      qWarning() << "Unable to set terminal attributes.";
  }
}
bool Pty::flowControlEnabled() const
{
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        return ttmode.c_iflag & IXOFF &&
               ttmode.c_iflag & IXON;
    }
    qWarning() << "Unable to get flow control status, terminal not connected.";
    return false;
}

void Pty::setUtf8Mode(bool enable)
{
#ifdef IUTF8 // XXX not a reasonable place to check it.
  _utf8 = enable;

  if (pty()->masterFd() >= 0)
  {
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!enable)
      ttmode.c_iflag &= ~IUTF8;
    else
      ttmode.c_iflag |= IUTF8;
    if (!pty()->tcSetAttr(&ttmode))
      qWarning() << "Unable to set terminal attributes.";
  }
#endif
}

void Pty::setErase(char erase)
{
  _eraseChar = erase;

  if (pty()->masterFd() >= 0)
  {
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    ttmode.c_cc[VERASE] = erase;
    if (!pty()->tcSetAttr(&ttmode))
      qWarning() << "Unable to set terminal attributes.";
  }
}

char Pty::erase() const
{
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttyAttributes;
        pty()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    }

    return _eraseChar;
}

void Pty::addEnvironmentVariables(const QStringList& environment)
{
    QListIterator<QString> iter(environment);
    while (iter.hasNext())
    {
        QString pair = iter.next();

        // split on the first '=' character
        int pos = pair.indexOf(QLatin1Char('='));

        if ( pos >= 0 )
        {
            QString variable = pair.left(pos);
            QString value = pair.mid(pos+1);

            setEnv(variable,value);
        }
    }
}

int Pty::start(const QString& program,
               const QStringList& programArguments,
               const QStringList& environment,
               ulong winid,
               bool addToUtmp
               //const QString& dbusService,
               //const QString& dbusSession
               )
{
  clearProgram();

  // For historical reasons, the first argument in programArguments is the
  // name of the program to execute, so create a list consisting of all
  // but the first argument to pass to setProgram()
  Q_ASSERT(programArguments.count() >= 1);
  setProgram(program, programArguments.mid(1));

  addEnvironmentVariables(environment);

  setEnv(QLatin1String("WINDOWID"), QString::number(winid));
  setEnv(QLatin1String("COLORTERM"), QLatin1String("truecolor"));

  // unless the LANGUAGE environment variable has been set explicitly
  // set it to a null string
  setEnv(QLatin1String("LANGUAGE"),QString(),false /* do not overwrite existing value if any */);

  setUseUtmp(addToUtmp);

  struct ::termios ttmode;
  pty()->tcGetAttr(&ttmode);
  if (!_xonXoff)
    ttmode.c_iflag &= ~(IXOFF | IXON);
  else
    ttmode.c_iflag |= (IXOFF | IXON);
#ifdef IUTF8 // XXX not a reasonable place to check it.
  if (!_utf8)
    ttmode.c_iflag &= ~IUTF8;
  else
    ttmode.c_iflag |= IUTF8;
#endif

  if (_eraseChar != 0)
      ttmode.c_cc[VERASE] = _eraseChar;

  if (!pty()->tcSetAttr(&ttmode))
    qWarning() << "Unable to set terminal attributes.";

  pty()->setWinSize(_windowLines, _windowColumns);

  KProcess::start();

  if (!waitForStarted())
      return -1;

  return 0;
}

void Pty::setEmptyPTYProperties()
{
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!_xonXoff)
      ttmode.c_iflag &= ~(IXOFF | IXON);
    else
      ttmode.c_iflag |= (IXOFF | IXON);
  #ifdef IUTF8 // XXX not a reasonable place to check it.
    if (!_utf8)
      ttmode.c_iflag &= ~IUTF8;
    else
      ttmode.c_iflag |= IUTF8;
  #endif

    if (_eraseChar != 0)
        ttmode.c_cc[VERASE] = _eraseChar;

    if (!pty()->tcSetAttr(&ttmode))
      qWarning() << "Unable to set terminal attributes.";
}

void Pty::setWriteable(bool writeable)
{
  struct stat sbuf;
  stat(pty()->ttyName(), &sbuf);
  if (writeable)
    chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP);
  else
    chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
}

Pty::Pty(int masterFd, QObject* parent)
    : KPtyProcess(masterFd,parent)
{
    init();
}
Pty::Pty(QObject* parent)
    : KPtyProcess(parent)
{
    init();
}
void Pty::init()
{
  _windowColumns = 0;
  _windowLines = 0;
  _eraseChar = 0;
  _xonXoff = true;
  _utf8 =true;

  connect(pty(), SIGNAL(readyRead()) , this , SLOT(dataReceived()));
  setPtyChannels(KPtyProcess::AllChannels);
}

Pty::~Pty()
{
}

void Pty::sendData(const char* data, int length)
{
  if (!length)
      return;

  if (!pty()->write(data,length))
  {
    qWarning() << "Pty::doSendJobs - Could not send input data to terminal process.";
    return;
  }
}

void Pty::dataReceived()
{
     QByteArray data = pty()->readAll();
    emit receivedData(data.constData(),data.count());
}

void Pty::lockPty(bool lock)
{
    Q_UNUSED(lock);

// TODO: Support for locking the Pty
  //if (lock)
    //suspend();
  //else
    //resume();
}

int Pty::foregroundProcessGroup() const
{
    int pid = tcgetpgrp(pty()->masterFd());

    if ( pid != -1 )
    {
        return pid;
    }

    return 0;
}

void Pty::setupChildProcess()
{
    KPtyProcess::setupChildProcess();

    // reset all signal handlers
    // this ensures that terminal applications respond to
    // signals generated via key sequences such as Ctrl+C
    // (which sends SIGINT)
    struct sigaction action;
    sigset_t sigset;
    sigemptyset(&action.sa_mask);
    sigemptyset(&sigset);
    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    for (int signal=1;signal < NSIG; signal++) {
        sigaction(signal,&action,nullptr);
        sigaddset(&sigset, signal);
    }
    sigprocmask(SIG_UNBLOCK, &sigset, nullptr);
}

#endif // Q_OS_WIN

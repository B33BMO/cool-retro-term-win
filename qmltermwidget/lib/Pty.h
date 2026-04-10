/*
 * This file is a part of QTerminal - http://gitorious.org/qterminal
 *
 * This file was un-linked from KDE and modified
 * by Maxim Bourmistrov <maxim@unixconn.com>
 *
 */

/*
    This file is part of Konsole, KDE's terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
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

#ifndef PTY_H
#define PTY_H

// Qt
#include <QStringList>
#include <QVector>
#include <QList>
#include <QSize>
#include <QProcess>  // for QProcess::ExitStatus

#if defined(Q_OS_WIN)
// Windows: use ConPTY backend
#include "ConPtyProcess.h"
#else
// Unix: use KDE PTY stack
#include "kptyprocess.h"
#endif

namespace Konsole {

/**
 * The Pty class is used to start the terminal process,
 * send data to it, receive data from it and manipulate
 * various properties of the pseudo-teletype interface
 * used to communicate with the process.
 *
 * To use this class, construct an instance and connect
 * to the sendData slot and receivedData signal to
 * send data to or receive data from the process.
 *
 * To start the terminal process, call the start() method
 * with the program name and appropriate arguments.
 */
#if defined(Q_OS_WIN)
class Pty : public QObject
#else
class Pty: public KPtyProcess
#endif
{
Q_OBJECT

  public:

    /**
     * Constructs a new Pty.
     */
    explicit Pty(QObject* parent = nullptr);

#if !defined(Q_OS_WIN)
    /**
     * Construct a process using an open pty master.
     * See KPtyProcess::KPtyProcess()
     */
    explicit Pty(int ptyMasterFd, QObject* parent = nullptr);
#endif

    ~Pty() override;

    /**
     * Starts the terminal process.
     *
     * Returns 0 if the process was started successfully or non-zero
     * otherwise.
     */
    int start( const QString& program,
               const QStringList& arguments,
               const QStringList& environment,
               ulong winid,
               bool addToUtmp
             );

    /** set properties for "EmptyPTY" */
    void setEmptyPTYProperties();

    /** TODO: Document me */
    void setWriteable(bool writeable);

    /**
     * Enables or disables Xon/Xoff flow control.
     */
    void setFlowControlEnabled(bool on);

    /** Queries the terminal state and returns true if Xon/Xoff flow control is enabled. */
    bool flowControlEnabled() const;

    /**
     * Sets the size of the window (in lines and columns of characters)
     * used by this teletype.
     */
    void setWindowSize(int lines, int cols);

    /** Returns the size of the window used by this teletype. */
    QSize windowSize() const;

    /** TODO Document me */
    void setErase(char erase);

    /** */
    char erase() const;

    /**
     * Returns the process id of the teletype's current foreground
     * process.
     */
    int foregroundProcessGroup() const;

#if defined(Q_OS_WIN)
    /** Returns the process ID of the shell process. */
    qint64 processId() const;

    /** Returns true if the process is running. */
    bool isRunning() const;

    /** Sets the working directory for the process. */
    void setWorkingDirectory(const QString &dir);
#endif

  public slots:

    /**
     * Put the pty into UTF-8 mode on systems which support it.
     */
    void setUtf8Mode(bool on);

    /**
     * Suspend or resume processing of data from the standard
     * output of the terminal process.
     */
    void lockPty(bool lock);

    /**
     * Sends data to the process currently controlling the teletype.
     */
    void sendData(const char* buffer, int length);

  signals:

    /**
     * Emitted when a new block of data is received from
     * the teletype.
     */
    void receivedData(const char* buffer, int length);

#if defined(Q_OS_WIN)
    /**
     * Emitted when the process exits (Windows only; on Unix this comes from QProcess).
     */
    void finished(int exitCode, QProcess::ExitStatus exitStatus = QProcess::NormalExit);
#endif

  protected:
#if !defined(Q_OS_WIN)
      void setupChildProcess() override;
#endif

  private slots:
    // called when data is received from the terminal process
    void dataReceived();

#if defined(Q_OS_WIN)
    void onProcessExited(int exitCode);
#endif

  private:
      void init();

    // takes a list of key=value pairs and adds them
    // to the environment for the process
    void addEnvironmentVariables(const QStringList& environment);

    int  _windowColumns;
    int  _windowLines;
    char _eraseChar;
    bool _xonXoff;
    bool _utf8;

#if defined(Q_OS_WIN)
    ConPtyProcess *_conPty;
    QString _workingDirectory;
    QStringList _environment;
#endif
};

}

#endif // PTY_H

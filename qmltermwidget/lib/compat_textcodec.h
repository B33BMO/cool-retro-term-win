/*
    Minimal UTF-8-only QTextCodec stub for Windows/Qt6.

    In Qt 6, QTextCodec was moved to the Qt5Compat module. To avoid the
    Qt5Compat dependency, this header provides a minimal drop-in replacement
    that only supports UTF-8 (which is what modern terminal emulators use
    anyway — the cool-retro-term default is UTF-8).
*/
#ifndef COMPAT_TEXTCODEC_H
#define COMPAT_TEXTCODEC_H

#if defined(Q_OS_WIN) || defined(_WIN32)

#include <QString>
#include <QByteArray>
#include <QStringDecoder>
#include <QStringEncoder>

// Minimal stub providing what Emulation/Vt102Emulation need.
// Always represents UTF-8.
class QTextCodec
{
public:
    QTextCodec() {}
    virtual ~QTextCodec() {}

    static QTextCodec* codecForName(const char* /*name*/)
    {
        // Only UTF-8 is supported; all requests return the same instance
        static QTextCodec instance;
        return &instance;
    }

    static QTextCodec* codecForLocale()
    {
        return codecForName("UTF-8");
    }

    class QTextDecoder* makeDecoder() const;

    int mibEnum() const { return 106; /* UTF-8 MIB */ }

    QByteArray fromUnicode(const QString& str) const
    {
        return str.toUtf8();
    }

    QString toUnicode(const QByteArray& data) const
    {
        return QString::fromUtf8(data);
    }

    QString toUnicode(const char* chars, int len) const
    {
        return QString::fromUtf8(chars, len);
    }
};

class QTextDecoder
{
public:
    QTextDecoder() : m_decoder(QStringDecoder::Utf8) {}
    virtual ~QTextDecoder() {}

    QString toUnicode(const char* chars, int len)
    {
        return m_decoder.decode(QByteArrayView(chars, len));
    }

    QString toUnicode(const QByteArray& data)
    {
        return m_decoder.decode(data);
    }

private:
    QStringDecoder m_decoder;
};

inline QTextDecoder* QTextCodec::makeDecoder() const
{
    return new QTextDecoder();
}

#else
#include <QTextCodec>
#endif // Q_OS_WIN

#endif // COMPAT_TEXTCODEC_H

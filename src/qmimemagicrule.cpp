#include "qmimemagicrule.h"

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <qendian.h>

QT_BEGIN_NAMESPACE

static const char *kStrings[] = { "unknown", "string", "byte", "big16", "big32",
                                   "little16", "little32", "host16", "host32"};

typedef bool (*MatchFunction)(QMimeMagicRulePrivate* m_d, const QByteArray &data);

class QMimeMagicRulePrivate
{
public:
    QMimeMagicRule::Type type;
    QString value;
    int startPos;
    int endPos;

    QByteArray pattern; // String
    QList<int> bytes; // Byte
    quint16 value16; // *16
    quint32 value32; // *32

    MatchFunction matchFunction;
};

bool matchString(QMimeMagicRulePrivate* m_d, const QByteArray& data)
{
    // Quick check
    if ((m_d->startPos + m_d->pattern.size()) > data.size())
        return false;
    // Most common: some string at position 0:
    if (m_d->startPos == 0 && m_d->startPos == m_d->endPos)
        return data.startsWith(m_d->pattern);
    // Range
    if (data.size() <= m_d->startPos)
        return false;

    int end = m_d->endPos - m_d->startPos + m_d->pattern.size();
    return QByteArray::fromRawData(data.constData() + m_d->startPos,
                                   qMin(end, data.size())).contains(m_d->pattern);
}

bool matchBytes(QMimeMagicRulePrivate *m_d, const QByteArray &data)
{
    int bytesSize = m_d->bytes.size();
    if (bytesSize == 0)
        return false;

    const int dataSize = data.size();
    for (int start = m_d->startPos; start <= m_d->endPos; ++start) {
        if ((start + bytesSize) > dataSize)
            return false;

        int matchAt = 0;
        while (matchAt < bytesSize) {
            if (data.at(start + matchAt) != m_d->bytes.at(matchAt))
                break;
            ++matchAt;
        }
        if (matchAt == bytesSize)
            return true;
    }

    return false;
}

bool match16(QMimeMagicRulePrivate *m_d, const QByteArray &data)
{
    const char *p = data.constData() + m_d->startPos;
    const char *e = data.constData() + qMin(data.size() - 2, m_d->endPos + 1);
    while (p < e) {
        if (*reinterpret_cast<const quint16*>(p) == m_d->value16)
            return true;
        ++p;
    }
    return false;
}

bool match32(QMimeMagicRulePrivate *m_d, const QByteArray &data)
{
    const char *p = data.constData() + m_d->startPos;
    const char *e = data.constData() + qMin(data.size() - 4, m_d->endPos + 1);
    while (p < e) {
        if (*reinterpret_cast<const quint32*>(p) == m_d->value32)
            return true;
        ++p;
    }
    return false;
}

bool unknown(QMimeMagicRulePrivate *, const QByteArray &)
{
    return false;
}

bool validateByteSequence(const QString &sequence, QList<int> *bytes)
{
    // Expect an hex format value like this: \0x7f\0x45\0x4c\0x46
    const QStringList &byteSequence = sequence.split(QLatin1Char('\\'), QString::SkipEmptyParts);
    foreach (const QString &byte, byteSequence) {
        bool ok;
        const int hex = byte.toInt(&ok, 16);
        if (ok) {
            if (bytes)
                bytes->push_back(hex);
        } else {
            return false;
        }
    }
    return true;
}

QMimeMagicRule::QMimeMagicRule(Type type, const QString &string, int startPos, int endPos) :
    m_d(new QMimeMagicRulePrivate)
{
    m_d->type = type;
    m_d->value = string;
    m_d->startPos = startPos;
    m_d->endPos = endPos;

    uint value = 0;
    if (type >= Big16 && type <= Host32) {
        bool ok;
        value = string.toUInt(&ok, 0);

        if (!ok)
            qWarning() << QString("QMimeMagicRule::QMimeMagicRule: Can't convert %1 string to int").
                          arg(string);
    }

    switch (type) {
    case String:
        m_d->pattern = string.toUtf8();
        m_d->matchFunction = matchString;
        break;
    case Byte:
        if (!validateByteSequence(string, &m_d->bytes))
            m_d->bytes.clear();
        m_d->matchFunction = matchBytes;
        break;
    case Big16:
    case Host16: // reverse on little-endians
        m_d->value16 = qFromBigEndian<quint16>(value);
        m_d->matchFunction = match16;
        break;
    case Little16:
        m_d->value16 = qFromLittleEndian<quint16>(value);
        m_d->matchFunction = match16;
        break;
    case Big32:
    case Host32: // reverse on little-endians
        m_d->value32 = qFromBigEndian<quint32>(value);
        m_d->matchFunction = match32;
        break;
    case Little32:
        m_d->value32 = qFromLittleEndian<quint32>(value);
        m_d->matchFunction = match32;
        break;
    default:
        m_d->matchFunction = unknown;
    }
}

QMimeMagicRule::~QMimeMagicRule()
{
    delete m_d;
}

QMimeMagicRule::QMimeMagicRule(const QMimeMagicRule &other) :
    m_d(new QMimeMagicRulePrivate(*other.m_d))
{
}

QMimeMagicRule& QMimeMagicRule::operator=(const QMimeMagicRule &other)
{
    *m_d = *other.m_d;
    return *this;
}

QMimeMagicRule::Type QMimeMagicRule::type() const
{
    return m_d->type;
}

QString QMimeMagicRule::matchType() const
{
    return QLatin1String(kStrings[m_d->type]);
}

QString QMimeMagicRule::matchValue() const
{
    return m_d->value;
}

int QMimeMagicRule::startPos() const
{
    return m_d->startPos;
}

int QMimeMagicRule::endPos() const
{
    return m_d->endPos;
}

bool QMimeMagicRule::matches(const QByteArray &data) const
{
    return m_d->matchFunction(m_d, data);
}

QString QMimeMagicRule::toOffset(const QPair<int, int> &startEnd)
{
    return QString(QLatin1String("%1:%2")).arg(startEnd.first).arg(startEnd.second);
}

QPair<int, int> QMimeMagicRule::fromOffset(const QString &offset)
{
    const QStringList &startEnd = offset.split(QChar(QLatin1Char(':')));
    Q_ASSERT(startEnd.size() == 2);
    return qMakePair(startEnd.at(0).toInt(), startEnd.at(1).toInt());
}

QMimeMagicRule::Type QMimeMagicRule::stringToType(const QByteArray &type)
{
    for (int i = QMimeMagicRule::Unknown; i != QMimeMagicRule::Host32; i++) {
        if (type == kStrings[i])
            return (QMimeMagicRule::Type)i;
    }
    return QMimeMagicRule::Unknown;
}

QT_END_NAMESPACE

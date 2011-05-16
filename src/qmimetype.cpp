/**************************************************************************
**
** This file is part of QMime
**
** Based on Qt Creator source code
**
** Qt Creator Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**************************************************************************/

#include "qmimetype.h"
#include "qmimetype_p.h"

#include "qmimedatabase.h"
#include "magicmatcher_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

/*!
    \class GlobPattern
    \brief Glob pattern for file names for mime type matching.

    \sa MimeType, MimeDatabase, IMagicMatcher, MagicRuleMatcher, MagicRule, MagicStringRule, MagicByteRule
    \sa FileMatchContext, BinaryMatcher, HeuristicTextMagicMatcher
    \sa BaseMimeTypeParser, MimeTypeParser
*/

MimeGlobPattern::MimeGlobPattern(const QRegExp &regExp, unsigned weight) :
    m_regExp(regExp), m_weight(weight)
{
}

MimeGlobPattern::~MimeGlobPattern()
{
}

const QRegExp &MimeGlobPattern::regExp() const
{
    return m_regExp;
}

unsigned MimeGlobPattern::weight() const
{
    return m_weight;
}

/*!
    \class MimeType

    \brief Mime type data used in Qt Creator.

    Contains most information from standard mime type XML database files.

    Currently, magic of types "string", "bytes" is supported. In addition,
    C++ classes, derived from IMagicMatcher can be added to check
    on contents.

    In addition, the class provides a list of suffixes and a concept of the
    'preferred suffix' (derived from glob patterns). This is used for example
    to be able to configure the suffix used for C++-files in Qt Creator.

    Mime XML looks like:
    \code
    <?xml version="1.0" encoding="UTF-8"?>
    <mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
    <!-- Mime types must match the desktop file associations -->
      <mime-type type="application/vnd.nokia.qt.qmakeprofile">
        <comment xml:lang="en">Qt qmake Profile</comment>
        <glob pattern="*.pro" weight="50"/>
      </mime-type>
    </mime-info>
    \endcode

    \sa MimeDatabase, IMagicMatcher, MagicRuleMatcher, MagicRule, MagicStringRule, MagicByteRule, GlobPattern
    \sa FileMatchContext, BinaryMatcher, HeuristicTextMagicMatcher
    \sa BaseMimeTypeParser, MimeTypeParser
*/

QMimeTypeData::QMimeTypeData()
    // RE to match a suffix glob pattern: "*.ext" (and not sth like "Makefile" or
    // "*.log[1-9]"
    : suffixPattern(QLatin1String("^\\*\\.[\\w+]+$"))
{
    if (!suffixPattern.isValid())
        qWarning() << "MimeTypeData::MimeTypeData : invalid suffixPattern";
}

void QMimeTypeData::clear()
{
    type.clear();
    comment.clear();
    aliases.clear();
    globPatterns.clear();
    subClassesOf.clear();
    preferredSuffix.clear();
    suffixes.clear();
    magicMatchers.clear();
}

void QMimeTypeData::assignSuffix(const QString &pattern)
{
    if (suffixPattern.exactMatch(pattern)) {
        const QString suffix = pattern.right(pattern.size() - 2);
        suffixes.push_back(suffix);
        if (preferredSuffix.isEmpty())
            preferredSuffix = suffix;
    }
}

void QMimeTypeData::assignSuffixes(const QStringList &patterns)
{
    foreach (const QString &pattern, patterns)
        assignSuffix(pattern);
}

void QMimeTypeData::debug(QTextStream &str, int indent) const
{
    const QString indentS = QString(indent, QLatin1Char(' '));
    const QString comma = QString(1, QLatin1Char(','));
    str << indentS << "Type: " << type;
    if (!aliases.empty())
        str << " Aliases: " << aliases.join(comma);
    str << ", magic: " << magicMatchers.size() << '\n';
    str << indentS << "Comment: " << comment << '\n';
    if (!subClassesOf.empty())
        str << indentS << "SubClassesOf: " << subClassesOf.join(comma) << '\n';
    if (!globPatterns.empty()) {
        str << indentS << "Glob: ";
        foreach (const MimeGlobPattern &gp, globPatterns)
            str << gp.regExp().pattern() << '(' << gp.weight() << ')';
        str << '\n';
        if (!suffixes.empty()) {
            str <<  indentS << "Suffixes: " << suffixes.join(comma)
                << " preferred: " << preferredSuffix << '\n';
        }
    }
    str << '\n';
}

// ---------------- MimeType
QMimeType::QMimeType() :
    m_d(new QMimeTypeData)
{
}

QMimeType::QMimeType(const QMimeType &rhs) :
    m_d(rhs.m_d)
{
}

QMimeType &QMimeType::operator=(const QMimeType &rhs)
{
    if (this != &rhs)
        m_d = rhs.m_d;
    return *this;
}

QMimeType::QMimeType(const QMimeTypeData &d) :
    m_d(new QMimeTypeData(d))
{
}

QMimeType::~QMimeType()
{
}

void QMimeType::clear()
{
    m_d->clear();
}

bool QMimeType::isValid() const
{
    return !m_d->type.isEmpty();
}

QMimeType::operator bool() const
{
    return isValid();
}

bool QMimeType::isTopLevel() const
{
    return m_d->subClassesOf.empty();
}

QString QMimeType::type() const
{
    return m_d->type;
}

void QMimeType::setType(const QString &type)
{
    m_d->type = type;
}

QString QMimeType::comment() const
{
    return m_d->comment;
}

void QMimeType::setComment(const QString &comment)
{
    m_d->comment = comment;
}

// Return "en", "de", etc. derived from "en_US", de_DE".
static inline QString systemLanguage()
{
    QString name = QLocale::system().name();
    const int underScorePos = name.indexOf(QLatin1Char('_'));
    if (underScorePos != -1)
        name.truncate(underScorePos);
    return name;
}

QString QMimeType::localeComment(const QString &localeArg) const
{
    const QString locale = localeArg.isEmpty() ? systemLanguage() : localeArg;
    const QMimeTypeData::LocaleHash::const_iterator it = m_d->localeComments.constFind(locale);
    if (it == m_d->localeComments.constEnd())
        return m_d->comment;
    return it.value();
}

void QMimeType::setLocaleComment(const QString &locale, const QString &comment)
{
     m_d->localeComments[locale] = comment;
}

QStringList QMimeType::aliases() const
{
    return m_d->aliases;
}

void QMimeType::setAliases(const QStringList &a)
{
     m_d->aliases = a;
}

QList<MimeGlobPattern> QMimeType::globPatterns() const
{
    return m_d->globPatterns;
}

void QMimeType::setGlobPatterns(const QList<MimeGlobPattern> &g)
{
    m_d->globPatterns = g;

    QString oldPrefferedSuffix = m_d->preferredSuffix;
    m_d->suffixes.clear();
    m_d->preferredSuffix.clear();
    m_d->assignSuffixes(QMimeDatabase::fromGlobPatterns(g));
    if (m_d->preferredSuffix != oldPrefferedSuffix && m_d->suffixes.contains(oldPrefferedSuffix))
        m_d->preferredSuffix = oldPrefferedSuffix;
}

QStringList QMimeType::subClassesOf() const
{
    return m_d->subClassesOf;
}

void QMimeType::setSubClassesOf(const QStringList &s)
{
    m_d->subClassesOf = s;
}

QString QMimeType::preferredSuffix() const
{
    return m_d->preferredSuffix;
}

bool QMimeType::setPreferredSuffix(const QString &s)
{
    if (!m_d->suffixes.contains(s)) {
        qWarning("%s: Attempt to set preferred suffix to '%s', which is not in the list of suffixes: %s.",
                 m_d->type.toUtf8().constData(),
                 s.toUtf8().constData(),
                 m_d->suffixes.join(QLatin1String(",")).toUtf8().constData());
        return false;
    }
    m_d->preferredSuffix = s;
    return true;
}

QString QMimeType::formatFilterString(const QString &description, const QList<MimeGlobPattern> &globs)
{
    QString rc;
    if (globs.empty())  // Binary files
        return rc;
    {
        QTextStream str(&rc);
        str << description;
        if (!globs.empty()) {
            str << " (";
            const int size = globs.size();
            for (int i = 0; i < size; i++) {
                if (i)
                    str << ' ';
                str << globs.at(i).regExp().pattern();
            }
            str << ')';
        }
    }
    return rc;
}

QString QMimeType::filterString() const
{
    // @todo: Use localeComment() once creator is shipped with translations
    return formatFilterString(comment(), m_d->globPatterns);
}

bool QMimeType::matchesType(const QString &type) const
{
    return m_d->type == type || m_d->aliases.contains(type);
}

unsigned QMimeType::matchesFile(const QFileInfo &file) const
{
    FileMatchContext context(file);
    const unsigned suffixPriority = m_d->matchesFileBySuffix(context);
    if (suffixPriority >= MimeGlobPattern::MaxWeight)
        return suffixPriority;
    return qMax(suffixPriority, m_d->matchesFileByContent(context));
}

unsigned QMimeTypeData::matchesFileBySuffix(FileMatchContext &c) const
{
    // check globs
    foreach (const MimeGlobPattern &gp, /*m_d->*/globPatterns) {
        if (gp.regExp().exactMatch(c.fileName()))
            return gp.weight();
    }
    return 0;
}

unsigned QMimeTypeData::matchesFileBySuffix(const QString &name) const
{
    // check globs
    foreach (const MimeGlobPattern &gp, globPatterns) {
        if (gp.regExp().exactMatch(name))
            return gp.weight();
    }
    return 0;
}

unsigned QMimeTypeData::matchesFileByContent(FileMatchContext &c) const
{
    // Nope, try magic matchers on context data
    if (/*m_d->*/magicMatchers.isEmpty())
        return 0;

    return matchesData(c.data());
}

unsigned QMimeTypeData::matchesData(const QByteArray &data) const
{
    unsigned priority = 0;
    if (!data.isEmpty()) {
        foreach (const IMagicMatcher::IMagicMatcherSharedPointer &matcher, /*m_d->*/magicMatchers) {
            const unsigned magicPriority = matcher->priority();
            if (magicPriority > priority && matcher->matches(data))
                priority = magicPriority;
        }
    }
    return priority;
}

QStringList QMimeType::suffixes() const
{
    return m_d->suffixes;
}

void QMimeType::addMagicMatcher(const IMagicMatcherSharedPointer &matcher)
{
    m_d->magicMatchers.push_back(matcher);
}

const QMimeType::IMagicMatcherList &QMimeType::magicMatchers() const
{
    return m_d->magicMatchers;
}

void QMimeType::setMagicMatchers(const IMagicMatcherList &matchers)
{
    m_d->magicMatchers = matchers;
}

namespace {
struct RemovePred : std::unary_function<QMimeType::IMagicMatcherSharedPointer, bool>
{
    RemovePred(bool keepRuleBased) : m_keepRuleBase(keepRuleBased) {}
    bool m_keepRuleBase;

    bool operator()(const QMimeType::IMagicMatcherSharedPointer &matcher) {
        if ((m_keepRuleBase && !dynamic_cast<MagicRuleMatcher *>(matcher.data()))
                || (!m_keepRuleBase && dynamic_cast<MagicRuleMatcher *>(matcher.data())))
            return true;
        return false;
    }
};
} // Anonymous

QMimeType::IMagicMatcherList QMimeType::magicRuleMatchers() const
{
    IMagicMatcherList ruleMatchers = m_d->magicMatchers;
    ruleMatchers.erase(std::remove_if(ruleMatchers.begin(), ruleMatchers.end(), RemovePred(true)),
                       ruleMatchers.end());
    return ruleMatchers;
}

void QMimeType::setMagicRuleMatchers(const IMagicMatcherList &matchers)
{
    m_d->magicMatchers.erase(std::remove_if(m_d->magicMatchers.begin(), m_d->magicMatchers.end(),
                                            RemovePred(false)),
                             m_d->magicMatchers.end());
    m_d->magicMatchers.append(matchers);
}

QDebug operator<<(QDebug d, const QMimeType &mt)
{
    QString s;
    {
        QTextStream str(&s);
        mt.m_d->debug(str);
    }
    d << s;
    return d;
}

QT_END_NAMESPACE

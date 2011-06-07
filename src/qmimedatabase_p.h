#ifndef QMIMEDATABASE_P_H
#define QMIMEDATABASE_P_H

#include "qmimedatabase.h"

#include <QtCore/QMultiHash>
#include <QtCore/QMutex>
#ifndef QT_NO_DEBUG_STREAM
#include <QtCore/QTextStream>
#endif

QT_BEGIN_NAMESPACE

#define MIN_MATCH_WEIGHT 50

// MimeMapEntry: Entry of a type map, consisting of type and level.
struct MimeMapEntry
{
    static const int Dangling = 32767;

    inline MimeMapEntry(const QMimeType &aType = QMimeType(), int aLevel = Dangling) :
        type(aType), level(aLevel)
    {}

    QMimeType type;
    int level; // hierachy level
};

class QMimeDatabasePrivate
{
    Q_DISABLE_COPY(QMimeDatabasePrivate)
    friend class QMimeDatabase;
public:
    QMimeDatabasePrivate();

    bool addMimeTypes(const QString &fileName, QString *errorMessage);
    bool addMimeTypes(QIODevice *device, QString *errorMessage);
    bool addMimeType(QMimeType mt);

    QStringList filterStrings() const;

    QStringList suffixes() const;
    bool setPreferredSuffix(const QString &typeOrAlias, const QString &suffix);

    QList<QMimeGlobPattern> globPatterns() const;
    void setGlobPatterns(const QString &typeOrAlias, const QList<QMimeGlobPattern> &globPatterns);

    QList<QMimeMagicRuleMatcher> magicMatchers() const;
    void setMagicMatchers(const QString &typeOrAlias,
                          const QList<QMimeMagicRuleMatcher> &matchers);

    QList<QMimeType> mimeTypes() const;

    void syncUserModifiedMimeTypes();
    static QList<QMimeType> readUserModifiedMimeTypes();
    static void writeUserModifiedMimeTypes(const QList<QMimeType> &mimeTypes);
    void clearUserModifiedMimeTypes();

    static QList<QMimeGlobPattern> toGlobPatterns(const QStringList &patterns,
                                                  int weight = QMimeGlobPattern::DefaultWeight);
    static QStringList fromGlobPatterns(const QList<QMimeGlobPattern> &globPatterns);

    static bool isTextFile(const QByteArray &data);

#ifndef QT_NO_DEBUG_STREAM
    void debug(QTextStream &str) const;
#endif

private:
    typedef QHash<QString, QString> AliasMap;
    typedef QMultiHash<QString, QString> ParentChildrenMap;

    static const QString kModifiedMimeTypesFile;
    static QString kModifiedMimeTypesPath;


    bool addMimeTypes(QIODevice *device, const QString &fileName, QString *errorMessage);
    inline QString resolveAlias(const QString &name) const
    { return m_aliasMap.value(name, name); }

    QMimeType findByType(const QString &type) const;
    QMimeType findByFile(const QFileInfo &f, unsigned *priorityPtr) const;
    QMimeType findByData(const QByteArray &data, unsigned *priorityPtr) const;
    QMimeType findByName(const QString &name, unsigned *priorityPtr) const;

    void determineLevels();
    void raiseLevelRecursion(MimeMapEntry &e, int level);

    QHash<QString, MimeMapEntry*> m_typeMimeTypeMap;
    AliasMap m_aliasMap;
    ParentChildrenMap m_parentChildrenMap;
    int m_maxLevel;
    QMutex m_mutex;
};

class BaseMimeTypeParser {
    Q_DISABLE_COPY(BaseMimeTypeParser)
public:
    BaseMimeTypeParser() {}
    virtual ~BaseMimeTypeParser() {}

    bool parse(QIODevice *dev, const QString &fileName, QString *errorMessage);

private:
    // Overwrite to process the sequence of parsed data
    virtual bool process(const QMimeType &t, QString *errorMessage) = 0;

    enum ParseStage { ParseBeginning,
                      ParseMimeInfo,
                      ParseMimeType,
                      ParseComment,
                      ParseGlobPattern,
                      ParseSubClass,
                      ParseAlias,
                      ParseMagic,
                      ParseMagicMatchRule,
                      ParseOtherMimeTypeSubTag,
                      ParseError };

    static ParseStage nextStage(ParseStage currentStage, const QStringRef &startElement);
};

// Parser that builds MimeDB hierarchy by adding to MimeDatabasePrivate
class MimeTypeParser : public BaseMimeTypeParser {
public:
    explicit MimeTypeParser(QMimeDatabasePrivate &db) : m_db(db) {}
private:
    virtual bool process(const QMimeType &t, QString *) { m_db.addMimeType(t); return true; }

    QMimeDatabasePrivate &m_db;
};

QT_END_NAMESPACE

#endif // QMIMEDATABASE_P_H

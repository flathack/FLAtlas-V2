#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::infrastructure {

enum class IdsUsageType {
    Unknown,
    IdsName,
    IdsInfo,
    StridName,
};

enum class IdsIssueSeverity {
    Info,
    Warning,
    Error,
};

struct IdsEntryRecord {
    int globalId = 0;
    int localId = 0;
    int dllSlot = 0;
    QString dllName;
    QString dllPath;
    bool hasStringValue = false;
    bool hasHtmlValue = false;
    QString stringValue;
    QString htmlValue;
    QString plainText;
    QStringList usageTypes;
    bool editable = false;
    QString searchBlob;
};

struct IdsReferenceRecord {
    int globalId = 0;
    IdsUsageType usageType = IdsUsageType::Unknown;
    QString filePath;
    QString relativePath;
    QString sectionName;
    int sectionIndex = -1;
    QString keyName;
    QString nickname;
    QString archetype;
    int lineNumber = 0;
    QString lineText;
};

struct IdsMissingAssignmentRecord {
    IdsUsageType usageType = IdsUsageType::Unknown;
    QString filePath;
    QString relativePath;
    QString sectionName;
    int sectionIndex = -1;
    QString nickname;
    QString archetype;
    int lineNumber = 0;
    int currentValue = 0;
    int siblingValue = 0;
};

struct IdsIssueRecord {
    IdsIssueSeverity severity = IdsIssueSeverity::Info;
    QString code;
    QString message;
    int globalId = 0;
    QString dllName;
    QString filePath;
};

struct IdsDataset {
    QString gameRoot;
    QString exeDir;
    QString dataDir;
    QString freelancerIniPath;
    QStringList resourceDlls;
    QVector<IdsEntryRecord> entries;
    QVector<IdsReferenceRecord> references;
    QVector<IdsMissingAssignmentRecord> missingAssignments;
    QVector<IdsIssueRecord> issues;
};

class IdsDataService {
public:
    static IdsDataset loadFromGameRoot(const QString &gameRoot);
    static IdsDataset loadFromDllPath(const QString &dllPath);

    static bool writeStringEntry(const IdsDataset &dataset,
                                 const QString &dllName,
                                 int currentGlobalId,
                                 const QString &text,
                                 int *outGlobalId,
                                 QString *errorMessage);

    static bool writeInfocardEntry(const IdsDataset &dataset,
                                   const QString &dllName,
                                   int currentGlobalId,
                                   const QString &xmlText,
                                   int *outGlobalId,
                                   QString *errorMessage);

    static bool assignFieldValue(const QString &filePath,
                                 int sectionIndex,
                                 const QString &fieldName,
                                 int value,
                                 QString *errorMessage);

    static QString usageTypeLabel(IdsUsageType usageType);
    static QString usageTypeKey(IdsUsageType usageType);
    static QString normalizedEntryTypeKey(const IdsEntryRecord &entry);
    static QString prettyInfocardXml(const QString &xmlText, QString *errorMessage);
};

} // namespace flatlas::infrastructure
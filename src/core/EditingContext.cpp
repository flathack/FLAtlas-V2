#include "EditingContext.h"
#include "Config.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUuid>

namespace flatlas::core {

static QString normalizeComparablePath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty())
        return {};

    path = QDir::fromNativeSeparators(path);
    path = QDir::cleanPath(path);

#ifdef Q_OS_WIN
    path = path.toLower();
#endif
    return path;
}

static QSet<QString> profilePathVariants(const ModProfile &profile)
{
    QSet<QString> variants;
    const QString source = profile.sourcePath().trimmed();
    if (source.isEmpty())
        return variants;

    variants.insert(normalizeComparablePath(source));

    const QFileInfo rawInfo(source);
    variants.insert(normalizeComparablePath(rawInfo.absoluteFilePath()));

    const QString cleaned = QDir::cleanPath(source);
    variants.insert(normalizeComparablePath(cleaned));

    const QFileInfo cleanedInfo(cleaned);
    variants.insert(normalizeComparablePath(cleanedInfo.absoluteFilePath()));

    if (rawInfo.exists()) {
        const QString canonical = rawInfo.canonicalFilePath();
        if (!canonical.isEmpty())
            variants.insert(normalizeComparablePath(canonical));
    }
    if (cleanedInfo.exists()) {
        const QString canonical = cleanedInfo.canonicalFilePath();
        if (!canonical.isEmpty())
            variants.insert(normalizeComparablePath(canonical));
    }

    variants.remove(QString());
    return variants;
}

static bool profilesReferToSameLocation(const ModProfile &lhs, const ModProfile &rhs)
{
    const QSet<QString> lhsVariants = profilePathVariants(lhs);
    const QSet<QString> rhsVariants = profilePathVariants(rhs);
    if (lhsVariants.isEmpty() || rhsVariants.isEmpty())
        return false;

    for (const auto &candidate : lhsVariants) {
        if (rhsVariants.contains(candidate))
            return true;
    }
    return false;
}

// ── ModProfile ───────────────────────────────────────────────────────

QString ModProfile::sourcePath() const
{
    if (mode == QStringLiteral("direct"))
        return directPath;
    if (mode == QStringLiteral("repo") && !repoRoot.isEmpty() && !repoFolder.isEmpty())
        return QDir(repoRoot).filePath(repoFolder);
    return {};
}

QJsonObject ModProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("mode")] = mode;
    obj[QStringLiteral("direct_path")] = directPath;
    obj[QStringLiteral("repo_root")] = repoRoot;
    obj[QStringLiteral("repo_folder")] = repoFolder;
    return obj;
}

ModProfile ModProfile::fromJson(const QJsonObject &obj)
{
    ModProfile p;
    p.id = obj.value(QStringLiteral("id")).toString();
    p.name = obj.value(QStringLiteral("name")).toString();
    p.mode = obj.value(QStringLiteral("mode")).toString();
    p.directPath = obj.value(QStringLiteral("direct_path")).toString();
    p.repoRoot = obj.value(QStringLiteral("repo_root")).toString();
    p.repoFolder = obj.value(QStringLiteral("repo_folder")).toString();
    return p;
}

// ── EditingContext ───────────────────────────────────────────────────

EditingContext &EditingContext::instance()
{
    static EditingContext ctx;
    return ctx;
}

void EditingContext::restore()
{
    auto &cfg = Config::instance();
    bool changed = false;

    // Restore profiles
    m_profiles.clear();
    const auto arr = cfg.getJsonArray(QStringLiteral("modmanager.profiles"));
    QVector<ModProfile> uniqueProfiles;
    for (const auto &v : arr) {
        auto p = ModProfile::fromJson(v.toObject());
        if (!p.isValid())
            continue;

        bool duplicate = false;
        for (const auto &existing : uniqueProfiles) {
            if (profilesReferToSameLocation(existing, p)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
                changed = true;
                continue;
        }

        uniqueProfiles.append(p);
    }
    m_profiles = uniqueProfiles;

    // Restore active editing ID
    m_editingId = cfg.getString(QStringLiteral("modmanager.editing_id"));

    // Validate that the saved editing ID still exists
    if (!m_editingId.isEmpty()) {
        bool found = false;
        for (const auto &p : m_profiles) {
            if (p.id == m_editingId) { found = true; break; }
        }
        if (!found) {
            m_editingId.clear();
            changed = true;
        }
    }

    if (changed)
        persist();

    emit profilesChanged();
    emit contextChanged(m_editingId);
}

void EditingContext::persist() const
{
    auto &cfg = Config::instance();

    QJsonArray arr;
    for (const auto &p : m_profiles)
        arr.append(p.toJson());
    cfg.setJsonArray(QStringLiteral("modmanager.profiles"), arr);
    cfg.setString(QStringLiteral("modmanager.editing_id"), m_editingId);
    cfg.save();
}

bool EditingContext::addProfile(const ModProfile &profile)
{
    ModProfile p = profile;
    if (p.id.isEmpty())
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);

    for (const auto &existing : m_profiles) {
        if (profilesReferToSameLocation(existing, p))
            return false;
    }

    m_profiles.append(p);
    persist();
    emit profilesChanged();
    return true;
}

void EditingContext::removeProfile(const QString &profileId)
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == profileId) {
            m_profiles.removeAt(i);
            if (m_editingId == profileId) {
                m_editingId.clear();
                persist();
                emit profilesChanged();
                emit contextChanged({});
                return;
            }
            persist();
            emit profilesChanged();
            return;
        }
    }
}

ModProfile EditingContext::profileById(const QString &id) const
{
    for (const auto &p : m_profiles) {
        if (p.id == id)
            return p;
    }
    return {};
}

ModProfile EditingContext::editingProfile() const
{
    return profileById(m_editingId);
}

QString EditingContext::primaryGamePath() const
{
    auto profile = editingProfile();
    if (!profile.isValid())
        return {};
    QString path = profile.sourcePath();
    if (path.isEmpty() || !QDir(path).exists())
        return {};
    return path;
}

bool EditingContext::setEditingProfile(const QString &profileId)
{
    auto profile = profileById(profileId);
    if (!profile.isValid())
        return false;

    QString path = profile.sourcePath();
    if (path.isEmpty() || !QDir(path).exists())
        return false;

    m_editingId = profileId;
    persist();
    emit contextChanged(profileId);
    return true;
}

void EditingContext::clearEditingProfile()
{
    if (m_editingId.isEmpty())
        return;
    m_editingId.clear();
    persist();
    emit contextChanged({});
}

} // namespace flatlas::core

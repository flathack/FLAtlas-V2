#include "EditingContext.h"
#include "Config.h"

#include <QDir>
#include <QUuid>

namespace flatlas::core {

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

    // Restore profiles
    m_profiles.clear();
    const auto arr = cfg.getJsonArray(QStringLiteral("modmanager.profiles"));
    for (const auto &v : arr) {
        auto p = ModProfile::fromJson(v.toObject());
        if (p.isValid())
            m_profiles.append(p);
    }

    // Restore active editing ID
    m_editingId = cfg.getString(QStringLiteral("modmanager.editing_id"));

    // Validate that the saved editing ID still exists
    if (!m_editingId.isEmpty()) {
        bool found = false;
        for (const auto &p : m_profiles) {
            if (p.id == m_editingId) { found = true; break; }
        }
        if (!found)
            m_editingId.clear();
    }
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

void EditingContext::addProfile(const ModProfile &profile)
{
    ModProfile p = profile;
    if (p.id.isEmpty())
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    m_profiles.append(p);
    persist();
    emit profilesChanged();
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

#pragma once
// core/EditingContext.h – Zentraler Editing-Kontext (welche Installation/Mod bearbeitet wird)

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace flatlas::core {

/// A managed Freelancer installation or mod profile.
struct ModProfile {
    QString id;            ///< Unique identifier (UUID-based)
    QString name;          ///< Display name
    QString mode;          ///< "direct" (standalone FL installation) or "repo" (mod in repository)
    QString directPath;    ///< For mode=="direct": path to the FL installation
    QString repoRoot;      ///< For mode=="repo": repository root path
    QString repoFolder;    ///< For mode=="repo": subfolder name within repo

    /// Resolves the source path of this profile's game data.
    QString sourcePath() const;

    QJsonObject toJson() const;
    static ModProfile fromJson(const QJsonObject &obj);
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

/// Singleton that manages the active editing context.
/// All editors query this to know which game data to load.
class EditingContext : public QObject {
    Q_OBJECT
public:
    static EditingContext &instance();

    /// Load profiles and active context from Config.
    void restore();
    /// Persist profiles and active context to Config.
    void persist() const;

    // ── Profile management ──────────────────────────────────
    const QVector<ModProfile> &profiles() const { return m_profiles; }
    bool addProfile(const ModProfile &profile);
    void removeProfile(const QString &profileId);
    ModProfile profileById(const QString &id) const;

    // ── Editing context ─────────────────────────────────────
    /// The currently-editing profile ID (empty if none).
    QString editingProfileId() const { return m_editingId; }
    /// The currently-editing profile (null-like if none).
    ModProfile editingProfile() const;
    /// The resolved game path for the current editing context.
    QString primaryGamePath() const;
    /// Whether an editing context is active.
    bool hasContext() const { return !m_editingId.isEmpty(); }

    /// Set the editing context to a profile. Closes old context.
    bool setEditingProfile(const QString &profileId);
    /// Clear the editing context.
    void clearEditingProfile();

signals:
    /// Emitted when the editing context changes (new profile or cleared).
    void contextChanged(const QString &profileId);
    /// Emitted when the profile list changes (add/remove).
    void profilesChanged();

private:
    EditingContext() = default;
    QVector<ModProfile> m_profiles;
    QString m_editingId;
};

} // namespace flatlas::core

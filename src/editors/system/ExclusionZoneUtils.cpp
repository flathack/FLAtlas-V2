#include "ExclusionZoneUtils.h"

#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace flatlas::editors {

QString ExclusionZoneUtils::normalizeName(const QString &value)
{
    QString cleaned = value.trimmed();
    cleaned.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    cleaned.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    cleaned.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return cleaned.isEmpty() ? QStringLiteral("zone") : cleaned;
}

QString ExclusionZoneUtils::generateExclusionNickname(const QString &systemNickname,
                                                      const QString &fieldZoneNickname,
                                                      const QStringList &existingZoneNicknames)
{
    const QString sysPart = normalizeName(systemNickname);
    QString fieldPartRaw = normalizeName(fieldZoneNickname);
    QString fieldLower = fieldPartRaw.toLower();

    if (fieldLower.startsWith(QStringLiteral("zone_"))) {
        fieldPartRaw = fieldPartRaw.mid(5);
        fieldLower = fieldPartRaw.toLower();
    }

    const QString sysPrefix = sysPart.toLower() + QLatin1Char('_');
    if (fieldLower.startsWith(sysPrefix))
        fieldPartRaw = fieldPartRaw.mid(sysPrefix.size());

    const QString fieldPart = normalizeName(fieldPartRaw);
    const QString base = QStringLiteral("Zone_%1_%2_exclusion").arg(sysPart, fieldPart);
    QSet<QString> existingLower;
    for (const QString &nickname : existingZoneNicknames)
        existingLower.insert(nickname.trimmed().toLower());

    for (int index = 1;; ++index) {
        const QString candidate = QStringLiteral("%1_%2").arg(base).arg(index);
        if (!existingLower.contains(candidate.toLower()))
            return candidate;
    }
}

QVector<QPair<QString, QString>> ExclusionZoneUtils::buildZoneEntries(const QString &nickname,
                                                                      const QString &shape,
                                                                      const QVector3D &position,
                                                                      const QVector3D &size,
                                                                      const QVector3D &rotation,
                                                                      const QString &comment,
                                                                      int sort)
{
    QString shapeUpper = shape.trimmed().toUpper();
    if (shapeUpper != QStringLiteral("SPHERE")
        && shapeUpper != QStringLiteral("ELLIPSOID")
        && shapeUpper != QStringLiteral("BOX")
        && shapeUpper != QStringLiteral("CYLINDER")) {
        shapeUpper = QStringLiteral("SPHERE");
    }

    QVector<QPair<QString, QString>> entries;
    entries.append({QStringLiteral("nickname"), nickname});
    entries.append({QStringLiteral("pos"),
                    QStringLiteral("%1, %2, %3")
                        .arg(position.x(), 0, 'f', 0)
                        .arg(position.y(), 0, 'f', 0)
                        .arg(position.z(), 0, 'f', 0)});
    entries.append({QStringLiteral("rotate"),
                    QStringLiteral("%1, %2, %3")
                        .arg(rotation.x(), 0, 'f', 0)
                        .arg(rotation.y(), 0, 'f', 0)
                        .arg(rotation.z(), 0, 'f', 0)});
    entries.append({QStringLiteral("shape"), shapeUpper});
    entries.append({QStringLiteral("size"),
                    QStringLiteral("%1, %2, %3")
                        .arg(size.x(), 0, 'f', 0)
                        .arg(size.y(), 0, 'f', 0)
                        .arg(size.z(), 0, 'f', 0)});
    entries.append({QStringLiteral("property_flags"), QStringLiteral("131072")});
    entries.append({QStringLiteral("sort"), QString::number(sort)});
    if (!comment.trimmed().isEmpty())
        entries.append({QStringLiteral("comment"), comment.trimmed()});
    return entries;
}

QStringList ExclusionZoneUtils::nebulaShellOptionsForGamePath(const QString &gamePath)
{
    QString dataDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
    QString shellDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/NEBULA"));
    if (shellDir.isEmpty()) {
        return {
            QStringLiteral("solar\\nebula\\generic_exclusion.3db"),
            QStringLiteral("solar\\nebula\\crow_exclusion.3db"),
            QStringLiteral("solar\\nebula\\walker_exclusion.3db"),
            QStringLiteral("solar\\nebula\\generic02_exclusion.3db"),
            QStringLiteral("solar\\nebula\\plain_inner_sphere.3db"),
            QStringLiteral("solar\\nebula\\exclu_generic_sphere.3db"),
        };
    }

    QDir dir(shellDir);
    QStringList values;
    for (const QFileInfo &fileInfo : dir.entryInfoList({QStringLiteral("*exclusion*.3db")}, QDir::Files, QDir::Name))
        values.append(QStringLiteral("solar\\nebula\\%1").arg(fileInfo.fileName()));
    for (const QFileInfo &fileInfo : dir.entryInfoList({QStringLiteral("plain_inner_sphere.3db")}, QDir::Files, QDir::Name)) {
        const QString relative = QStringLiteral("solar\\nebula\\%1").arg(fileInfo.fileName());
        if (!values.contains(relative, Qt::CaseInsensitive))
            values.append(relative);
    }
    if (values.isEmpty())
        values.append(QStringLiteral("solar\\nebula\\generic_exclusion.3db"));
    return values;
}

QPair<QString, bool> ExclusionZoneUtils::patchFieldIniExclusionSection(const QString &iniText,
                                                                       const QString &exclusionZoneNickname,
                                                                       const ExclusionShellSettings &shellSettings)
{
    const QString target = exclusionZoneNickname.trimmed();
    if (target.isEmpty())
        return {iniText, false};

    QStringList optionLines;
    if (shellSettings.enabled) {
        if (shellSettings.fogFar > 0)
            optionLines.append(QStringLiteral("fog_far = %1").arg(shellSettings.fogFar));
        if (!shellSettings.zoneShell.trimmed().isEmpty())
            optionLines.append(QStringLiteral("zone_shell = %1").arg(shellSettings.zoneShell.trimmed()));
        optionLines.append(QStringLiteral("shell_scalar = %1")
                               .arg(QString::number(shellSettings.shellScalar, 'f', 2).remove(QRegularExpression(QStringLiteral("0+$"))).remove(QRegularExpression(QStringLiteral("\\.$")))));
        optionLines.append(QStringLiteral("max_alpha = %1")
                               .arg(QString::number(shellSettings.maxAlpha, 'f', 2).remove(QRegularExpression(QStringLiteral("0+$"))).remove(QRegularExpression(QStringLiteral("\\.$")))));
        if (!shellSettings.exclusionTint.trimmed().isEmpty())
            optionLines.append(QStringLiteral("exclusion_tint = %1").arg(shellSettings.exclusionTint.trimmed()));
    }

    QStringList lines = iniText.split(QLatin1Char('\n'));
    QVector<QPair<int, QString>> headers;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines[index].trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']')) && trimmed.size() > 2)
            headers.append({index, trimmed.mid(1, trimmed.size() - 2).trimmed().toLower()});
    }

    int exclusionStart = -1;
    int exclusionEnd = lines.size();
    for (int index = 0; index < headers.size(); ++index) {
        if (headers[index].second == QStringLiteral("exclusion zones")) {
            exclusionStart = headers[index].first;
            exclusionEnd = (index + 1 < headers.size()) ? headers[index + 1].first : lines.size();
            break;
        }
    }

    bool changed = false;
    if (exclusionStart < 0) {
        int insertAt = lines.size();
        int propertiesEnd = -1;
        for (int index = 0; index < headers.size(); ++index) {
            if (headers[index].second == QStringLiteral("properties")) {
                propertiesEnd = (index + 1 < headers.size()) ? headers[index + 1].first : lines.size();
                break;
            }
        }
        if (propertiesEnd >= 0) {
            insertAt = propertiesEnd;
        } else {
            for (const auto &header : headers) {
                if (header.second == QStringLiteral("fog")
                    || header.second == QStringLiteral("dynamiclightning")
                    || header.second == QStringLiteral("backgroundlight")) {
                    insertAt = header.first;
                    break;
                }
            }
        }

        QStringList block;
        if (insertAt > 0 && !lines.value(insertAt - 1).trimmed().isEmpty())
            block.append(QString());
        block.append(QStringLiteral("[Exclusion Zones]"));
        block.append(QStringLiteral("exclusion = %1").arg(target));
        block.append(optionLines);
        if (insertAt < lines.size() && !lines.value(insertAt).trimmed().isEmpty())
            block.append(QString());
        for (int blockIndex = 0; blockIndex < block.size(); ++blockIndex)
            lines.insert(insertAt + blockIndex, block[blockIndex]);
        QString out = lines.join(QLatin1Char('\n'));
        if (iniText.endsWith(QLatin1Char('\n')) && !out.endsWith(QLatin1Char('\n')))
            out.append(QLatin1Char('\n'));
        return {out, true};
    }

    int entryLine = -1;
    int lastExclusionLine = -1;
    for (int index = exclusionStart + 1; index < exclusionEnd; ++index) {
        const QString raw = lines[index].trimmed();
        if (raw.isEmpty() || raw.startsWith(QLatin1Char(';')) || raw.startsWith(QStringLiteral("//")) || !raw.contains(QLatin1Char('=')))
            continue;
        const QString key = raw.section(QLatin1Char('='), 0, 0).trimmed().toLower();
        const QString value = raw.section(QLatin1Char('='), 1).trimmed();
        if (key == QStringLiteral("exclusion")) {
            lastExclusionLine = index;
            if (value.compare(target, Qt::CaseInsensitive) == 0) {
                entryLine = index;
                break;
            }
        }
    }

    if (entryLine < 0) {
        const int insertAt = (lastExclusionLine >= 0) ? lastExclusionLine + 1 : exclusionStart + 1;
        lines.insert(insertAt, QStringLiteral("exclusion = %1").arg(target));
        for (int optionIndex = 0; optionIndex < optionLines.size(); ++optionIndex)
            lines.insert(insertAt + optionIndex + 1, optionLines[optionIndex]);
        changed = true;
    } else {
        int blockEnd = entryLine + 1;
        while (blockEnd < exclusionEnd) {
            const QString raw = lines[blockEnd].trimmed();
            if (raw.isEmpty() || raw.startsWith(QLatin1Char(';')) || raw.startsWith(QStringLiteral("//"))) {
                ++blockEnd;
                continue;
            }
            if (!raw.contains(QLatin1Char('=')))
                break;
            const QString key = raw.section(QLatin1Char('='), 0, 0).trimmed().toLower();
            if (key == QStringLiteral("exclusion"))
                break;
            ++blockEnd;
        }
        const QStringList oldOptionLines = lines.mid(entryLine + 1, blockEnd - entryLine - 1);
        if (oldOptionLines != optionLines) {
            for (int index = blockEnd - 1; index >= entryLine + 1; --index)
                lines.removeAt(index);
            for (int optionIndex = 0; optionIndex < optionLines.size(); ++optionIndex)
                lines.insert(entryLine + optionIndex + 1, optionLines[optionIndex]);
            changed = true;
        }
    }

    QString out = lines.join(QLatin1Char('\n'));
    if (iniText.endsWith(QLatin1Char('\n')) && !out.endsWith(QLatin1Char('\n')))
        out.append(QLatin1Char('\n'));
    return {out, changed};
}

QPair<QString, bool> ExclusionZoneUtils::patchFieldIniRemoveExclusion(const QString &iniText,
                                                                      const QString &exclusionZoneNickname)
{
    const QString target = exclusionZoneNickname.trimmed().toLower();
    if (target.isEmpty())
        return {iniText, false};

    QStringList lines = iniText.split(QLatin1Char('\n'));
    QVector<QPair<int, QString>> headers;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines[index].trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']')) && trimmed.size() > 2)
            headers.append({index, trimmed.mid(1, trimmed.size() - 2).trimmed().toLower()});
    }

    int exclusionStart = -1;
    int exclusionEnd = lines.size();
    for (int index = 0; index < headers.size(); ++index) {
        if (headers[index].second == QStringLiteral("exclusion zones")) {
            exclusionStart = headers[index].first;
            exclusionEnd = (index + 1 < headers.size()) ? headers[index + 1].first : lines.size();
            break;
        }
    }
    if (exclusionStart < 0)
        return {iniText, false};

    QVector<int> removeIndices;
    for (int index = exclusionStart + 1; index < exclusionEnd; ++index) {
        const QString raw = lines[index].trimmed();
        if (raw.isEmpty() || raw.startsWith(QLatin1Char(';')) || raw.startsWith(QStringLiteral("//")) || !raw.contains(QLatin1Char('=')))
            continue;
        const QString key = raw.section(QLatin1Char('='), 0, 0).trimmed().toLower();
        const QString value = raw.section(QLatin1Char('='), 1).trimmed().toLower();
        if (key == QStringLiteral("exclusion") && value == target) {
            removeIndices.append(index);
            int follow = index + 1;
            while (follow < exclusionEnd) {
                const QString followRaw = lines[follow].trimmed();
                if (followRaw.isEmpty() || followRaw.startsWith(QLatin1Char(';')) || followRaw.startsWith(QStringLiteral("//"))) {
                    ++follow;
                    continue;
                }
                if (!followRaw.contains(QLatin1Char('=')))
                    break;
                const QString followKey = followRaw.section(QLatin1Char('='), 0, 0).trimmed().toLower();
                if (followKey == QStringLiteral("exclusion"))
                    break;
                removeIndices.append(follow);
                ++follow;
            }
        }
    }

    if (removeIndices.isEmpty())
        return {iniText, false};

    std::sort(removeIndices.begin(), removeIndices.end(), std::greater<>());
    for (int index : std::as_const(removeIndices))
        lines.removeAt(index);

    bool hasRemainingExclusion = false;
    for (int index = exclusionStart + 1; index < qMin(exclusionEnd, lines.size()); ++index) {
        const QString raw = lines[index].trimmed();
        if (raw.isEmpty() || raw.startsWith(QLatin1Char(';')) || raw.startsWith(QStringLiteral("//")) || !raw.contains(QLatin1Char('=')))
            continue;
        if (raw.section(QLatin1Char('='), 0, 0).trimmed().compare(QStringLiteral("exclusion"), Qt::CaseInsensitive) == 0) {
            hasRemainingExclusion = true;
            break;
        }
    }

    if (!hasRemainingExclusion) {
        headers.clear();
        for (int index = 0; index < lines.size(); ++index) {
            const QString trimmed = lines[index].trimmed();
            if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']')) && trimmed.size() > 2)
                headers.append({index, trimmed.mid(1, trimmed.size() - 2).trimmed().toLower()});
        }
        for (int index = 0; index < headers.size(); ++index) {
            if (headers[index].second == QStringLiteral("exclusion zones")) {
                int start = headers[index].first;
                const int end = (index + 1 < headers.size()) ? headers[index + 1].first : lines.size();
                if (start > 0 && lines[start - 1].trimmed().isEmpty())
                    --start;
                for (int removeIndex = end - 1; removeIndex >= start; --removeIndex)
                    lines.removeAt(removeIndex);
                break;
            }
        }
    }

    QString out = lines.join(QLatin1Char('\n'));
    if (iniText.endsWith(QLatin1Char('\n')) && !out.endsWith(QLatin1Char('\n')))
        out.append(QLatin1Char('\n'));
    return {out, true};
}

ExclusionShellSettings ExclusionZoneUtils::readFieldIniExclusionSettings(const QString &iniText,
                                                                         const QString &exclusionZoneNickname,
                                                                         bool *found)
{
    if (found)
        *found = false;

    const QString target = exclusionZoneNickname.trimmed().toLower();
    if (target.isEmpty())
        return {};

    QStringList lines = iniText.split(QLatin1Char('\n'));
    QVector<QPair<int, QString>> headers;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines[index].trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']')) && trimmed.size() > 2)
            headers.append({index, trimmed.mid(1, trimmed.size() - 2).trimmed().toLower()});
    }

    int exclusionStart = -1;
    int exclusionEnd = lines.size();
    for (int index = 0; index < headers.size(); ++index) {
        if (headers[index].second == QStringLiteral("exclusion zones")) {
            exclusionStart = headers[index].first;
            exclusionEnd = (index + 1 < headers.size()) ? headers[index + 1].first : lines.size();
            break;
        }
    }
    if (exclusionStart < 0)
        return {};

    for (int index = exclusionStart + 1; index < exclusionEnd; ++index) {
        const QString raw = lines[index].trimmed();
        if (raw.isEmpty() || raw.startsWith(QLatin1Char(';')) || raw.startsWith(QStringLiteral("//")) || !raw.contains(QLatin1Char('=')))
            continue;
        const QString key = raw.section(QLatin1Char('='), 0, 0).trimmed().toLower();
        const QString value = raw.section(QLatin1Char('='), 1).trimmed();
        if (key != QStringLiteral("exclusion") || value.compare(target, Qt::CaseInsensitive) != 0)
            continue;

        ExclusionShellSettings settings;
        if (found)
            *found = true;
        for (int follow = index + 1; follow < exclusionEnd; ++follow) {
            const QString followRaw = lines[follow].trimmed();
            if (followRaw.isEmpty() || followRaw.startsWith(QLatin1Char(';')) || followRaw.startsWith(QStringLiteral("//")))
                continue;
            if (!followRaw.contains(QLatin1Char('=')))
                break;
            const QString followKey = followRaw.section(QLatin1Char('='), 0, 0).trimmed().toLower();
            const QString followValue = followRaw.section(QLatin1Char('='), 1).trimmed();
            if (followKey == QStringLiteral("exclusion"))
                break;
            if (followKey == QStringLiteral("fog_far")) {
                settings.fogFar = static_cast<int>(followValue.toDouble());
                settings.enabled = true;
            } else if (followKey == QStringLiteral("zone_shell")) {
                settings.zoneShell = followValue;
                settings.enabled = settings.enabled || !followValue.isEmpty();
            } else if (followKey == QStringLiteral("shell_scalar")) {
                settings.shellScalar = followValue.toDouble();
                settings.enabled = true;
            } else if (followKey == QStringLiteral("max_alpha")) {
                settings.maxAlpha = followValue.toDouble();
                settings.enabled = true;
            } else if (followKey == QStringLiteral("exclusion_tint")) {
                settings.exclusionTint = followValue;
                settings.enabled = settings.enabled || !followValue.isEmpty();
            }
        }
        return settings;
    }

    return {};
}

} // namespace flatlas::editors

#pragma once

#include "NewSystemDialog.h"

#include "domain/UniverseData.h"

class QString;
class QPointF;

namespace flatlas::editors {

struct CreatedSystemResult {
    flatlas::domain::SystemInfo systemInfo;
    QString absoluteSystemFilePath;
    QString ingameName;
};

class NewSystemService {
public:
    static NewSystemDialogOptions scanOptions(const QString &universeFilePath,
                                              const flatlas::domain::UniverseData *data);
    static QString nextSystemNickname(const QString &prefix,
                                      const flatlas::domain::UniverseData &data);
    static bool createSystem(const QString &universeFilePath,
                             const flatlas::domain::UniverseData &currentUniverse,
                             const NewSystemRequest &request,
                             const QPointF &position,
                             CreatedSystemResult *result,
                             QString *errorMessage);
};

} // namespace flatlas::editors

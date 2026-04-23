#pragma once

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

namespace flatlas::domain { class SystemDocument; }

namespace flatlas::editors {

class SystemSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SystemSettingsDialog(const flatlas::domain::SystemDocument *document,
                                  bool hasNonStandardSectionOrder,
                                  QWidget *parent = nullptr);

    QString systemNickname() const;
    double navMapScale() const;
    bool shouldNormalizeSectionOrder() const;

private:
    QLineEdit *m_nicknameEdit = nullptr;
    QDoubleSpinBox *m_navMapScaleSpin = nullptr;
    QCheckBox *m_normalizeSectionsCheck = nullptr;
};

} // namespace flatlas::editors

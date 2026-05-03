#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;

namespace flatlas::editors {

class ModSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit ModSettingsPage(QWidget *parent = nullptr);

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void reload();
    void save();
    QString mbasesPath() const;
    int detectedBribePrice() const;
    bool writeBribePrice(int price, QString *errorMessage);

    QSpinBox *m_bribePriceSpin = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
};

} // namespace flatlas::editors

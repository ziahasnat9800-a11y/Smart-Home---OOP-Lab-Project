#pragma once
#include <QFrame>
#include <QLabel>
#include <QPushButton>

class DeviceCard : public QFrame {
    Q_OBJECT
public:
    DeviceCard(int deviceId, const QString& name, const QString& type, bool isOn, QWidget* parent = nullptr);
    void setState(bool isOn);
    int deviceId() const { return m_deviceId; }

signals:
    void toggleRequested(int deviceId, const QString& name, const QString& type);

private:
    void updateCardStyle();
    static QString iconFor(const QString& type);

    int m_deviceId;
    QString m_name, m_type;
    bool m_isOn;
    QLabel* m_iconLabel;
    QPushButton* m_toggleBtn;
};
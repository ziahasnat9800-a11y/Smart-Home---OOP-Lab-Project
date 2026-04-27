#pragma once
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QStringList>
#include "database.h"

class DeviceCard;

class DashboardWindow : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWindow(const House& house, QWidget* parent = nullptr);

signals:
    void logoutRequested();

private slots:
    void refreshAll();
    void onScheduleTimerTick();
    void onUsageTimerTick();
    void openScheduleDialog();
    void onLogout();
    void onDeleteSchedule();
    void onRenameDevice();
    void onExportCSV();
    void onAbout();        // ← must be here

private:
    void setupUI();
    void applyStyles();
    void buildDeviceCards();
    void toggleDevice(int deviceId, const QString& deviceName, const QString& type);
    void refreshLogs();
    void refreshUsage();
    void refreshSchedules();
    void updateArduinoForDevice(int deviceId, bool isOn, const QString& type);
    void flushUsage();

    House m_house;
    QWidget* m_devicesContainer;
    QVBoxLayout* m_devicesLayout;
    QListWidget* m_logsList;
    QListWidget* m_schedulesList;
    QLabel* m_dailyLabel;
    QLabel* m_weeklyLabel;
    QLabel* m_monthlyLabel;
    QLabel* m_billLabel;
    QLabel* m_arduinoStatusLabel;
    QLabel* m_connectionDot;
    QLabel* m_devicesOnLabel;   // shows "X devices currently ON"

    QTimer* m_scheduleTimer;
    QTimer* m_usageTimer;

    QMap<int, QDateTime> m_deviceOnTimes;
    QMap<int, DeviceCard*> m_deviceCards;
    QMap<int, int> m_scheduleRowToId;   // ← must be here
};

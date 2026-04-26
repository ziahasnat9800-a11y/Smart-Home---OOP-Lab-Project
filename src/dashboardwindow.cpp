#include "dashboardwindow.h"
#include "devicecard.h"
#include "arduinocontroller.h"
#include "scheduledialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QFrame>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>

DashboardWindow::DashboardWindow(const House& house, QWidget* parent)
    : QWidget(parent), m_house(house) {
    setWindowTitle("Smart Home — House " + house.houseNumber);
    resize(1000, 700);

    QRect screen = QApplication::primaryScreen()->geometry();
    move((screen.width() - width()) / 2, (screen.height() - height()) / 2);

    setupUI();
    applyStyles();

    bool connected = ArduinoController::instance().connectToArduino();
    if (connected) {
        m_arduinoStatusLabel->setText("Arduino: Connected ✓");
        m_arduinoStatusLabel->setStyleSheet("color: #4ade80; font-size: 12px;");
        m_connectionDot->setStyleSheet("color: #4ade80;");
    } else {
        m_arduinoStatusLabel->setText("Arduino: Not connected");
        m_arduinoStatusLabel->setStyleSheet("color: #f87171; font-size: 12px;");
        m_connectionDot->setStyleSheet("color: #f87171;");
    }

    // Schedule checker — runs every 30 seconds
    m_scheduleTimer = new QTimer(this);
    connect(m_scheduleTimer, &QTimer::timeout, this, &DashboardWindow::onScheduleTimerTick);
    m_scheduleTimer->start(10000);
    m_scheduleTimer->setSingleShot(false);
    QTimer::singleShot(0, this, &DashboardWindow::onScheduleTimerTick); // immediate first check


    // Usage flush timer — runs every 1 minute.
    // Flushes elapsed minutes for every currently-ON device into the DB,
    // then resets their start time so minutes don't get double-counted.
    m_usageTimer = new QTimer(this);
    connect(m_usageTimer, &QTimer::timeout, this, &DashboardWindow::onUsageTimerTick);
    m_usageTimer->start(60000); // every 1 minute

    // On startup, record start times for any devices already ON in the DB
    // (in case of re-login after a crash)
    m_house = Database::instance().getHouse(m_house.houseNumber);
    for (const Room& r : m_house.rooms)
        for (const Device& d : r.devices)
            if (d.isOn)
                m_deviceOnTimes[d.id] = QDateTime::currentDateTime();

    buildDeviceCards();
    refreshLogs();
    refreshUsage();
    refreshSchedules();
}

// ---------------------------------------------------------------------------
// flushUsage() — the heart of the fix.
// For every device that is currently ON (tracked in m_deviceOnTimes),
// calculate how many minutes have elapsed since it was turned on (or since
// the last flush), write that to the DB, then reset the start time to NOW
// so the next flush only counts new time.
// Call this: (a) every minute via timer, (b) when a device is turned OFF,
// (c) on logout.
// ---------------------------------------------------------------------------
void DashboardWindow::flushUsage() {
    QDateTime now = QDateTime::currentDateTime();
    for (auto it = m_deviceOnTimes.begin(); it != m_deviceOnTimes.end(); ++it) {
        int deviceId = it.key();
        QDateTime& startTime = it.value();
        int elapsedMins = (int)(startTime.secsTo(now) / 60);
        if (elapsedMins > 0) {
            Database::instance().recordUsage(deviceId, elapsedMins);
            startTime = now; // reset so next flush doesn't double-count
        }
    }
}

void DashboardWindow::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget* topBar = new QWidget;
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(56);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 0, 20, 0);

    QLabel* titleLabel = new QLabel("🏠  Smart Home — " + m_house.houseNumber);
    titleLabel->setObjectName("topTitle");
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();

    m_connectionDot = new QLabel("●");
    m_connectionDot->setStyleSheet("color: #f87171; font-size: 14px;");
    m_arduinoStatusLabel = new QLabel("Arduino: Checking...");
    m_arduinoStatusLabel->setStyleSheet("color: #9ca3af; font-size: 12px;");
    topLayout->addWidget(m_connectionDot);
    topLayout->addWidget(m_arduinoStatusLabel);
    topLayout->addSpacing(16);

    QPushButton* scheduleBtn = new QPushButton("⏰  Schedule");
    scheduleBtn->setObjectName("headerBtn");
    QPushButton* logoutBtn = new QPushButton("Logout");
    logoutBtn->setObjectName("logoutBtn");
    topLayout->addWidget(scheduleBtn);
    topLayout->addWidget(logoutBtn);
    mainLayout->addWidget(topBar);

    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(16);

    // Left — device cards
    QWidget* leftPanel = new QWidget;
    leftPanel->setObjectName("panel");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 16, 16, 16);

    QLabel* devicesTitle = new QLabel("Electrical Devices");
    devicesTitle->setObjectName("panelTitle");
    leftLayout->addWidget(devicesTitle);

    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setObjectName("scrollArea");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_devicesContainer = new QWidget;
    m_devicesLayout = new QVBoxLayout(m_devicesContainer);
    m_devicesLayout->setContentsMargins(0, 0, 0, 0);
    m_devicesLayout->setSpacing(12);
    m_devicesLayout->addStretch();

    scroll->setWidget(m_devicesContainer);
    leftLayout->addWidget(scroll);
    contentLayout->addWidget(leftPanel, 3);

    // Right — stats / schedules / log
    QWidget* rightPanel = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(12);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Usage panel
    QWidget* usagePanel = new QWidget;
    usagePanel->setObjectName("panel");
    QVBoxLayout* usageLayout = new QVBoxLayout(usagePanel);
    usageLayout->setContentsMargins(16, 16, 16, 16);
    QLabel* usageTitle = new QLabel("📊  Usage Statistics");
    usageTitle->setObjectName("panelTitle");
    usageLayout->addWidget(usageTitle);

    auto makeStatRow = [&](const QString& label) -> QLabel* {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* lbl = new QLabel(label);
        lbl->setStyleSheet("color: #9ca3af; font-size: 12px;");
        QLabel* val = new QLabel("0.0 hrs");
        val->setStyleSheet("color: #e0e0f0; font-size: 13px; font-weight: 600;");
        val->setAlignment(Qt::AlignRight);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        usageLayout->addLayout(row);
        return val;
    };

    m_dailyLabel   = makeStatRow("Today:");
    m_weeklyLabel  = makeStatRow("This Week:");
    m_monthlyLabel = makeStatRow("This Month:");

    QFrame* divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color: #2d2d45; margin: 4px 0;");
    usageLayout->addWidget(divider);

    m_billLabel = new QLabel("Estimated Bill: PKR 0");
    m_billLabel->setStyleSheet("color: #fbbf24; font-size: 14px; font-weight: bold;");
    usageLayout->addWidget(m_billLabel);
    rightLayout->addWidget(usagePanel);

    // Schedules panel
    QWidget* schedPanel = new QWidget;
    schedPanel->setObjectName("panel");
    QVBoxLayout* schedLayout = new QVBoxLayout(schedPanel);
    schedLayout->setContentsMargins(16, 16, 16, 16);
    QLabel* schedTitle = new QLabel("⏰  Schedules");
    schedTitle->setObjectName("panelTitle");
    schedLayout->addWidget(schedTitle);
    m_schedulesList = new QListWidget;
    m_schedulesList->setObjectName("logList");
    m_schedulesList->setMaximumHeight(150);
    schedLayout->addWidget(m_schedulesList);

    QPushButton* deleteSchedBtn = new QPushButton("🗑  Delete Selected Schedule");
    deleteSchedBtn->setObjectName("dangerBtn");
    schedLayout->addWidget(deleteSchedBtn);
    connect(deleteSchedBtn, &QPushButton::clicked, this, &DashboardWindow::onDeleteSchedule);

    rightLayout->addWidget(schedPanel);

    // Activity log panel
    QWidget* logPanel = new QWidget;
    logPanel->setObjectName("panel");
    QVBoxLayout* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(16, 16, 16, 16);
    QLabel* logTitle = new QLabel("📋  Activity Log");
    logTitle->setObjectName("panelTitle");
    logLayout->addWidget(logTitle);
    m_logsList = new QListWidget;
    m_logsList->setObjectName("logList");
    logLayout->addWidget(m_logsList);
    rightLayout->addWidget(logPanel, 1);

    contentLayout->addWidget(rightPanel, 2);
    mainLayout->addLayout(contentLayout);

    connect(scheduleBtn, &QPushButton::clicked, this, &DashboardWindow::openScheduleDialog);
    connect(logoutBtn,   &QPushButton::clicked, this, &DashboardWindow::onLogout);
}

void DashboardWindow::applyStyles() {
    setStyleSheet(R"(
        QWidget { background-color: #0f0f1a; color: #e0e0f0; font-family: 'Segoe UI', sans-serif; }
        QWidget#topBar { background-color: #131325; border-bottom: 1px solid #2d2d45; }
        QLabel#topTitle { font-size: 16px; font-weight: bold; color: #7c9fff; }
        QWidget#panel { background: #131325; border: 1px solid #2d2d45; border-radius: 12px; }
        QLabel#panelTitle { font-size: 14px; font-weight: bold; color: #7c9fff; margin-bottom: 6px; }
        QScrollArea#scrollArea { border: none; background: transparent; }
        QScrollArea#scrollArea > QWidget > QWidget { background: transparent; }
        QListWidget#logList {
            background: #0f0f1a; border: 1px solid #2d2d45; border-radius: 8px;
            color: #d1d5db; font-size: 12px;
        }
        QListWidget#logList::item { padding: 5px; border-bottom: 1px solid #1e1e2e; }
        QPushButton#headerBtn {
            background: #1e1e2e; color: #7c9fff; border: 1.5px solid #7c9fff;
            border-radius: 8px; padding: 6px 14px; font-size: 13px;
        }
        QPushButton#headerBtn:hover { background: #2d2d45; }
        QPushButton#logoutBtn {
            background: #2e1e1e; color: #f87171; border: 1.5px solid #f87171;
            border-radius: 8px; padding: 6px 14px; font-size: 13px;
        }
        QPushButton#logoutBtn:hover { background: #3e2e2e; }
        QPushButton#dangerBtn {
            background: #2e1e1e; color: #f87171; border: 1.5px solid #f87171;
            border-radius: 6px; padding: 5px 10px; font-size: 12px;
        }
        QPushButton#dangerBtn:hover { background: #3e2e2e; }
        QScrollBar:vertical { background: #0f0f1a; width: 6px; }
        QScrollBar::handle:vertical { background: #2d2d45; border-radius: 3px; }
    )");
}

void DashboardWindow::buildDeviceCards() {
    for (auto* card : m_deviceCards)
        if (card) card->disconnect();
    m_deviceCards.clear();

    while (m_devicesLayout->count() > 0) {
        QLayoutItem* item = m_devicesLayout->takeAt(0);
        if (!item) break;
        if (QWidget* w = item->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* sub = item->layout()) {
            while (sub->count() > 0) {
                QLayoutItem* child = sub->takeAt(0);
                if (!child) break;
                if (QWidget* cw = child->widget()) { cw->hide(); cw->deleteLater(); }
                delete child;
            }
            delete sub;
        }
        delete item;
    }

    m_house = Database::instance().getHouse(m_house.houseNumber);

    for (const Room& room : m_house.rooms) {
        if (room.devices.isEmpty()) continue;

        QLabel* roomLabel = new QLabel(room.name);
        roomLabel->setStyleSheet("font-size: 13px; color: #9ca3af; font-weight: 600; margin-top: 4px;");
        m_devicesLayout->addWidget(roomLabel);

        QHBoxLayout* rowLayout = new QHBoxLayout;
        rowLayout->setSpacing(10);

        for (const Device& dev : room.devices) {
            DeviceCard* card = new DeviceCard(dev.id, dev.name, dev.type, dev.isOn);
            m_deviceCards[dev.id] = card;
            connect(card, &DeviceCard::toggleRequested,
                    this, &DashboardWindow::toggleDevice,
                    Qt::QueuedConnection);
            rowLayout->addWidget(card);
        }
        rowLayout->addStretch();
        m_devicesLayout->addLayout(rowLayout);
    }
    m_devicesLayout->addStretch();
}

void DashboardWindow::toggleDevice(int deviceId, const QString& deviceName, const QString& type) {
    Device dev = Database::instance().getDevice(deviceId);
    bool newState = !dev.isOn;

    Database::instance().setDeviceState(deviceId, newState);
    Database::instance().addActivityLog(deviceId, deviceName,
                                        QString("%1 turned %2").arg(deviceName).arg(newState ? "ON" : "OFF"));

    if (newState) {
        // Device turned ON — record the exact start time
        m_deviceOnTimes[deviceId] = QDateTime::currentDateTime();
    } else {
        // Device turned OFF — flush all elapsed time to DB right now, then remove it
        if (m_deviceOnTimes.contains(deviceId)) {
            int elapsedMins = (int)(m_deviceOnTimes[deviceId].secsTo(QDateTime::currentDateTime()) / 60);
            if (elapsedMins > 0)
                Database::instance().recordUsage(deviceId, elapsedMins);
            m_deviceOnTimes.remove(deviceId);
        }
    }

    updateArduinoForDevice(deviceId, newState, type);

    if (m_deviceCards.contains(deviceId) && m_deviceCards[deviceId])
        m_deviceCards[deviceId]->setState(newState);

    refreshLogs();
    refreshUsage();
}

void DashboardWindow::updateArduinoForDevice(int deviceId, bool isOn, const QString& type) {
    Q_UNUSED(deviceId)
    if (type == "light" && ArduinoController::instance().isConnected())
        ArduinoController::instance().controlLight(isOn);
}

void DashboardWindow::refreshAll() {
    buildDeviceCards();
    refreshLogs();
    refreshUsage();
    refreshSchedules();
}

void DashboardWindow::refreshLogs() {
    m_logsList->clear();
    QList<ActivityLog> logs = Database::instance().getActivityLogs(m_house.id, 30);
    for (const ActivityLog& log : logs) {
        m_logsList->addItem(
            QString("[%1]  %2")
                .arg(log.timestamp.toString("MM-dd hh:mm"))
                .arg(log.action));
    }
}

void DashboardWindow::refreshUsage() {
    double daily   = Database::instance().getDailyUsageHours(m_house.id);
    double weekly  = Database::instance().getWeeklyUsageHours(m_house.id);
    double monthly = Database::instance().getMonthlyUsageHours(m_house.id);
    double bill    = Database::instance().getEstimatedBill(m_house.id);

    m_dailyLabel->setText(QString("%1 hrs").arg(daily,   0, 'f', 2));
    m_weeklyLabel->setText(QString("%1 hrs").arg(weekly,  0, 'f', 2));
    m_monthlyLabel->setText(QString("%1 hrs").arg(monthly, 0, 'f', 2));
    m_billLabel->setText(QString("Estimated Bill: PKR %1").arg(bill, 0, 'f', 0));
}

void DashboardWindow::refreshSchedules() {
    m_schedulesList->clear();
    m_scheduleRowToId.clear();
    QList<Schedule> schedules = Database::instance().getSchedules(m_house.id);
    int row = 0;
    for (const Schedule& s : schedules) {
        if (s.executed) continue;
        m_schedulesList->addItem(
            QString("%1 → %2 at %3")
                .arg(s.deviceName)
                .arg(s.turnOn ? "ON" : "OFF")
                .arg(s.scheduledTime.toString("MM-dd hh:mm")));
        m_scheduleRowToId[row] = s.id;
        row++;
    }
}

void DashboardWindow::onDeleteSchedule() {
    int row = m_schedulesList->currentRow();
    if (row < 0 || !m_scheduleRowToId.contains(row)) {
        QMessageBox::information(this, "No Selection", "Please select a schedule to delete.");
        return;
    }
    int schedId = m_scheduleRowToId[row];
    if (Database::instance().deleteSchedule(schedId))
        refreshSchedules();
}

void DashboardWindow::onScheduleTimerTick() {
    QList<Schedule> pending = Database::instance().getPendingSchedules();
    for (const Schedule& s : pending) {
        Database::instance().setDeviceState(s.deviceId, s.turnOn);
        Database::instance().markScheduleExecuted(s.id);
        Database::instance().addActivityLog(s.deviceId, s.deviceName,
                                            QString("[Scheduled] %1 turned %2").arg(s.deviceName).arg(s.turnOn ? "ON" : "OFF"));

        // Keep m_deviceOnTimes in sync with scheduled toggles too
        if (s.turnOn) {
            m_deviceOnTimes[s.deviceId] = QDateTime::currentDateTime();
        } else {
            if (m_deviceOnTimes.contains(s.deviceId)) {
                int mins = (int)(m_deviceOnTimes[s.deviceId].secsTo(QDateTime::currentDateTime()) / 60);
                if (mins > 0) Database::instance().recordUsage(s.deviceId, mins);
                m_deviceOnTimes.remove(s.deviceId);
            }
        }

        Device dev = Database::instance().getDevice(s.deviceId);
        updateArduinoForDevice(s.deviceId, s.turnOn, dev.type);

        if (m_deviceCards.contains(s.deviceId) && m_deviceCards[s.deviceId])
            m_deviceCards[s.deviceId]->setState(s.turnOn);
    }
    if (!pending.isEmpty()) {
        refreshLogs();
        refreshSchedules();
    }
    // Always refresh usage after schedule tick (even if no schedule fired)
    // so the display stays current while devices are on
    flushUsage();
    refreshUsage();
}

void DashboardWindow::onUsageTimerTick() {
    // Every 60 seconds: flush elapsed time for all ON devices → DB, then update display
    flushUsage();
    refreshUsage();
}

void DashboardWindow::openScheduleDialog() {
    m_house = Database::instance().getHouse(m_house.houseNumber);
    ScheduleDialog dlg(m_house.rooms, this);
    if (dlg.exec() == QDialog::Accepted)
        refreshSchedules();
}

void DashboardWindow::onLogout() {
    // Flush any remaining usage before leaving
    flushUsage();
    ArduinoController::instance().disconnectArduino();
    emit logoutRequested();
}
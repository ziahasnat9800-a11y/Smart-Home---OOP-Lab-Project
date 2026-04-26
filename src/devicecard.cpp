#include "devicecard.h"
#include <QVBoxLayout>

DeviceCard::DeviceCard(int deviceId, const QString& name, const QString& type, bool isOn, QWidget* parent)
    : QFrame(parent), m_deviceId(deviceId), m_name(name), m_type(type), m_isOn(isOn) {
    setObjectName("deviceCard");
    setFixedSize(160, 130);
    setFrameShape(QFrame::StyledPanel);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    m_iconLabel = new QLabel(iconFor(type), this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("font-size: 28px;");
    layout->addWidget(m_iconLabel);

    QLabel* nameLabel = new QLabel(name, this);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet("font-size: 12px; color: #d1d5db; font-weight: 600;");
    layout->addWidget(nameLabel);

    m_toggleBtn = new QPushButton(isOn ? "ON" : "OFF", this);
    m_toggleBtn->setObjectName(isOn ? "toggleOn" : "toggleOff");
    layout->addWidget(m_toggleBtn);

    updateCardStyle();

    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        emit toggleRequested(m_deviceId, m_name, m_type);
    });
}

void DeviceCard::setState(bool isOn) {
    m_isOn = isOn;
    m_toggleBtn->setText(isOn ? "ON" : "OFF");
    m_toggleBtn->setObjectName(isOn ? "toggleOn" : "toggleOff");
    updateCardStyle();
}

void DeviceCard::updateCardStyle() {
    if (m_isOn) {
        setStyleSheet(R"(
            QFrame#deviceCard {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #1a2a40,stop:1 #1e1e35);
                border: 2px solid #7c9fff;
                border-radius: 12px;
            }
            QPushButton#toggleOn {
                background: #7c9fff; color: #0f0f1a;
                border: none; border-radius: 6px; padding: 5px;
                font-weight: bold; font-size: 12px;
            }
        )");
    } else {
        setStyleSheet(R"(
            QFrame#deviceCard {
                background: #1e1e2e;
                border: 1.5px solid #2d2d45;
                border-radius: 12px;
            }
            QPushButton#toggleOff {
                background: #2d2d45; color: #6b7280;
                border: none; border-radius: 6px; padding: 5px;
                font-size: 12px;
            }
        )");
    }
}

QString DeviceCard::iconFor(const QString& type) {
    if (type == "light") return "💡";
    if (type == "fan")   return "🌀";
    if (type == "ac")    return "❄️";
    if (type == "tv")    return "📺";
    return "🔌";
}
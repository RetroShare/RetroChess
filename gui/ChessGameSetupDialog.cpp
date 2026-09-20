/*******************************************************************************
 * gui/ChessGameSetupDialog.cpp                                                *
 *                                                                             *
 * Copyright (C) 2026 RetroShare Team <retroshare.project@gmail.com>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#include "ChessGameSetupDialog.h"

#include <QTabWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QFrame>
#include <QSpacerItem>

// ---------------------------------------------------------------------------
// Preset definitions  (minutes, increment)
// ---------------------------------------------------------------------------
static const struct { int min; int inc; const char *label; } kPresets[] = {
    {  1,  0, "1+0"   },
    {  2,  1, "2+1"   },
    {  3,  0, "3+0"   },
    {  3,  2, "3+2"   },
    {  5,  0, "5+0"   },
    {  5,  3, "5+3"   },
    { 10,  0, "10+0"  },
    { 10,  5, "10+5"  },
    { 15, 10, "15+10" },
    { 30,  0, "30+0"  },
    { 30, 20, "30+20" },
};
static constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

// ---------------------------------------------------------------------------

ChessGameSetupDialog::ChessGameSetupDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Game Setup"));
    setMinimumWidth(420);
    setSizeGripEnabled(false);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(12);

    // Title
    QLabel *title = new QLabel(tr("Game Setup"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size: 20pt; font-weight: bold;"));
    root->addWidget(title);

    // Tab widget
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);

    // --- Unlimited tab ---
    QWidget *unlimitedTab = new QWidget;
    buildUnlimitedTab(unlimitedTab);
    m_tabs->addTab(unlimitedTab, tr("Unlimited"));

    // --- Real time tab ---
    QWidget *realTimeTab = new QWidget;
    buildRealTimeTab(realTimeTab);
    m_tabs->addTab(realTimeTab, tr("Real time"));

    root->addWidget(m_tabs);

    // "Create lobby game" button
    QPushButton *createBtn = new QPushButton(this);
    createBtn->setText(tr("Create lobby game"));
    createBtn->setMinimumHeight(40);
    createBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: #5d9c3a;"
            "  color: white;"
            "  border-radius: 6px;"
            "  font-size: 13pt;"
            "  font-weight: bold;"
            "  padding: 6px 20px;"
            "}"
            "QPushButton:hover { background-color: #4e8631; }"
            "QPushButton:pressed { background-color: #3d6a27; }"));
    connect(createBtn, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(createBtn);
}

// ---------------------------------------------------------------------------
// Unlimited tab
// ---------------------------------------------------------------------------

void ChessGameSetupDialog::buildUnlimitedTab(QWidget *tab)
{
    QVBoxLayout *lay = new QVBoxLayout(tab);
    lay->setAlignment(Qt::AlignCenter);
    lay->addStretch();

    QLabel *desc = new QLabel(tr("Take all the time you need"), tab);
    desc->setAlignment(Qt::AlignCenter);
    desc->setStyleSheet(QStringLiteral("font-size: 13pt; color: #555;"));
    lay->addWidget(desc);

    lay->addStretch();
}

// ---------------------------------------------------------------------------
// Real-time tab
// ---------------------------------------------------------------------------

void ChessGameSetupDialog::buildRealTimeTab(QWidget *tab)
{
    QVBoxLayout *lay = new QVBoxLayout(tab);
    lay->setSpacing(10);

    // ---- Sliders row -------------------------------------------------------
    //   [Minutes per side]  [N]  +  [I]  [Increment in seconds]
    QHBoxLayout *slidersRow = new QHBoxLayout;

    QLabel *minLabel = new QLabel(tr("Minutes per side"), tab);
    minLabel->setStyleSheet(QStringLiteral("font-size: 10pt;"));

    m_minutesSlider = new QSlider(Qt::Horizontal, tab);
    m_minutesSlider->setRange(1, 60);
    m_minutesSlider->setValue(10);

    m_minutesBadge = new QLabel(QStringLiteral("10"), tab);
    m_minutesBadge->setAlignment(Qt::AlignCenter);
    m_minutesBadge->setFixedSize(30, 24);
    m_minutesBadge->setStyleSheet(
        QStringLiteral("background:#333; color:white; border-radius:4px; font-weight:bold;"));

    QLabel *plusLabel = new QLabel(QStringLiteral("+"), tab);
    plusLabel->setAlignment(Qt::AlignCenter);
    plusLabel->setStyleSheet(QStringLiteral("font-size: 12pt; color: #555;"));

    m_incrBadge = new QLabel(QStringLiteral("0"), tab);
    m_incrBadge->setAlignment(Qt::AlignCenter);
    m_incrBadge->setFixedSize(30, 24);
    m_incrBadge->setStyleSheet(
        QStringLiteral("background:#333; color:white; border-radius:4px; font-weight:bold;"));

    m_incrSlider = new QSlider(Qt::Horizontal, tab);
    m_incrSlider->setRange(0, 60);
    m_incrSlider->setValue(0);

    QLabel *incrLabel = new QLabel(tr("Increment in seconds"), tab);
    incrLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    incrLabel->setStyleSheet(QStringLiteral("font-size: 10pt;"));

    slidersRow->addWidget(minLabel);
    slidersRow->addWidget(m_minutesSlider, 1);
    slidersRow->addWidget(m_minutesBadge);
    slidersRow->addWidget(plusLabel);
    slidersRow->addWidget(m_incrBadge);
    slidersRow->addWidget(m_incrSlider, 1);
    slidersRow->addWidget(incrLabel);

    lay->addLayout(slidersRow);

    // ---- Preset buttons ----------------------------------------------------
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing(6);

    for (int i = 0; i < kPresetCount; ++i) {
        const auto &p = kPresets[i];
        QPushButton *btn = new QPushButton(QLatin1String(p.label), tab);
        btn->setCheckable(true);
        btn->setFlat(false);
        btn->setFixedHeight(32);
        btn->setStyleSheet(
            QStringLiteral(
                "QPushButton {"
                "  border: 1px solid #bbb;"
                "  border-radius: 4px;"
                "  padding: 2px 6px;"
                "  font-size: 10pt;"
                "}"
                "QPushButton:checked {"
                "  background-color: #5d9c3a;"
                "  color: white;"
                "  border-color: #4e8631;"
                "}"
                "QPushButton:hover:!checked { background-color: #e8e8e8; }"));

        const int min = p.min;
        const int inc = p.inc;
        connect(btn, &QPushButton::clicked, this, [this, min, inc]() {
            onPresetClicked(min, inc);
        });

        Preset preset;
        preset.minutes = p.min;
        preset.increment = p.inc;
        preset.btn = btn;
        m_presets.append(preset);

        grid->addWidget(btn, i / 4, i % 4);   // 4 columns
    }
    lay->addLayout(grid);

    // ---- Category label ----------------------------------------------------
    m_categoryLabel = new QLabel(tab);
    m_categoryLabel->setAlignment(Qt::AlignCenter);
    m_categoryLabel->setStyleSheet(QStringLiteral("font-size: 10pt; color: #666;"));
    lay->addWidget(m_categoryLabel);

    lay->addStretch();

    // Wire sliders
    connect(m_minutesSlider, &QSlider::valueChanged, this, &ChessGameSetupDialog::onSlidersChanged);
    connect(m_incrSlider,    &QSlider::valueChanged, this, &ChessGameSetupDialog::onSlidersChanged);

    // Select "10+0" as default
    onPresetClicked(10, 0);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void ChessGameSetupDialog::onPresetClicked(int minutes, int increment)
{
    // Block signals so we don't re-enter while updating sliders
    const QSignalBlocker b1(m_minutesSlider);
    const QSignalBlocker b2(m_incrSlider);

    m_minutesSlider->setValue(minutes);
    m_incrSlider->setValue(increment);
    m_minutesBadge->setText(QString::number(minutes));
    m_incrBadge->setText(QString::number(increment));

    updatePresetHighlight();
    updateCategoryLabel();
}

void ChessGameSetupDialog::onSlidersChanged()
{
    m_minutesBadge->setText(QString::number(m_minutesSlider->value()));
    m_incrBadge->setText(QString::number(m_incrSlider->value()));
    updatePresetHighlight();
    updateCategoryLabel();
}

void ChessGameSetupDialog::updatePresetHighlight()
{
    const int min  = m_minutesSlider->value();
    const int incr = m_incrSlider->value();
    for (auto &p : m_presets) {
        if (p.btn)
            p.btn->setChecked(p.minutes == min && p.increment == incr);
    }
}

void ChessGameSetupDialog::updateCategoryLabel()
{
    const ChessTimeControl tc(m_minutesSlider->value(), m_incrSlider->value());
    m_categoryLabel->setText(tc.category());
}

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

ChessTimeControl ChessGameSetupDialog::selectedTimeControl() const
{
    // If the Unlimited tab is active, return unlimited.
    if (m_tabs->currentIndex() == 0)
        return ChessTimeControl{};

    return ChessTimeControl(m_minutesSlider->value(), m_incrSlider->value());
}

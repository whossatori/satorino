// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"
#include "singletons/Settings.hpp"
#include "util/SatorinoFlags.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include <QColor>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace chatterino {
void SatorinoPage::addLocalFlags(GeneralPageView &layout)
{
    layout.addTitle("Local user highlights");
    layout.addDescription(
        "Local to this device and channel. Flags never "
        "report or restrict users on Twitch. Remove individual "
        "highlights with Remove highlight in the user's popup flag menu.");
    SettingWidget::checkbox("Enable local user flags",
                            getSettings()->satorinoFlagsEnabled)
        ->addKeywords({"flag", "user", "highlight"})
        ->addTo(layout);
    SettingWidget::checkbox("Show flag mark before the name",
                            getSettings()->satorinoFlagsMark)
        ->conditionallyEnabledBy(getSettings()->satorinoFlagsEnabled)
        ->addTo(layout);
    auto *group = new QWidget;
    auto *box = new QVBoxLayout(group);
    box->setContentsMargins(0, 0, 0, 0);
    auto *actions = new QHBoxLayout;
    auto *colour = new QPushButton("Highlight colour");
    colour->setToolTip("Subtle fallback colour; mentions, rewards and existing "
                       "highlights keep priority. Alpha is preserved.");
    actions->addWidget(colour);
    actions->addStretch(1);
    box->addLayout(actions);
    getSettings()->satorinoFlagsEnabled.connect(
        [colour](bool enabled) {
            colour->setEnabled(enabled);
        },
        this->managedConnections_);
    getSettings()->satorinoFlagsColour.connect(
        [colour](const QString &value) {
            const QColor parsed(value);
            const QColor chosen = parsed.isValid()
                                      ? parsed
                                      : QColor(satorino::FLAGS_COLOUR_DEFAULT);
            colour->setStyleSheet(
                QString("background-color: rgba(%1,%2,%3,%4);")
                    .arg(chosen.red())
                    .arg(chosen.green())
                    .arg(chosen.blue())
                    .arg(chosen.alpha()));
        },
        this->managedConnections_);
    QObject::connect(colour, &QPushButton::clicked, this, [this] {
        const QPointer<SatorinoPage> guard(this);
        const QColor stored(getSettings()->satorinoFlagsColour.getValue());
        const auto chosen = QColorDialog::getColor(
            stored.isValid() ? stored : QColor(satorino::FLAGS_COLOUR_DEFAULT),
            this, "Local flag highlight colour",
            QColorDialog::ShowAlphaChannel);
        if (guard && chosen.isValid())
            getSettings()->satorinoFlagsColour.setValue(
                chosen.name(QColor::HexArgb));
    });
    layout.addWidget(group,
                     {"local highlights", "flag", "mark", "highlight colour"});
}
}  // namespace chatterino

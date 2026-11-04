// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ChannelHighlightsDialog.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/highlights/ChannelHighlightModel.hpp"
#include "controllers/highlights/HighlightController.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/splits/Split.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <tuple>

namespace chatterino::satorino {

void addChannelHighlightsAction(QMenu *menu, Split *split)
{
    auto *action = menu->addAction(QString::fromUtf8("Channel highlights…"));
    const auto updateEnabled = [action, split] {
        action->setEnabled(
            !channelHighlightPlatform(split->getSelectedChannel().get())
                 .isEmpty());
    };
    updateEnabled();
    QObject::connect(menu, &QMenu::aboutToShow, split, updateEnabled);
    QObject::connect(action, &QAction::triggered, split, [split] {
        const auto channel = split->getSelectedChannel();
        if (!channelHighlightPlatform(channel.get()).isEmpty())
        {
            auto *dialog =
                new ChannelHighlightsDialog(*channel, split->window());
            dialog->show();
        }
    });
}

ChannelHighlightsDialog::ChannelHighlightsDialog(const Channel &channel,
                                                 QWidget *parent)
    : BasePopup(
          {BaseWindow::DisableCustomScaling, BaseWindow::DisableLayoutSave,
           BaseWindow::BoundsCheckOnShow, BaseWindow::UseSettingsStylesheet},
          parent)
{
    this->setObjectName("ChannelHighlightsDialog");
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle("Channel highlights - #" + channel.getName());
    this->resize(1100, 450);

    auto &phrases = getApp()->getHighlights()->channelHighlights().editable(
        {channelHighlightPlatform(&channel), channel.getName()});
    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    auto *description = new QLabel(
        "Message phrases for this channel. Changes apply to new messages "
        "immediately and are shared by all splits showing this channel. "
        "Channel colour and sound take priority over global highlights.");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *model = new ChannelHighlightModel(phrases, this);
    auto *view = new EditableModelView(model);
    view->setTitles({"Pattern", "Show in mentions", "Flash taskbar",
                     "Play sound", "Custom sound", "Colour", "Regex",
                     "Case-sensitive"});
    view->addRegexHelpLink();
    auto *table = view->getTableView();
    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(
        ChannelHighlightModel::Pattern, QHeaderView::Stretch);
    layout->addWidget(view);
    std::ignore = view->addButtonPressed.connect([&phrases] {
        phrases.append(ChannelHighlightPhrase::createDefault());
    });

    QObject::connect(
        table, &QTableView::doubleClicked, this,
        [this, model](const QModelIndex &clicked) {
            const QPersistentModelIndex index(clicked);
            if (clicked.column() == ChannelHighlightModel::CustomSound)
            {
                const auto file = QFileDialog::getOpenFileName(
                    this, "Choose highlight sound", {},
                    "Audio files (*.wav *.mp3 *.ogg *.flac);;All files (*)");
                if (!file.isEmpty() && index.isValid())
                {
                    model->setData(index, QUrl::fromLocalFile(file),
                                   Qt::UserRole);
                }
            }
            else if (clicked.column() == ChannelHighlightModel::Color)
            {
                auto *picker = new ColorPickerDialog(
                    clicked.data(Qt::DecorationRole).value<QColor>(), this);
                QObject::connect(picker, &ColorPickerDialog::colorConfirmed,
                                 this, [model, index](QColor color) {
                                     if (index.isValid())
                                     {
                                         model->setData(index, color,
                                                        Qt::DecorationRole);
                                     }
                                 });
                picker->show();
            }
        });

    auto *defaultSound = new QPushButton("Use default sound");
    defaultSound->setToolTip("Clear the selected phrase's custom sound file. "
                             "The Play sound checkbox controls playback.");
    view->addCustomButton(defaultSound);
    QObject::connect(defaultSound, &QPushButton::clicked, this, [model, table] {
        const auto rows = table->selectionModel()->selectedRows(0);
        if (!rows.isEmpty())
        {
            model->setData(model->index(rows.front().row(),
                                        ChannelHighlightModel::CustomSound),
                           QUrl(), Qt::UserRole);
        }
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                     &QWidget::close);
}

}  // namespace chatterino::satorino

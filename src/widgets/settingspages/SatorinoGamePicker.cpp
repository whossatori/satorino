// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoGamePicker.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Settings.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColorDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QImageReader>
#include <QIODevice>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <utility>

namespace chatterino {
using namespace satorino;
namespace {
void clearRows(QVBoxLayout *layout)
{
    while (auto *item = layout->takeAt(0))
    {
        if (auto *widget = item->widget())
        {
            widget->hide();
            widget
                ->deleteLater();  // A clicked button may still be delivering its signal.
        }
        delete item;
    }
}
QScrollArea *makeList(QWidget *parent, QVBoxLayout *&rows)
{
    auto *view = new QScrollArea(parent);
    view->setWidgetResizable(true);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body = new QWidget(view);
    rows = new QVBoxLayout(body);
    rows->setContentsMargins(2, 2, 2, 2);
    rows->setSpacing(0);
    rows->setAlignment(Qt::AlignTop);
    view->setWidget(body);
    return view;
}
QLabel *gameLabel(const HighlightedGame &game, QWidget *row)
{
    auto *label =
        new QLabel(game.name.isEmpty() ? "Game ID " + game.id : game.name, row);
    label->setTextFormat(Qt::PlainText);
    label->setToolTip(label->text());
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}
QLabel *artLabel(QWidget *row)
{
    auto *label = new QLabel("…", row);
    label->setFixedSize(26, 36);
    label->setAlignment(Qt::AlignCenter);
    label->setToolTip("Loading box art");
    return label;
}
std::optional<qsizetype> selectedIndex(const GameList &games,
                                       const HighlightedGame &game)
{
    return firstMatchingGame(games, QVector<LiveGame>{{game.id, game.name}});
}
QImage decodeThumbnail(QByteArray bytes)
{
    if (bytes.size() > 256 * 1024)
        return {};
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::ReadOnly))
        return {};
    QImageReader reader(&buffer);
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 2048 || size.height() > 2048)
        return {};
    reader.setScaledSize(size.scaled(52, 72, Qt::KeepAspectRatio));
    return reader.read();
}
}  // namespace

SatorinoGamePicker::SatorinoGamePicker(QWidget *parent)
    : QWidget(parent)
{
    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(new QLabel("Games to highlight", this));
    this->query_ = new QLineEdit(this);
    this->query_->setPlaceholderText("Search games");
    this->query_->setAccessibleName("Search games");
    this->query_->setClearButtonEnabled(true);
    this->query_->setMaxLength(200);
    box->addWidget(this->query_);
    this->status_ = new QLabel(this);
    this->status_->setWordWrap(true);
    box->addWidget(this->status_);
    this->results_ = makeList(this, this->resultRows_);
    box->addWidget(this->results_);
    box->addWidget(new QLabel("Selected games", this));
    auto *help = new QLabel("First matching game in this list wins across all "
                            "splits. Use ↑ / ↓ to reorder.",
                            this);
    help->setWordWrap(true);
    help->setStyleSheet("color: #999;");
    box->addWidget(help);
    this->selectedView_ = makeList(this, this->selectedRows_);
    box->addWidget(this->selectedView_);
    this->selectionStatus_ = new QLabel(this);
    this->selectionStatus_->setWordWrap(true);
    box->addWidget(this->selectionStatus_);
    this->clock_.start();
    this->debounce_.setSingleShot(true);
    this->debounce_.setTimerType(
        Qt::PreciseTimer);  // Never fire before the pure deadline.
    this->debounce_.setInterval(GameSearchState::DEBOUNCE_MS);
    QObject::connect(this->query_, &QLineEdit::textChanged, this, [this] {
        this->editSearch();
    });
    QObject::connect(&this->debounce_, &QTimer::timeout, this, [this] {
        this->search();
    });
    getSettings()->satorinoLiveGamesV2.connect(
        [this](const GameList &games) {
            this->selected_ = games;
            this->showSelected();
            this->updateAddButtons();
        },
        this->connections_);
    this->accountConnection_ =
        getApp()->getAccounts()->twitch.currentUserChanged.connect([this] {
            this->editSearch();  // Invalidate results from the previous account, including failures.
        });
    this->editSearch();
}
void SatorinoGamePicker::editSearch()
{
    this->debounce_.stop();
    this->searchState_.edit(this->query_->text(), this->clock_.elapsed());
    this->showResults({});
    if (getApp()->getAccounts()->twitch.getCurrent()->isAnon())
        this->status_->setText("Log in to Twitch to search games.");
    else if (!this->searchState_.longEnough())
        this->status_->setText("Enter at least 2 characters to search games.");
    else
    {
        this->status_->setText("Searching games…");
        this->debounce_.start();
    }
}
void SatorinoGamePicker::search()
{
    if (this->inFlight_ ||
        getApp()->getAccounts()->twitch.getCurrent()->isAnon())
        return;
    const auto request = this->searchState_.takeReady(this->clock_.elapsed());
    if (!request)
        return;
    this->inFlight_ = true;
    QPointer<SatorinoGamePicker> self(this);
    getHelix()->searchGames(
        request->query,
        [self, generation = request->generation](std::vector<HelixGame> games) {
            if (!self)
                return;
            self->inFlight_ = false;
            if (self->searchState_.accepts(generation))
            {
                GameList results;
                for (const auto &game : games)
                {
                    if (game.id.isEmpty() || game.name.isEmpty())
                        continue;
                    results.push_back({game.id, game.name, game.boxArtUrl, {}});
                    if (results.size() >= 20)
                        break;
                }
                auto selected = self->selected_;
                bool learned = false;
                for (const auto &game : results)
                    learned = learnGame(selected, game.id, game.name,
                                        game.boxArtUrl) ||
                              learned;
                if (learned)
                    getSettings()->satorinoLiveGamesV2.setValue(selected);
                self->status_->setText(
                    results.isEmpty() ? "No games found."
                                      : "Add games from the results below.");
                self->showResults(results);
            }
            self->search();  // Start the newest query if an older request occupied the slot.
        },
        [self, generation = request->generation] {
            if (!self)
                return;
            self->inFlight_ = false;
            if (self->searchState_.accepts(generation))
                self->status_->setText("Game search failed. Check your Twitch "
                                       "login or connection and try again.");
            self->search();
        });
}
void SatorinoGamePicker::showResults(const GameList &games)
{
    clearRows(this->resultRows_);
    this->addButtons_.clear();
    for (const auto &game : games)
    {
        auto *row = new QWidget(this->results_);
        auto *line = new QHBoxLayout(row);
        line->setContentsMargins(2, 4, 2, 4);
        auto *art = artLabel(row);
        line->addWidget(art);
        line->addWidget(gameLabel(game, row), 1);
        auto *add = new QPushButton("Add", row);
        add->setFixedWidth(70);
        line->addWidget(add);
        this->addButtons_.emplace_back(game, add);
        QObject::connect(add, &QPushButton::clicked, this, [this, game] {
            if (selectedIndex(this->selected_, game))
                return;
            if (this->selected_.size() >= MAX_HIGHLIGHTED_GAMES)
            {
                this->selectionStatus_->setText(
                    "Limit reached: remove a game before adding another "
                    "(maximum 50).");
                return;
            }
            auto chosen = game;
            chosen.colour = nextGameColour(this->selected_);
            this->selected_.push_back(chosen);
            this->save();
        });
        this->resultRows_->addWidget(row);
        this->thumbnail(game.boxArtUrl, art);
    }
    this->results_->setVisible(!games.isEmpty());
    this->results_->setFixedHeight(
        static_cast<int>(std::min<qsizetype>(8, games.size())) * 46 + 6);
    this->updateAddButtons();
}
void SatorinoGamePicker::updateAddButtons()
{
    for (const auto &[game, button] : this->addButtons_)
    {
        if (!button)
            continue;
        const bool selected = selectedIndex(this->selected_, game).has_value();
        button->setText(selected ? "Added" : "Add");
        button->setEnabled(!selected &&
                           this->selected_.size() < MAX_HIGHLIGHTED_GAMES);
    }
}
void SatorinoGamePicker::showSelected()
{
    clearRows(this->selectedRows_);
    for (qsizetype i = 0; i < this->selected_.size(); ++i)
    {
        const auto game = this->selected_[i];
        auto *row = new QWidget(this->selectedView_);
        auto *line = new QHBoxLayout(row);
        line->setContentsMargins(2, 4, 2, 4);
        line->setSpacing(4);
        auto *art = artLabel(row);
        line->addWidget(art);
        line->addWidget(gameLabel(game, row), 1);
        auto *colour = new QPushButton(row);
        colour->setFixedSize(28, 26);
        colour->setStyleSheet("background-color: " + game.colour.name() +
                              "; border: 1px solid " +
                              dotOutline(game.colour).name() + ";");
        colour->setToolTip("Dot / star colour");
        colour->setAccessibleName("Dot / star colour for " + game.name);
        line->addWidget(colour);
        QObject::connect(colour, &QPushButton::clicked, this, [this, game] {
            const QPointer<SatorinoGamePicker> guard(this);
            const auto colour =
                QColorDialog::getColor(game.colour, this, "Dot / star colour");
            if (!guard)
                return;
            const auto index = selectedIndex(this->selected_, game);
            if (colour.isValid() && index)
            {
                this->selected_[*index].colour = colour;
                this->save();
            }
        });
        for (int direction : {-1, 1})
        {
            auto *move = new QPushButton(direction < 0 ? "↑" : "↓", row);
            move->setFixedWidth(26);
            move->setToolTip(direction < 0 ? "Move game up" : "Move game down");
            move->setAccessibleName(move->toolTip());
            move->setEnabled(direction < 0 ? i > 0
                                           : i + 1 < this->selected_.size());
            line->addWidget(move);
            QObject::connect(
                move, &QPushButton::clicked, this, [this, game, direction] {
                    const auto index = selectedIndex(this->selected_, game);
                    if (index && *index + direction >= 0 &&
                        *index + direction < this->selected_.size())
                    {
                        this->selected_.swapItemsAt(*index, *index + direction);
                        this->save();
                    }
                });
        }
        auto *remove = new QPushButton("Remove", row);
        remove->setFixedWidth(60);
        line->addWidget(remove);
        QObject::connect(remove, &QPushButton::clicked, this, [this, game] {
            if (const auto index = selectedIndex(this->selected_, game))
            {
                this->selected_.removeAt(*index);
                this->save();
            }
        });
        this->selectedRows_->addWidget(row);
        this->thumbnail(game.boxArtUrl, art);
    }
    this->selectedView_->setVisible(!this->selected_.isEmpty());
    this->selectedView_->setFixedHeight(
        static_cast<int>(std::min<qsizetype>(4, this->selected_.size())) * 46 +
        6);
    this->selectionStatus_->setText(
        this->selected_.size() >= MAX_HIGHLIGHTED_GAMES
            ? "Limit reached: remove a game before adding another (maximum 50)."
        : this->selected_.isEmpty() ? "No games selected."
                                    : QString{});
}
void SatorinoGamePicker::save()
{
    // Synchronous registered-setting notification rebuilds every open picker/tab.
    getSettings()->satorinoLiveGamesV2.setValue(this->selected_);
}
void SatorinoGamePicker::thumbnail(const QString &source, QLabel *label)
{
    const auto url = gameBoxArtUrl(source);
    if (url.isEmpty())
    {
        label->setText("—");
        label->setToolTip("Box art unavailable");
        return;
    }
    if (const auto *pixmap = this->thumbnailCache_.object(url))
    {
        if (!pixmap->isNull())
            label->setPixmap(pixmap->scaled(26, 36, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        else
            label->setText("—");
        label->setToolTip(pixmap->isNull() ? "Box art unavailable" : QString{});
        return;
    }
    if (this->thumbnailWaiters_.size() >= 128)
    {
        label->setText("—");
        label->setToolTip("Box art unavailable");
        return;
    }
    auto [it, inserted] = this->thumbnailWaiters_.try_emplace(url);
    // Discard dead row subscriptions when lists have been rebuilt.
    std::erase_if(it->second, [](const auto &target) {
        return target.isNull();
    });
    it->second.push_back(label);
    if (inserted)
        this->thumbnailQueue_.push_back(url);
    this->pumpThumbnails();
}
void SatorinoGamePicker::pumpThumbnails()
{
    while (this->activeThumbnails_ < 4 && !this->thumbnailQueue_.empty())
    {
        const auto url = this->thumbnailQueue_.front();
        this->thumbnailQueue_.pop_front();
        const auto it = this->thumbnailWaiters_.find(url);
        if (it == this->thumbnailWaiters_.end())
            continue;
        if (std::none_of(it->second.begin(), it->second.end(),
                         [](const auto &label) {
                             return !label.isNull();
                         }))
        {
            this->thumbnailWaiters_.erase(it);
            continue;
        }
        ++this->activeThumbnails_;
        NetworkRequest(QUrl(url))
            .caller(this)
            .timeout(10000)
            .onSuccess([this, url](const NetworkResult &result) {
                // Decode on the existing Qt worker pool; only make QPixmaps on the GUI thread.
                auto *watcher = new QFutureWatcher<QImage>(this);
                QObject::connect(watcher, &QFutureWatcher<QImage>::finished,
                                 this, [this, url, watcher] {
                                     this->finishThumbnail(url,
                                                           watcher->result());
                                     watcher->deleteLater();
                                 });
                watcher->setFuture(
                    QtConcurrent::run([bytes = result.getData()] {
                        return decodeThumbnail(bytes);
                    }));
            })
            .onError([this, url](const NetworkResult &) {
                this->finishThumbnail(url, {});
            })
            .execute();
    }
}
void SatorinoGamePicker::finishThumbnail(const QString &url,
                                         const QImage &image)
{
    const auto pixmap = QPixmap::fromImage(image);
    this->thumbnailCache_.insert(
        url,
        new QPixmap(pixmap));  // Null images are a bounded negative cache too.
    if (const auto it = this->thumbnailWaiters_.find(url);
        it != this->thumbnailWaiters_.end())
    {
        for (const auto &label : it->second)
        {
            if (!label)
                continue;
            if (pixmap.isNull())
                label->setText("—");
            else
                label->setPixmap(pixmap.scaled(26, 36, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
            label->setToolTip(pixmap.isNull() ? "Box art unavailable"
                                              : QString{});
        }
        this->thumbnailWaiters_.erase(it);
    }
    --this->activeThumbnails_;
    this->pumpThumbnails();
}
}  // namespace chatterino

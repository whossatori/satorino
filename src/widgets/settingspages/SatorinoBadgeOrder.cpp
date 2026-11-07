// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoBadgeOrder.hpp"

#include "Application.hpp"
#include "common/Aliases.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/badges/SatorinoBadgeOrder.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "messages/Message.hpp"
#include "messages/MessageElement.hpp"
#include "providers/badgehq/BadgeHQBadges.hpp"
#include "providers/bluzyrino/BluzyrinoBadges.hpp"
#include "providers/bttv/BttvBadges.hpp"
#include "providers/chatsen/ChatsenBadges.hpp"
#include "providers/chatterino/ChatterinoBadges.hpp"
#include "providers/dankchat/DankChatBadges.hpp"
#include "providers/ffz/FfzBadges.hpp"
#include "providers/ffzap/FfzApBadges.hpp"
#include "providers/folhinha/FolhinhaBadges.hpp"
#include "providers/jilchat/JilChatBadges.hpp"
#include "providers/limerino/LimerinoBadges.hpp"
#include "providers/homies/HomiesBadges.hpp"
#include "providers/seventv/SeventvBadges.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchBadges.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/SatorinoBadgeOrder.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QStringList>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace chatterino {
using namespace Qt::StringLiterals;
using namespace satorino;
namespace {

// Native drag/drop completes before observers can rebuild the settings model.
class BadgeKindList : public QListWidget
{
public:
    explicit BadgeKindList(QWidget *parent)
        : QListWidget(parent)
    {
    }
    std::function<void()> afterDrop;

protected:
    void dropEvent(QDropEvent *event) override
    {
        QStringList before;
        for (int i = 0; i < this->count(); ++i)
            before.push_back(this->item(i)->data(Qt::UserRole).toString());
        QListWidget::dropEvent(event);
        QStringList after;
        for (int i = 0; i < this->count(); ++i)
            after.push_back(this->item(i)->data(Qt::UserRole).toString());
        if (event->isAccepted() && before != after && this->afterDrop)
            this->afterDrop();
    }
};

struct PreviewBadge {
    QString kind;
    EmotePtr emote;
    QString tooltip;
};

QString resourceFor(const QString &kind)
{
    for (const auto &info : badgeKinds())
        if (info.key == kind)
            return info.resource;
    return u":/badges/platform-twitch-18.webp"_s;
}

QIcon previewIcon(const PreviewBadge &badge)
{
    // Never request pixels: select only an already decoded image on the GUI thread.
    if (badge.emote)
    {
        const auto &image = badge.emote->images.getImageOrLoadedNoLoad(1.F);
        if (image && image->loaded())
            if (const auto pixmap = image->pixmapOrLoad();
                pixmap && !pixmap->isNull())
                return QIcon(*pixmap);
    }
    return QIcon(resourceFor(badge.kind));
}

bool visible(MessageElementFlags flags, bool exhaustive)
{
    const auto mask = getApp()->getWindows()->getWordFlags();
    const bool supporter = flags.has(MessageElementFlag::BadgeHomiesSupporter);
    const bool custom = flags.has(MessageElementFlag::BadgeHomiesCustom);
    if (supporter || custom)
        return mask.hasAny(flags) &&
               ((supporter &&
                 getSettings()->showBadgesHomiesSupporter.getValue()) ||
                (custom && getSettings()->showBadgesHomiesCustom.getValue()));
    // Same formula as protected MessageElement::matchesFlags, without calling it.
    return exhaustive ? mask.hasAll(flags) : mask.hasAny(flags);
}

MessagePtr lastOwn(const ChannelPtr &channel, const QString &id)
{
    if (!channel)
        return {};
    const auto messages = channel->getMessageSnapshot();
    for (auto it = messages.rbegin(); it != messages.rend(); ++it)
        if ((*it)->platform == MessagePlatform::AnyOrTwitch &&
            badgeOrderOwnAuthor((*it)->userID, id, true))
            return *it;
    return {};
}

std::vector<PreviewBadge> cachedPreview(bool &placeholder)
{
    auto *app = getApp();
    const auto account = app->getAccounts()->twitch.getCurrent();
    placeholder =
        !account || account->isAnon() ||
        (account->getUserId().isEmpty() || account->getUserId() == u"0");
    if (placeholder)
        return {{u"broadcaster"_s, {}, u"Broadcaster (placeholder)"_s},
                {u"moderator"_s, {}, u"Moderator (placeholder)"_s},
                {u"vip"_s, {}, u"VIP (placeholder)"_s}};

    const auto id = account->getUserId();
    std::vector<PreviewBadge> native;
    std::vector<PreviewBadge> providers;
    auto *server = app->getTwitch();
    const auto own = server ? server->getChannelOrEmpty(account->getUserName())
                            : ChannelPtr{};
    const auto ownTwitch = std::dynamic_pointer_cast<TwitchChannel>(own);
    auto known = lastOwn(own, id);
    const bool ownChannelKnown = static_cast<bool>(known);
    if (!known && server)
        server->forEachChannel([&](ChannelPtr channel) {
            if (!known)
                known = lastOwn(channel, id);
        });

    const auto append = [](std::vector<PreviewBadge> &to, const QString &kind,
                           const EmotePtr &emote, const QString &tooltip) {
        to.push_back({kind, emote, tooltip});
    };
    if (known)
        for (const auto &element : known->elements)
        {
            if (element->getFlags().has(MessageElementFlag::RepliedMessage))
                continue;
            if (element->getFlags().hasAny(MessageElementFlag::Username,
                                           MessageElementFlag::KickUsername) &&
                !element->getFlags().has(MessageElementFlag::RepliedMessage))
                break;
            auto *badge = dynamic_cast<BadgeElement *>(element.get());
            if (!badge || !visible(badge->getFlags(), badge->exhaustiveFlags))
                continue;
            const auto kind =
                badgeOrderKind(*badge, MessagePlatform::AnyOrTwitch);
            if (ownChannelKnown &&
                (badge->twitchBadgeSlug() ||
                 badge->type() == ModBadgeElement::TYPE ||
                 badge->type() == VipBadgeElement::TYPE ||
                 badge->getFlags().has(MessageElementFlag::BadgeSharedChannel)))
                append(native, kind, badge->getEmote(), badge->getTooltip());
            else if (!badge->twitchBadgeSlug() &&
                     badge->type() != ModBadgeElement::TYPE &&
                     badge->type() != VipBadgeElement::TYPE)
            {
                // Known providers are read once from current local membership below.
                // Preserve unclassified source entries; never deduplicate real badges.
                if (kind.isEmpty() || kind == u"other")
                    append(providers, kind, badge->getEmote(),
                           badge->getTooltip());
            }
            else if (kind == u"global-authority" || kind == u"twitch-other")
            {
                const auto slug = badge->twitchBadgeSlug();
                const auto version = badge->twitchBadgeVersion();
                if (slug && version && app->getTwitchBadges())
                    if (auto global =
                            app->getTwitchBadges()->badge(*slug, *version))
                        append(native, kind, *global, badge->getTooltip());
            }
        }
    if (visible(MessageElementFlag::BadgeChannelAuthority, false) &&
        std::ranges::none_of(native, [](const PreviewBadge &b) {
            return b.kind == u"broadcaster";
        }))
    {
        EmotePtr broadcaster;
        if (auto *badges = app->getTwitchBadges())
            if (auto cached = badges->badge(u"broadcaster"_s))
                broadcaster = *cached;
        native.insert(native.begin(), {u"broadcaster"_s, broadcaster,
                                       u"Broadcaster (own channel assumed)"_s});
    }
    const auto provider = [&](const QString &kind, const EmotePtr &emote,
                              MessageElementFlags flags) {
        if (emote && visible(flags, false))
            append(providers, kind, emote, emote->tooltip.string);
    };
    if (auto *badges = app->getChatterinoBadges())
        if (auto badge = badges->getBadge(UserId{id}))
            provider(u"chatterino"_s, *badge,
                     MessageElementFlag::BadgeChatterino);
    if (auto *badges = app->getFfzBadges())
        for (const auto &badge : badges->getUserBadges(UserId{id}))
            provider(u"ffz-user"_s, badge.emote, MessageElementFlag::BadgeFfz);
    if (ownTwitch)
        for (const auto &badge : ownTwitch->ffzChannelBadges(id))
            provider(u"ffz-user"_s, badge.emote, MessageElementFlag::BadgeFfz);
    if (auto *badges = app->getBttvBadges())
        if (auto badge = badges->getBadge(UserId{id}))
            provider(u"bttv"_s, *badge, MessageElementFlag::BadgeBttv);
    if (auto *badges = app->getSeventvBadges())
        if (auto badge = badges->getBadge(UserId{id}))
            provider(u"seventv"_s, *badge, MessageElementFlag::BadgeSevenTV);
    // All these getters were inspected: local lookup/materialisation only, never load().
    if (auto *badges = app->getFfzApBadges())
        if (auto badge = badges->getBadge(id))
            provider(u"ffz-ap"_s, badge->emote, MessageElementFlag::BadgeFfzAp);
    if (auto *badges = app->getDankChatBadges())
        if (auto badge = badges->getBadge(id))
            provider(u"dankchat"_s, *badge, MessageElementFlag::BadgeDankChat);
    if (auto *badges = app->getChatsenBadges())
        if (auto badge = badges->getBadge(id))
            provider(u"chatsen"_s, *badge, MessageElementFlag::BadgeChatsen);
    if (auto *badges = app->getBluzyrinoBadges())
        for (const auto &badge : badges->getBadges(UserId{id}))
            provider(u"bluzyrino"_s, badge, MessageElementFlag::BadgeBluzyrino);
    if (auto *badges = app->getJilChatBadges())
        for (const auto &badge : badges->getBadges(UserId{id}))
            provider(u"jilchat"_s, badge, MessageElementFlag::BadgeJilChat);
    if (auto *badges = app->getBadgeHQBadges())
        for (const auto &badge : badges->getBadges(UserId{id}))
            provider(u"badgehq"_s, badge, MessageElementFlag::BadgeBadgeHQ);
    if (auto *badges = app->getLimerinoBadges())
        for (const auto &badge : badges->getBadges(UserId{id}))
            provider(u"limerino"_s, badge, MessageElementFlag::BadgeLimerino);
    if (auto *badges = app->getFolhinhaBadges())
        if (auto badge = badges->getBadge(UserId{id}))
            provider(u"folhinha"_s, *badge, MessageElementFlag::BadgeFolhinha);
    if (auto *badges = app->getHomiesBadges())
    {
        const auto found = badges->getBadges(id);
        for (std::size_t i = 0; i < found.size(); ++i)
            provider(u"homies"_s, found[i],
                     i == 0 ? MessageElementFlag::BadgeHomiesCustom
                            : MessageElementFlag::BadgeHomiesSupporter);
    }
    providers = stableBadgeOrder(
        std::move(providers), makeBadgeOrderRanks(defaultBadgeKindOrder()),
        [](const PreviewBadge &badge) -> const QString & {
            return badge.kind;
        });
    native.insert(native.end(), providers.begin(), providers.end());
    return native;
}
}  // namespace

SatorinoBadgeOrder::SatorinoBadgeOrder(QWidget *parent)
    : QWidget(parent)
{
    using namespace Qt::StringLiterals;
    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    auto *description = new QLabel(
        u"Drag badge kinds or use Up/Down. Reset keeps the exact original message order."_s,
        this);
    description->setWordWrap(true);
    box->addWidget(description);
    this->previewLabel_ = new QLabel(this);
    this->previewLabel_->setWordWrap(true);
    box->addWidget(this->previewLabel_);
    auto *previewRow = new QWidget(this);
    this->preview_ = new QHBoxLayout(previewRow);
    this->preview_->setContentsMargins(0, 0, 0, 0);
    box->addWidget(previewRow);

    // Scope is directly above the reorder list, using the existing settings combo.
    box->addWidget(new QLabel(u"Apply badge order to"_s, this));
    this->scope_ = new ComboBox;
    this->scope_->addItems({u"Everyone"_s, u"Only me"_s});
    this->scope_->setAccessibleName(u"Apply badge order to"_s);
    box->addWidget(this->scope_);
    auto *list = new BadgeKindList(this);
    this->list_ = list;
    this->list_->setAccessibleName(u"Badge kinds in display order"_s);
    this->list_->setDragDropMode(QAbstractItemView::InternalMove);
    this->list_->setDefaultDropAction(Qt::MoveAction);
    this->list_->setSelectionMode(QAbstractItemView::SingleSelection);
    this->list_->setIconSize(QSize(20, 20));
    this->list_->setMinimumHeight(210);
    this->list_->setMaximumHeight(320);
    box->addWidget(this->list_);
    auto *buttons = new QHBoxLayout;
    this->up_ = new QPushButton(u"Up"_s, this);
    this->down_ = new QPushButton(u"Down"_s, this);
    this->up_->setAccessibleName(u"Move badge kind up"_s);
    this->down_->setAccessibleName(u"Move badge kind down"_s);
    auto *reset = new QPushButton(u"Reset to default"_s, this);
    buttons->addWidget(this->up_);
    buttons->addWidget(this->down_);
    buttons->addStretch();
    buttons->addWidget(reset);
    box->addLayout(buttons);
    this->saveTimer_.setSingleShot(true);
    QObject::connect(&this->saveTimer_, &QTimer::timeout, this, [this] {
        this->saveOrder();
    });
    list->afterDrop = [this] {
        if (!this->updating_)
            this->saveTimer_.start(0);
    };
    QObject::connect(this->list_, &QListWidget::currentRowChanged, this,
                     [this] {
                         this->updateButtons();
                     });
    QObject::connect(this->up_, &QPushButton::clicked, this, [this] {
        this->moveSelection(-1);
    });
    QObject::connect(this->down_, &QPushButton::clicked, this, [this] {
        this->moveSelection(1);
    });
    QObject::connect(reset, &QPushButton::clicked, this, [this] {
        this->saveTimer_.stop();
        getSettings()->satorinoBadgeOrder = QStringList{};
        getSettings()->satorinoBadgeOrderScope = u"everyone"_s;
        this->rebuildOrder();
        this->refreshPreview();
    });
    QObject::connect(this->scope_,
                     QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                     [](int index) {
                         getSettings()->satorinoBadgeOrderScope =
                             index == 1 ? u"only-me"_s : u"everyone"_s;
                     });
    this->refreshTimer_.setInterval(2000);
    QObject::connect(&this->refreshTimer_, &QTimer::timeout, this, [this] {
        this->refreshPreview();
    });
    getSettings()->satorinoBadgeOrder.connect(
        [this](const QStringList &) {
            this->rebuildOrder();
            this->refreshPreview();
        },
        this->connections_);
    getSettings()->satorinoBadgeOrderScope.connect(
        [this](const QString &scope) {
            const QSignalBlocker blocker(this->scope_);
            this->scope_->setCurrentIndex(scope == u"only-me" ? 1 : 0);
            this->refreshPreview();
        },
        this->connections_);
    this->rebuildOrder();
    this->refreshPreview();
}

void SatorinoBadgeOrder::rebuildOrder()
{
    this->saveTimer_.stop();
    const auto selected =
        this->list_->currentItem()
            ? this->list_->currentItem()->data(Qt::UserRole).toString()
            : QString{};
    this->updating_ = true;
    this->list_->clear();
    for (const auto &key :
         normalizeBadgeKindOrder(getSettings()->satorinoBadgeOrder.getValue()))
        for (const auto &kind : badgeKinds())
            if (kind.key == key)
            {
                auto *item = new QListWidgetItem(QIcon(kind.resource),
                                                 kind.label, this->list_);
                item->setData(Qt::UserRole, key);
                item->setToolTip(
                    u"Kind: "_s + key +
                    u". Generic icon when no decoded badge image is cached."_s);
                if (key == selected)
                    this->list_->setCurrentItem(item);
                break;
            }
    if (!this->list_->currentItem() && this->list_->count() > 0)
        this->list_->setCurrentRow(0);
    this->updating_ = false;
    this->updateButtons();
}

void SatorinoBadgeOrder::saveOrder()
{
    if (this->updating_)
        return;
    QStringList keys;
    for (int i = 0; i < this->list_->count(); ++i)
        keys.push_back(this->list_->item(i)->data(Qt::UserRole).toString());
    getSettings()->satorinoBadgeOrder = keys;
}

void SatorinoBadgeOrder::moveSelection(int delta)
{
    const auto row = this->list_->currentRow();
    const auto target = row + delta;
    if (row < 0 || target < 0 || target >= this->list_->count())
        return;
    this->saveTimer_.stop();
    this->updating_ = true;
    auto *item = this->list_->takeItem(row);
    this->list_->insertItem(target, item);
    this->list_->setCurrentRow(target);
    this->updating_ = false;
    this->saveOrder();
}

void SatorinoBadgeOrder::updateButtons()
{
    const auto row = this->list_->currentRow();
    this->up_->setEnabled(row > 0);
    this->down_->setEnabled(row >= 0 && row + 1 < this->list_->count());
}

void SatorinoBadgeOrder::refreshPreview()
{
    bool placeholder = false;
    auto badges = cachedPreview(placeholder);
    const auto ranks =
        makeBadgeOrderRanks(getSettings()->satorinoBadgeOrder.getValue());
    // This preview always represents the current user's own badges, regardless of scope.
    badges = stableBadgeOrder(std::move(badges), ranks,
                              [](const PreviewBadge &badge) -> const QString & {
                                  return badge.kind;
                              });
    this->previewLabel_->setText(
        placeholder
            ? u"Placeholder preview — sign in to show cached Twitch badges."_s
            : u"Your Twitch channel preview — cached badges; broadcaster assumed. Uncached images use generic icons."_s);
    this->previewLabel_->setToolTip(
        ranks.custom ? u"Custom badge order"_s
                     : u"Original message order (default)"_s);
    while (auto *item = this->preview_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
    for (const auto &badge : badges)
    {
        auto *label = new QLabel(this);
        label->setPixmap(previewIcon(badge).pixmap(QSize(22, 22)));
        label->setToolTip(badge.tooltip);
        this->preview_->addWidget(label);
    }
    this->preview_->addStretch();
    for (int i = 0; i < this->list_->count(); ++i)
    {
        auto *item = this->list_->item(i);
        const auto key = item->data(Qt::UserRole).toString();
        const auto found =
            std::ranges::find_if(badges, [&](const PreviewBadge &badge) {
                return badge.kind == key;
            });
        item->setIcon(found == badges.end() ? QIcon(resourceFor(key))
                                            : previewIcon(*found));
    }
}

void SatorinoBadgeOrder::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    this->refreshPreview();
    this->refreshTimer_.start();
}
void SatorinoBadgeOrder::hideEvent(QHideEvent *event)
{
    this->refreshTimer_.stop();
    this->saveTimer_.stop();  // no delayed edit after settings Cancel/hide
    QWidget::hideEvent(event);
}
}  // namespace chatterino

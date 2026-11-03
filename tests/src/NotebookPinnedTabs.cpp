// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "common/WindowDescriptors.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "mocks/BaseApplication.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/SplitContainer.hpp"

#include <QApplication>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QStringList>
#include <QTimer>

using namespace chatterino;

namespace {

class PinApplication : public mock::BaseApplication
{
public:
    PinApplication()
        : windows(this->args_, this->paths_, this->settings, this->theme,
                  this->fonts)
    {
    }

    WindowManager *getWindows() override
    {
        return &this->windows;
    }
    HotkeyController *getHotkeys() override
    {
        return &this->hotkeys;
    }

    HotkeyController hotkeys;
    WindowManager windows;
};

class PinNotebook : public Notebook
{
public:
    PinNotebook()
        : Notebook(nullptr)
    {
    }

    using Notebook::sortTabsAlphabetically;
};

class NotebookPins : public ::testing::Test
{
protected:
    NotebookPins()
    {
        this->notebook.setAllowUserTabManagement(true);
        this->notebook.setAttribute(Qt::WA_DontShowOnScreen);
        this->notebook.resize(700, 300);
        this->notebook.show();
    }

    NotebookTab *add(const QString &title, int position = -1)
    {
        auto *page = new SplitContainer(&this->notebook);
        auto *tab = this->notebook.addPageAt(page, position, title);
        page->setTab(tab);
        return tab;
    }

    QStringList order()
    {
        QStringList titles;
        bool foundUnpinned = false;
        for (int i = 0; i < this->notebook.getPageCount(); ++i)
        {
            auto *page =
                static_cast<SplitContainer *>(this->notebook.getPageAt(i));
            auto *tab = page->getTab();
            EXPECT_FALSE(foundUnpinned && tab->isPinned());
            foundUnpinned = foundUnpinned || !tab->isPinned();
            titles.append(tab->getTitle());
        }
        return titles;
    }

    static QAction *action(NotebookTab *tab, const QString &name)
    {
        for (auto *menu : tab->findChildren<QMenu *>())
        {
            for (auto *action : menu->actions())
            {
                if (action->text() == name)
                {
                    return action;
                }
            }
        }
        return nullptr;
    }

    static void prepareMenu(NotebookTab *tab)
    {
        for (auto *menu : tab->findChildren<QMenu *>())
        {
            for (auto *action : menu->actions())
            {
                if (action->text() == "Pin Tab" ||
                    action->text() == "Unpin Tab")
                {
                    EXPECT_TRUE(QMetaObject::invokeMethod(
                        menu, "aboutToShow", Qt::DirectConnection));
                    return;
                }
            }
        }
        FAIL() << "Missing pin menu";
    }

    static void answerConfirmation()
    {
        QTimer::singleShot(0, [] {
            for (auto *widget : QApplication::topLevelWidgets())
            {
                if (auto *box = qobject_cast<QMessageBox *>(widget))
                {
                    box->done(QMessageBox::Yes);
                }
            }
        });
    }

    PinApplication app;
    PinNotebook notebook;
};

}  // namespace

TEST_F(NotebookPins, PinUnpinInsertionAndReorderPreserveGroupOrder)
{
    auto *a = this->add("a");
    auto *b = this->add("b");
    auto *c = this->add("c");
    auto *d = this->add("d");
    c->setPinned(true);
    a->setPinned(true);
    EXPECT_EQ(this->order(), (QStringList{"c", "a", "b", "d"}));
    c->setPinned(false);
    EXPECT_EQ(this->order(), (QStringList{"a", "c", "b", "d"}));
    auto *e = this->add("e", 0);
    EXPECT_FALSE(e->isPinned());
    EXPECT_EQ(this->order(), (QStringList{"a", "e", "c", "b", "d"}));

    c->setPinned(true);
    this->notebook.rearrangePage(d->page, 0);
    EXPECT_EQ(this->order(), (QStringList{"a", "c", "d", "e", "b"}));
    this->notebook.rearrangePage(a->page, 4);
    EXPECT_EQ(this->order(), (QStringList{"c", "a", "d", "e", "b"}));
    this->notebook.sortTabsAlphabetically();
    EXPECT_EQ(this->order(), (QStringList{"a", "c", "b", "d", "e"}));

    this->notebook.setLockNotebookLayout(true);
    prepareMenu(a);
    ASSERT_NE(action(a, "Unpin Tab"), nullptr);
    EXPECT_FALSE(action(a, "Unpin Tab")->isEnabled());
    this->notebook.rearrangePage(a->page, 1);
    EXPECT_EQ(this->order().front(), "a");
    // Programmatic restoration must still work with the layout locked.
    a->setPinned(false);
    EXPECT_EQ(this->order(), (QStringList{"c", "a", "b", "d", "e"}));
    EXPECT_FALSE(b->isPinned());
}

TEST_F(NotebookPins, DuplicateOfPinnedTabIsUnpinnedAtTheBoundary)
{
    auto *a = this->add("a");
    auto *b = this->add("b");
    a->setPinned(true);
    b->setPinned(true);
    this->notebook.duplicatePage(a->page);
    EXPECT_EQ(this->order(), (QStringList{"a", "b", "a"}));
    auto *copy = static_cast<SplitContainer *>(this->notebook.getPageAt(2));
    EXPECT_FALSE(copy->getTab()->isPinned());
}

TEST_F(NotebookPins, UserClosesProtectPinsButProgrammaticRemovalWorks)
{
    auto *tab = this->add("pinned title");
    this->add("other");
    tab->setPinned(true);
    this->notebook.select(tab->page);
    this->notebook.removeCurrentPage();
    this->notebook.requestRemovePage(tab->page);
    EXPECT_EQ(this->notebook.getPageCount(), 2);
    prepareMenu(tab);
    ASSERT_NE(action(tab, "Close Tab"), nullptr);
    EXPECT_FALSE(action(tab, "Close Tab")->isEnabled());

    const QPointF point(tab->width() - 12, 8);
    const auto global = tab->mapToGlobal(point.toPoint());
    for (auto button : {Qt::MiddleButton, Qt::LeftButton})
    {
        answerConfirmation();
        QMouseEvent press(QEvent::MouseButtonPress, point, global, button,
                          button, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, global, button,
                            Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(tab, &press);
        QApplication::sendEvent(tab, &release);
        QApplication::processEvents();
        EXPECT_EQ(this->notebook.getPageCount(), 2);
    }
    this->notebook.removePage(tab->page);
    EXPECT_EQ(this->notebook.getPageCount(), 1);
}

TEST_F(NotebookPins, BulkCloseCountsAndTargetsExcludePinnedTabs)
{
    auto *pinned = this->add("pin");
    pinned->setPinned(true);
    auto *a = this->add("a");
    this->add("b");
    prepareMenu(a);
    ASSERT_NE(action(a, "Close Visible Tabs to Left"), nullptr);
    EXPECT_FALSE(action(a, "Close Visible Tabs to Left")->isEnabled());
    EXPECT_TRUE(action(a, "Close Visible Tabs to Right")->isEnabled());
    prepareMenu(pinned);
    auto *closeOthers = action(pinned, "Close Other Visible Tabs");
    ASSERT_NE(closeOthers, nullptr);
    ASSERT_TRUE(closeOthers->isEnabled());
    answerConfirmation();
    closeOthers->trigger();
    EXPECT_EQ(this->order(), (QStringList{"pin"}));
    prepareMenu(pinned);
    EXPECT_FALSE(action(pinned, "Close All Visible Tabs")->isEnabled());

    this->add("new");
    prepareMenu(pinned);
    auto *closeAll = action(pinned, "Close All Visible Tabs");
    ASSERT_NE(closeAll, nullptr);
    ASSERT_TRUE(closeAll->isEnabled());
    answerConfirmation();
    closeAll->trigger();
    EXPECT_EQ(this->order(), (QStringList{"pin"}));
}

TEST_F(NotebookPins, JsonCodecOmitsFalseAndRestoresPinState)
{
    auto *tab = this->add("saved");
    auto *page = static_cast<SplitContainer *>(tab->page);
    QJsonObject unpinned;
    WindowManager::encodeTab(page, true, unpinned);
    EXPECT_FALSE(unpinned.contains("pinned"));
    EXPECT_FALSE(TabDescriptor::loadFromJSON(unpinned).pinned_);

    tab->setPinned(true);
    QJsonObject pinned;
    WindowManager::encodeTab(page, true, pinned);
    const auto json = QJsonDocument(pinned).toJson();
    const auto descriptor =
        TabDescriptor::loadFromJSON(QJsonDocument::fromJson(json).object());
    EXPECT_TRUE(descriptor.pinned_);
    EXPECT_EQ(descriptor.customTitle_, "saved");
    tab->setPinned(false);
    this->add("second")->setPinned(true);
    tab->setPinned(descriptor.pinned_);
    EXPECT_EQ(this->order(), (QStringList{"second", "saved"}));

    pinned.insert("pinned", "true");
    EXPECT_FALSE(TabDescriptor::loadFromJSON(pinned).pinned_);
}

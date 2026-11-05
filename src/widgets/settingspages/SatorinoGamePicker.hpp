// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoGames.hpp"

#include <boost/signals2/connection.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QCache>
#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <deque>
#include <map>
#include <utility>
#include <vector>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace chatterino {
class SatorinoGamePicker : public QWidget
{
public:
    explicit SatorinoGamePicker(QWidget *parent = nullptr);

private:
    void editSearch();
    void resetSearchAfterAdd();
    void search();
    void showResults(const satorino::GameList &games);
    void showSelected();
    void updateAddButtons();
    void save();
    void thumbnail(const QString &url, QLabel *label);
    void pumpThumbnails();
    void finishThumbnail(const QString &url, const QImage &image);

    QLineEdit *query_;
    QLabel *status_;
    QLabel *selectionStatus_;
    QScrollArea *results_;
    QScrollArea *selectedView_;
    QVBoxLayout *resultRows_;
    QVBoxLayout *selectedRows_;
    QTimer debounce_;
    QElapsedTimer clock_;
    satorino::GameSearchState searchState_;
    bool inFlight_ = false;
    satorino::GameList selected_;
    std::vector<std::pair<satorino::HighlightedGame, QPointer<QPushButton>>>
        addButtons_;
    QCache<QString, QPixmap> thumbnailCache_{96};
    std::map<QString, std::vector<QPointer<QLabel>>> thumbnailWaiters_;
    std::deque<QString> thumbnailQueue_;
    int activeThumbnails_ = 0;
    pajlada::Signals::SignalHolder connections_;
    boost::signals2::scoped_connection accountConnection_;
};
}  // namespace chatterino

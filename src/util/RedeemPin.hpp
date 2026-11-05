// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QRectF>
#include <QString>

class QPainter;
namespace chatterino::satorino {
QString redeemChannelKey(const QString &primaryId, const QString &fallbackId);
QString redeemPinTooltip(bool pinned, bool channelReady, bool rewardReady,
                         bool failed = false);
QRectF redeemStarRect(const QRectF &image);
void paintRedeemStar(QPainter &painter, const QRectF &image);
}  // namespace chatterino::satorino

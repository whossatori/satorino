// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/RedeemPin.hpp"

#include "util/SatorinoIndicator.hpp"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cstddef>

namespace chatterino::satorino {
namespace {
bool channelIdValid(const QString &id)
{
    return !id.isEmpty() && id.size() <= 32 && id != "0" &&
           std::all_of(id.begin(), id.end(), [](QChar c) {
               return c >= '0' && c <= '9';
           });
}
}  // namespace
QString redeemChannelKey(const QString &primaryId, const QString &fallbackId)
{
    const auto primary = primaryId.trimmed();
    const auto fallback = fallbackId.trimmed();
    return channelIdValid(primary)    ? primary
           : channelIdValid(fallback) ? fallback
                                      : QString{};
}
QString redeemPinTooltip(bool pinned, bool channelReady, bool rewardReady,
                         bool failed)
{
    if (!channelReady)
        return "Can't pin: channel not loaded yet";
    if (!rewardReady)
        return "Can't pin: reward has no stable ID";
    if (failed)
        return "Can't pin: pin limit reached or settings unavailable";
    return pinned ? "Unpin redeem" : "Pin redeem";
}
QRectF redeemStarRect(const QRectF &image)
{
    if (!image.isValid())
        return {};
    const qreal margin = image.width() * 0.04;
    const qreal size = std::max(
        qreal(0), std::min(image.width() * 0.19, image.height() - 2 * margin));
    return QRectF(image.right() - margin - size, image.top() + margin, size,
                  size);
}
void paintRedeemStar(QPainter &painter, const QRectF &image)
{
    const auto bounds = redeemStarRect(image);
    if (bounds.isEmpty())
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal width = std::min(bounds.width() / 4,
                                 std::max(qreal(0.7), bounds.width() * 0.07));
    QPen pen(QColor("#29200a"));
    pen.setWidthF(width);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(QColor("#ffd23f"));
    const auto points =
        indicatorStar(bounds.center(), (bounds.width() - width) / 2, 0.5);
    QPainterPath path;
    path.moveTo(points.front());
    for (std::size_t i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    path.closeSubpath();
    painter.drawPath(path);
    painter.restore();
}
}  // namespace chatterino::satorino

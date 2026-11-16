#include "providers/seventv/paints/Paint.hpp"

#include "Application.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"

#include <private/qpixmapfilter_p.h>
#include <QCache>
#include <QCoreApplication>
#include <QFontMetricsF>
#include <QHashFunctions>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace chatterino {

using namespace Qt::Literals::StringLiterals;

namespace {

// satorino: share rendered username pixmaps across messages and loop cycles.
struct PaintFrameKey {
    QString paintId;
    qint64 frame;
    QString text;
    QFont font;
    QColor userColor;
    QColor colonColor;
    QSizeF size;
    float scale;
    float dpr;
    bool shadows;
    bool largeShadows;

    bool operator==(const PaintFrameKey &) const = default;
};

size_t qHash(const PaintFrameKey &key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.paintId, key.frame, key.text, key.font,
                      key.userColor.rgba(), key.colonColor.rgba(),
                      key.size.width(), key.size.height(), key.scale, key.dpr,
                      key.shadows, key.largeShadows);
}

QCache<PaintFrameKey, QPixmap> *paintFrames = nullptr;

void clearPaintFrames()
{
    delete paintFrames;
    paintFrames = nullptr;
}

QCache<PaintFrameKey, QPixmap> &paintFrameCache()
{
    constexpr int CACHE_KIB = 8 * 1024;
    if (paintFrames == nullptr)
    {
        paintFrames = new QCache<PaintFrameKey, QPixmap>(CACHE_KIB);
        qAddPostRoutine(clearPaintFrames);
    }
    return *paintFrames;
}

qreal textBaseline(const QFont &font, const QRectF &rect)
{
    const QFontMetricsF metrics(font);

    return rect.bottom() - metrics.descent();
}

}  // namespace

QPixmap Paint::getPixmap(const QString &text, const QFont &font,
                         QColor userColor, QSizeF size, float scale, float dpr,
                         bool centerVertically) const
{
    const QColor colonColor =
        text.endsWith(u':') ? getApp()->getThemes()->messages.textColors.regular
                            : QColor{};
    const auto frame = centerVertically ? std::nullopt : this->frameCacheKey();
    if (!frame)
    {
        return this->renderPixmap(text, font, userColor, colonColor, size,
                                  scale, dpr, centerVertically);
    }

    const auto *settings = getSettings();
    const bool shadows = settings->displaySevenTVPaintShadows;
    const PaintFrameKey key{
        .paintId = this->id,
        .frame = *frame,
        .text = text,
        .font = font,
        .userColor = userColor,
        .colonColor = colonColor,
        .size = size,
        .scale = scale,
        .dpr = dpr,
        .shadows = shadows,
        .largeShadows = shadows && settings->largeSevenTVPaintShadows,
    };
    auto &cache = paintFrameCache();
    if (const auto *pixmap = cache.object(key))
    {
        return *pixmap;
    }

    const auto pixmap = this->renderPixmap(text, font, userColor, colonColor,
                                           size, scale, dpr, centerVertically);
    constexpr qint64 BITS_PER_KIB = 8 * 1024;
    const auto bits = qint64(pixmap.width()) * pixmap.height() * pixmap.depth();
    const auto cost = (bits + BITS_PER_KIB - 1) / BITS_PER_KIB;
    if (cost > 0 && cost <= cache.maxCost())
    {
        cache.insert(key, new QPixmap(pixmap), static_cast<qsizetype>(cost));
    }
    return pixmap;
}

QPixmap Paint::renderPixmap(const QString &text, const QFont &font,
                            QColor userColor, QColor colonColor, QSizeF size,
                            float scale, float dpr, bool centerVertically) const
{
    QSizeF drawSize = size;
    if (centerVertically && getSettings()->displaySevenTVPaintShadows)
    {
        float shadowExtent = 0;
        for (const auto &shadow : this->getDropShadows())
        {
            if (!shadow.isValid())
            {
                continue;
            }

            shadowExtent = std::max(shadowExtent,
                                    shadow.scaled(scale / dpr).extentBelow());
        }

        drawSize.setHeight(size.height() + shadowExtent);
    }

    QPixmap pixmap((drawSize * dpr).toSize());
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter pixmapPainter(&pixmap);
    pixmapPainter.setRenderHint(QPainter::SmoothPixmapTransform);
    pixmapPainter.setFont(font);

    const QRectF pixmapRect(QPointF{}, drawSize);
    const QRectF textRect(QPointF{}, size);

    // NOTE: draw colon separately from the nametag
    // otherwise the paint would extend onto the colon
    bool drawColon = false;
    QRectF nametagBoundingRect = centerVertically ? textRect : pixmapRect;
    QString nametagText = text;
    if (nametagText.endsWith(':'))
    {
        drawColon = true;
        nametagText = nametagText.chopped(1);
        const auto textBounds = pixmapPainter.boundingRect(
            QRectF(0, 0, 10000, 10000), nametagText,
            QTextOption(Qt::AlignLeft | Qt::AlignTop));
        nametagBoundingRect =
            QRectF(0, 0, textBounds.width(),
                   centerVertically ? textRect.height() : pixmapRect.height());
    }

    QPen pen;
    const QBrush brush = this->asBrush(userColor, nametagBoundingRect);
    pen.setBrush(brush);
    pixmapPainter.setPen(pen);

    if (centerVertically)
    {
        // satorino: the user card allocates one line for the paint name.
        const int flags =
            int(Qt::AlignLeft | Qt::AlignVCenter) | Qt::TextSingleLine;
        pixmapPainter.drawText(nametagBoundingRect, flags, nametagText);
    }
    else
    {
        const auto baseline = textBaseline(font, nametagBoundingRect);
        pixmapPainter.drawText(QPointF(nametagBoundingRect.left(), baseline),
                               nametagText);
    }
    pixmapPainter.end();

    if (!this->getDropShadows().empty() &&
        getSettings()->displaySevenTVPaintShadows)
    {
        QPixmap outMap((drawSize * dpr).toSize());
        outMap.setDevicePixelRatio(dpr);
        for (const auto &shadow : this->getDropShadows())
        {
            if (!shadow.isValid())
            {
                continue;
            }
            outMap.fill(Qt::transparent);

            {
                QPainter outPainter(&outMap);
                auto scaled = shadow.scaled(
                    scale / static_cast<float>(outMap.devicePixelRatio()));

                QPixmapDropShadowFilter filter;
                scaled.apply(filter);
                filter.draw(&outPainter, {0, 0}, pixmap);
            }
            outMap.swap(pixmap);
        }
    }

    if (drawColon)
    {
        const auto baseline = textBaseline(font, nametagBoundingRect);

        pixmapPainter.begin(&pixmap);

        pixmapPainter.setPen(QPen(colonColor));
        pixmapPainter.setFont(font);

        pixmapPainter.drawText(QPointF(nametagBoundingRect.right(), baseline),
                               u":"_s);
        pixmapPainter.end();
    }

    return pixmap;
}

QColor Paint::overlayColors(QColor background, QColor foreground)
{
    auto alpha = foreground.alphaF();

    auto r = ((1 - alpha) * static_cast<float>(background.red())) +
             (alpha * static_cast<float>(foreground.red()));
    auto g = ((1 - alpha) * static_cast<float>(background.green())) +
             (alpha * static_cast<float>(foreground.green()));
    auto b = ((1 - alpha) * static_cast<float>(background.blue())) +
             (alpha * static_cast<float>(foreground.blue()));

    return {static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)};
}

qreal Paint::offsetRepeatingStopPosition(const qreal position,
                                         const QGradientStops &stops)
{
    const qreal gradientStart = stops.first().first;
    const qreal gradientEnd = stops.last().first;
    const qreal gradientLength = gradientEnd - gradientStart;
    const qreal offsetPosition = (position - gradientStart) / gradientLength;

    return offsetPosition;
}

}  // namespace chatterino

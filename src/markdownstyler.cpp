#include "markdownstyler.h"
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QVariantMap>

MarkdownStyler::MarkdownStyler(QObject *parent)
    : QObject(parent)
{
}

QVariantList MarkdownStyler::regions() const
{
    return m_regions;
}

void MarkdownStyler::styleDocument(QQuickTextDocument *doc)
{
    if (!doc || !doc->textDocument())
        return;

    QTimer::singleShot(0, this, [this, doc]() {
        QTextDocument *tdoc = doc->textDocument();
        if (!tdoc)
            return;

        QVariantList regions;

        // Group consecutive blocks at the same quote level into regions
        int regionStart = -1;
        int regionLevel = 0;
        qreal regionY = 0;
        qreal regionBottom = 0;

        QTextBlock block = tdoc->begin();
        while (block.isValid()) {
            int level = block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
            QTextLayout *layout = block.layout();

            if (level > 0 && layout) {
                QRectF blockRect = layout->boundingRect();
                blockRect.translate(layout->position());

                if (regionStart < 0 || level != regionLevel) {
                    // Flush previous region
                    if (regionStart >= 0) {
                        QVariantMap r;
                        r["y"] = regionY;
                        r["height"] = regionBottom - regionY;
                        r["level"] = regionLevel;
                        regions.append(r);
                    }
                    regionStart = block.blockNumber();
                    regionLevel = level;
                    regionY = blockRect.y();
                }
                regionBottom = blockRect.y() + blockRect.height();
            } else {
                // Flush region on non-quote block
                if (regionStart >= 0) {
                    QVariantMap r;
                    r["y"] = regionY;
                    r["height"] = regionBottom - regionY;
                    r["level"] = regionLevel;
                    regions.append(r);
                    regionStart = -1;
                }
            }
            block = block.next();
        }
        // Flush final region
        if (regionStart >= 0) {
            QVariantMap r;
            r["y"] = regionY;
            r["height"] = regionBottom - regionY;
            r["level"] = regionLevel;
            regions.append(r);
        }

        m_regions = regions;
        emit regionsChanged();
    });
}

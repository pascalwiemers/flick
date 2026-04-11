#include "listengine.h"
#include <QStringList>

ListEngine::ListEngine(QObject *parent)
    : QObject(parent)
{
}

bool ListEngine::active() const { return m_active; }
QString ListEngine::title() const { return m_title; }
QVariantList ListEngine::items() const { return m_items; }

void ListEngine::evaluate(const QString &text)
{
    QVariantList newItems;
    bool newActive = false;
    QString newTitle;

    QStringList lines = text.split('\n');
    if (lines.isEmpty()) {
        if (m_active != newActive || m_items != newItems) {
            m_active = newActive;
            m_title = newTitle;
            m_items = newItems;
            emit itemsChanged();
        }
        return;
    }

    // Check first line for "list" or "list: Title"
    QString first = lines[0].trimmed();
    if (first.compare("list", Qt::CaseInsensitive) == 0) {
        newActive = true;
    } else if (first.startsWith("list:", Qt::CaseInsensitive)) {
        newActive = true;
        newTitle = first.mid(5).trimmed();
    }

    if (!newActive) {
        if (m_active != newActive) {
            m_active = false;
            m_title.clear();
            m_items.clear();
            emit itemsChanged();
        }
        return;
    }

    // Parse remaining lines (skip line 0 which is the "list" trigger)
    for (int i = 1; i < lines.size(); i++) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        QVariantMap item;
        item["line"] = i;

        if (trimmed.isEmpty()) {
            item["type"] = "empty";
            newItems.append(item);
            continue;
        }

        // Comments: lines starting with //
        if (trimmed.startsWith("//")) {
            item["type"] = "comment";
            newItems.append(item);
            continue;
        }

        // Headings: #, ##, ###
        if (trimmed.startsWith("###")) {
            item["type"] = "heading";
            item["level"] = 3;
            newItems.append(item);
            continue;
        }
        if (trimmed.startsWith("##")) {
            item["type"] = "heading";
            item["level"] = 2;
            newItems.append(item);
            continue;
        }
        if (trimmed.startsWith("#")) {
            item["type"] = "heading";
            item["level"] = 1;
            newItems.append(item);
            continue;
        }

        // Regular list item — check if it ends with /x (checked)
        bool checked = trimmed.endsWith("/x") || trimmed.endsWith("/X");
        item["type"] = "item";
        item["checked"] = checked;
        newItems.append(item);
    }

    if (m_active != newActive || m_title != newTitle || m_items != newItems) {
        m_active = newActive;
        m_title = newTitle;
        m_items = newItems;
        emit itemsChanged();
    }
}

QString ListEngine::toggleCheck(const QString &text, int lineIndex)
{
    QStringList lines = text.split('\n');
    if (lineIndex < 0 || lineIndex >= lines.size())
        return text;

    QString line = lines[lineIndex];
    QString trimmed = line.trimmed();

    // Skip non-item lines
    if (trimmed.isEmpty() || trimmed.startsWith("//") || trimmed.startsWith("#"))
        return text;

    if (trimmed.endsWith("/x") || trimmed.endsWith("/X")) {
        // Uncheck: remove trailing /x (and any preceding space)
        int idx = line.lastIndexOf("/x", -1, Qt::CaseInsensitive);
        if (idx >= 0) {
            // Also remove one space before /x if present
            if (idx > 0 && line[idx - 1] == ' ')
                idx--;
            lines[lineIndex] = line.left(idx);
        }
    } else {
        // Check: append /x
        lines[lineIndex] = line + " /x";
    }

    return lines.join('\n');
}

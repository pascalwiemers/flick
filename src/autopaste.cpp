#include "autopaste.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>

AutoPaste::AutoPaste(NoteStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    m_debounceTimer.start();

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
            this, &AutoPaste::onClipboardChanged);
}

bool AutoPaste::active() const
{
    return m_active;
}

void AutoPaste::setActive(bool on)
{
    if (m_active == on)
        return;
    m_active = on;
    if (m_active) {
        // Reset state when turning on so we don't immediately capture
        // whatever is currently on the clipboard
        m_lastCaptured = QGuiApplication::clipboard()->text();
        m_debounceTimer.restart();
    }
    emit activeChanged();
}

void AutoPaste::onClipboardChanged()
{
    if (!m_active)
        return;

    // Debounce: minimum 200ms between captures
    if (m_debounceTimer.elapsed() < 200)
        return;
    m_debounceTimer.restart();

    QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mime = clipboard->mimeData();

    // Only capture plain text
    if (!mime || !mime->hasText())
        return;

    QString text = clipboard->text();

    // Anti-spam guards
    if (text.isEmpty())
        return;
    if (text.length() > 10000)
        return;
    if (text == m_lastCaptured)
        return;

    m_lastCaptured = text;
    m_store->appendText(text);
}

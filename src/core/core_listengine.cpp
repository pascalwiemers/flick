#include "core_listengine.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace flick {

static void notify(const std::function<void()> &fn) {
    if (fn) fn();
}

static std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool startsWithCI(const std::string &s, const std::string &prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

static bool endsWithCI(const std::string &s, const std::string &suffix) {
    if (s.size() < suffix.size()) return false;
    size_t offset = s.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower((unsigned char)s[offset + i]) != std::tolower((unsigned char)suffix[i]))
            return false;
    }
    return true;
}

static std::vector<std::string> splitLines(const std::string &s) {
    std::vector<std::string> lines;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line))
        lines.push_back(line);
    if (!s.empty() && s.back() == '\n')
        lines.emplace_back();
    return lines;
}

ListEngine::ListEngine() {}

bool ListEngine::active() const { return m_active; }
const std::string &ListEngine::title() const { return m_title; }
const std::vector<ListItem> &ListEngine::items() const { return m_items; }

void ListEngine::evaluate(const std::string &text)
{
    std::vector<ListItem> newItems;
    bool newActive = false;
    std::string newTitle;

    auto lines = splitLines(text);
    if (lines.empty()) {
        if (m_active != newActive || m_items != newItems) {
            m_active = newActive;
            m_title = newTitle;
            m_items = newItems;
            notify(onItemsChanged);
        }
        return;
    }

    std::string first = trim(lines[0]);
    if (startsWithCI(first, "list:")) {
        newActive = true;
        newTitle = trim(first.substr(5));
    } else if (first.size() == 4 && startsWithCI(first, "list")) {
        newActive = true;
    }

    if (!newActive) {
        if (m_active != newActive) {
            m_active = false;
            m_title.clear();
            m_items.clear();
            notify(onItemsChanged);
        }
        return;
    }

    for (int i = 1; i < (int)lines.size(); ++i) {
        std::string trimmed = trim(lines[i]);
        ListItem item;
        item.line = i;

        if (trimmed.empty()) {
            item.type = "empty";
            newItems.push_back(item);
            continue;
        }
        if (trimmed.substr(0, 2) == "//") {
            item.type = "comment";
            newItems.push_back(item);
            continue;
        }
        if (trimmed.substr(0, 3) == "###") {
            item.type = "heading"; item.level = 3;
            newItems.push_back(item);
            continue;
        }
        if (trimmed.substr(0, 2) == "##") {
            item.type = "heading"; item.level = 2;
            newItems.push_back(item);
            continue;
        }
        if (trimmed[0] == '#') {
            item.type = "heading"; item.level = 1;
            newItems.push_back(item);
            continue;
        }

        item.type = "item";
        item.checked = endsWithCI(trimmed, "/x");
        newItems.push_back(item);
    }

    bool changed = (m_active != newActive || m_title != newTitle || m_items != newItems);
    if (changed) {
        m_active = newActive;
        m_title = newTitle;
        m_items = std::move(newItems);
        notify(onItemsChanged);
    }
}

std::string ListEngine::toggleCheck(const std::string &text, int lineIndex)
{
    auto lines = splitLines(text);
    if (lineIndex < 0 || lineIndex >= (int)lines.size())
        return text;

    std::string &line = lines[lineIndex];
    std::string trimmed = trim(line);

    if (trimmed.empty() || trimmed.substr(0, 2) == "//" || trimmed[0] == '#')
        return text;

    if (endsWithCI(trimmed, "/x")) {
        // Find /x in the original line and remove it (plus preceding space)
        size_t len = line.size();
        // Search backwards for /x or /X
        for (size_t i = len; i >= 2; --i) {
            if ((line[i - 2] == '/') && (line[i - 1] == 'x' || line[i - 1] == 'X')) {
                size_t removeStart = i - 2;
                if (removeStart > 0 && line[removeStart - 1] == ' ')
                    removeStart--;
                line = line.substr(0, removeStart);
                break;
            }
        }
    } else {
        line += " /x";
    }

    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += '\n';
        result += lines[i];
    }
    return result;
}

// Comparison operator for change detection
bool operator==(const ListItem &a, const ListItem &b) {
    return a.line == b.line && a.type == b.type && a.level == b.level && a.checked == b.checked;
}
bool operator!=(const ListItem &a, const ListItem &b) { return !(a == b); }

} // namespace flick

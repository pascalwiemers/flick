#pragma once

#include <string>
#include <vector>
#include <functional>

namespace flick {

struct ListItem {
    int line = 0;
    std::string type; // "empty", "comment", "heading", "item"
    int level = 0;    // heading level (1-3)
    bool checked = false;
};

class ListEngine {
public:
    ListEngine();

    bool active() const;
    const std::string &title() const;
    const std::vector<ListItem> &items() const;

    void evaluate(const std::string &text);
    std::string toggleCheck(const std::string &text, int lineIndex);

    std::function<void()> onItemsChanged;

private:
    bool m_active = false;
    std::string m_title;
    std::vector<ListItem> m_items;
};

} // namespace flick

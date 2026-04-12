#pragma once

#include <string>
#include <vector>
#include <functional>

namespace flick {

struct StatsResult {
    int items = 0;
    int words = 0;
    int characters = 0;
    int sentences = 0;
    double fleschReadingEase = 0;
    double fleschKincaidGrade = 0;
};

class StatsEngine {
public:
    StatsEngine();

    bool active() const;
    const StatsResult &result() const;
    void evaluate(const std::string &text);

    std::function<void()> onResultChanged;

private:
    bool m_active = false;
    StatsResult m_result;

    static int countSyllables(const std::string &word);
};

} // namespace flick

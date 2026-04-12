#include "core_statsengine.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

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

static std::string toLower(const std::string &s) {
    std::string r = s;
    for (auto &c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static bool startsWithCI(const std::string &s, const std::string &prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

StatsEngine::StatsEngine() {}

bool StatsEngine::active() const { return m_active; }
const StatsResult &StatsEngine::result() const { return m_result; }

int StatsEngine::countSyllables(const std::string &word)
{
    if (word.empty()) return 0;
    std::string w = toLower(word);

    int count = 0;
    bool prevVowel = false;
    std::string vowels = "aeiouy";

    for (size_t i = 0; i < w.size(); ++i) {
        bool isVowel = vowels.find(w[i]) != std::string::npos;
        if (isVowel && !prevVowel)
            count++;
        prevVowel = isVowel;
    }

    // Silent e at end
    if (w.size() >= 2 && w.back() == 'e' && count > 1)
        count--;

    return std::max(1, count);
}

void StatsEngine::evaluate(const std::string &text)
{
    // Check for "stats:" prefix on first line
    std::istringstream iss(text);
    std::string firstLine;
    std::getline(iss, firstLine);
    std::string trimmedFirst = trim(firstLine);

    bool newActive = startsWithCI(trimmedFirst, "stats:");
    if (trimmedFirst.size() == 5 && startsWithCI(trimmedFirst, "stats"))
        newActive = true;

    if (!newActive) {
        if (m_active) {
            m_active = false;
            m_result = {};
            notify(onResultChanged);
        }
        return;
    }

    // Get body text (everything after first line)
    std::string body;
    std::string line;
    while (std::getline(iss, line)) {
        if (!body.empty()) body += '\n';
        body += line;
    }

    StatsResult r;

    // Items: count non-empty lines
    {
        std::istringstream bodyStream(body);
        std::string l;
        while (std::getline(bodyStream, l)) {
            if (!trim(l).empty())
                r.items++;
        }
    }

    // Characters (excluding leading/trailing whitespace of body)
    std::string trimmedBody = trim(body);
    r.characters = (int)trimmedBody.size();

    // Words and syllables
    std::vector<std::string> words;
    {
        std::istringstream ws(trimmedBody);
        std::string w;
        while (ws >> w) {
            // Strip punctuation from ends
            while (!w.empty() && std::ispunct((unsigned char)w.back()))
                w.pop_back();
            while (!w.empty() && std::ispunct((unsigned char)w.front()))
                w.erase(w.begin());
            if (!w.empty())
                words.push_back(w);
        }
    }
    r.words = (int)words.size();

    // Sentences: count . ! ?
    r.sentences = 0;
    for (char c : trimmedBody) {
        if (c == '.' || c == '!' || c == '?')
            r.sentences++;
    }
    if (r.sentences == 0 && r.words > 0)
        r.sentences = 1;

    // Flesch scores
    if (r.words > 0 && r.sentences > 0) {
        int totalSyllables = 0;
        for (auto &w : words)
            totalSyllables += countSyllables(w);

        double wordsPerSentence = (double)r.words / (double)r.sentences;
        double syllablesPerWord = (double)totalSyllables / (double)r.words;

        r.fleschReadingEase = 206.835 - 1.015 * wordsPerSentence - 84.6 * syllablesPerWord;
        r.fleschKincaidGrade = 0.39 * wordsPerSentence + 11.8 * syllablesPerWord - 15.59;

        // Clamp reading ease to 0-100ish range
        r.fleschReadingEase = std::round(r.fleschReadingEase * 100.0) / 100.0;
        r.fleschKincaidGrade = std::round(r.fleschKincaidGrade * 100.0) / 100.0;
    }

    bool changed = (m_active != newActive ||
                    r.items != m_result.items ||
                    r.words != m_result.words ||
                    r.characters != m_result.characters ||
                    r.sentences != m_result.sentences ||
                    r.fleschReadingEase != m_result.fleschReadingEase ||
                    r.fleschKincaidGrade != m_result.fleschKincaidGrade);

    m_active = newActive;
    m_result = r;
    if (changed) notify(onResultChanged);
}

} // namespace flick

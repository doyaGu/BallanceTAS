#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

/**
 * @class HudWatches
 * @brief Values scripts show on the HUD (tas.hud.set), in the order they were first set.
 *
 * Each entry belongs to the script context that set it, so a context's entries go away
 * with the context.
 */
class HudWatches {
public:
    struct Entry {
        std::string owner;
        std::string name;
        std::string value;
    };

    void Set(const std::string &owner, const std::string &name, std::string value) {
        auto it = Find(owner, name);
        if (it != m_Entries.end()) {
            it->value = std::move(value);
        } else {
            m_Entries.push_back({owner, name, std::move(value)});
        }
    }

    void Clear(const std::string &owner, const std::string &name) {
        auto it = Find(owner, name);
        if (it != m_Entries.end()) {
            m_Entries.erase(it);
        }
    }

    void ClearOwner(const std::string &owner) {
        m_Entries.erase(std::remove_if(m_Entries.begin(), m_Entries.end(),
                                       [&](const Entry &entry) { return entry.owner == owner; }),
                        m_Entries.end());
    }

    const std::vector<Entry> &Entries() const { return m_Entries; }

private:
    std::vector<Entry>::iterator Find(const std::string &owner, const std::string &name) {
        return std::find_if(m_Entries.begin(), m_Entries.end(), [&](const Entry &entry) {
            return entry.owner == owner && entry.name == name;
        });
    }

    std::vector<Entry> m_Entries;
};

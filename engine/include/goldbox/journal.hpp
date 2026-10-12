#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace goldbox {

using JournalId = std::uint32_t;

struct JournalEntry {
    JournalId id;
    std::string title;
    std::string body;

    bool operator==(const JournalEntry&) const = default;
};

// Only data overrides are represented here. Scripts are a later, separate layer.
struct JournalPatch {
    std::string mod_id;
    JournalId id;
    int priority;
    std::optional<std::string> title;
    std::optional<std::string> body;
    // Loader-assigned order incorporates dependency constraints.
    std::optional<std::size_t> apply_order;
};

class Journal {
public:
    void add_base(JournalEntry entry);
    void add_patch(JournalPatch patch);
    [[nodiscard]] std::optional<JournalEntry> resolve(JournalId id) const;

private:
    std::unordered_map<JournalId, JournalEntry> base_;
    std::vector<JournalPatch> patches_;
};

} // namespace goldbox

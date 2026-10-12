#include "goldbox/journal.hpp"

#include <algorithm>
#include <stdexcept>

namespace goldbox {

void Journal::add_base(JournalEntry entry) {
    const auto [it, inserted] = base_.emplace(entry.id, std::move(entry));
    if (!inserted) {
        throw std::invalid_argument("duplicate base journal entry id");
    }
}

void Journal::add_patch(JournalPatch patch) {
    if (patch.mod_id.empty()) {
        throw std::invalid_argument("mod_id must not be empty");
    }
    if (!base_.contains(patch.id)) {
        throw std::invalid_argument("patch targets missing journal entry " + std::to_string(patch.id));
    }
    patches_.push_back(std::move(patch));
}

std::optional<JournalEntry> Journal::resolve(JournalId id) const {
    const auto base_it = base_.find(id);
    if (base_it == base_.end()) {
        // Initial policy: overrides may not invent original journal IDs.
        return std::nullopt;
    }

    JournalEntry result = base_it->second;
    std::vector<const JournalPatch*> matches;
    for (const auto& patch : patches_) {
        if (patch.id == id) {
            matches.push_back(&patch);
        }
    }

    // Mod priority wins, then stable mod ID as a documented tie-breaker.
    std::stable_sort(matches.begin(), matches.end(), [](const auto* a, const auto* b) {
        if (a->apply_order && b->apply_order) {
            return *a->apply_order < *b->apply_order;
        }
        if (a->priority != b->priority) {
            return a->priority < b->priority;
        }
        return a->mod_id < b->mod_id;
    });

    for (const auto* patch : matches) {
        if (patch->title) {
            result.title = *patch->title;
        }
        if (patch->body) {
            result.body = *patch->body;
        }
    }
    return result;
}

} // namespace goldbox

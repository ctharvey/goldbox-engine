#pragma once

#include "goldbox/content.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace goldbox {

struct ModManifest {
    std::string id;
    std::string version;
    int priority = 0;
    std::vector<std::string> requires_mods;
    std::vector<JournalPatch> journal_patches;
};

struct ModResolution {
    std::vector<ModManifest> ordered;
    std::vector<std::string> warnings;
};

// Mod files are read from direct, non-symlinked subdirectories of mods_root.
// File sizes, manifest fields and ids are validated before anything is applied.
[[nodiscard]] std::vector<ModManifest> discover_mods(const std::filesystem::path& mods_root);
[[nodiscard]] ModResolution resolve_mods(std::vector<ModManifest> mods, const ImportedContent& content);
[[nodiscard]] Journal build_journal(const ImportedContent& content, const ModResolution& mods, bool enhanced);

// Save metadata is intentionally not a full campaign save file.
[[nodiscard]] std::string make_session_metadata(const ImportedContent& content, const ModResolution& mods,
                                                bool enhanced);

} // namespace goldbox

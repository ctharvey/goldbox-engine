#pragma once

#include "goldbox/journal.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace goldbox {

struct ImportedContent {
    std::string game_id;
    std::string source_format;
    std::vector<JournalEntry> journals;
};

// Deliberately independent of any SSI binary format or proprietary data.
// Implement one importer per supported original game format in later phases.
class ContentImporter {
public:
    virtual ~ContentImporter() = default;
    [[nodiscard]] virtual ImportedContent load(const std::filesystem::path& source) const = 0;
};

// Loads only independently authored fixtures for testing the content pipeline.
class SyntheticJsonImporter final : public ContentImporter {
public:
    [[nodiscard]] ImportedContent load(const std::filesystem::path& source) const override;
};

} // namespace goldbox

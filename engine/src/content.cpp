#include "goldbox/content.hpp"
#include "json_support.hpp"

#include <limits>
#include <set>
#include <stdexcept>

namespace goldbox {

ImportedContent SyntheticJsonImporter::load(const std::filesystem::path& source) const {
    namespace j = json_support;
    const auto parsed = j::read_json(source);
    const auto& root = j::object(parsed, "content");
    j::fields_only(root, {"schemaVersion", "gameId", "sourceFormat", "journals"}, "content");
    if (j::integer(j::field(root, "schemaVersion", "content"), "schemaVersion") != 1) {
        throw std::runtime_error("unsupported content schemaVersion");
    }
    ImportedContent result;
    result.game_id = j::string(j::field(root, "gameId", "content"), "gameId");
    result.source_format = j::string(j::field(root, "sourceFormat", "content"), "sourceFormat");
    if (result.game_id.empty() || result.source_format != "synthetic-v1") {
        throw std::runtime_error("content importer accepts synthetic-v1 fixtures only");
    }
    const auto& entries = j::array(j::field(root, "journals", "content"), "journals");
    if (entries.size() > 100000) throw std::runtime_error("too many journal entries");
    std::set<JournalId> seen;
    for (const auto& entry : entries) {
        const auto& obj = j::object(entry, "journal entry");
        j::fields_only(obj, {"id", "title", "body"}, "journal entry");
        const auto id = j::integer(j::field(obj, "id", "journal"), "journal.id");
        if (id < 1 || id > std::numeric_limits<JournalId>::max()) {
            throw std::runtime_error("journal.id out of range");
        }
        if (!seen.insert(static_cast<JournalId>(id)).second) {
            throw std::runtime_error("duplicate base journal id: " + std::to_string(id));
        }
        result.journals.push_back({static_cast<JournalId>(id),
            j::string(j::field(obj, "title", "journal"), "journal.title"),
            j::string(j::field(obj, "body", "journal"), "journal.body")});
    }
    return result;
}

} // namespace goldbox

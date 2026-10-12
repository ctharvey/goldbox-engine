#include "goldbox/mods.hpp"
#include "json_support.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <regex>
#include <set>
#include <stdexcept>
#include <utility>

namespace goldbox {
namespace fs = std::filesystem;
namespace j = json_support;
namespace bj = boost::json;

namespace {

bool valid_mod_id(const std::string& id) {
    static const std::regex pattern("^[a-z0-9][a-z0-9._-]{0,127}$");
    return std::regex_match(id, pattern) && id.find("..") == std::string::npos;
}

ModManifest parse_manifest(const fs::path& file) {
    const auto parsed = j::read_json(file);
    const auto& root = j::object(parsed, "mod manifest");
    j::fields_only(root, {"schemaVersion", "id", "version", "priority", "requires", "patches"}, "mod manifest");
    if (j::integer(j::field(root, "schemaVersion", "mod manifest"), "schemaVersion") != 1) {
        throw std::runtime_error("unsupported mod schemaVersion in " + file.string());
    }
    ModManifest mod;
    mod.id = j::string(j::field(root, "id", "mod manifest"), "mod.id");
    if (!valid_mod_id(mod.id)) throw std::runtime_error("invalid mod.id in " + file.string());
    mod.version = j::string(j::field(root, "version", "mod manifest"), "mod.version");
    static const std::regex version_pattern(R"(^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$)");
    if (!std::regex_match(mod.version, version_pattern)) {
        throw std::runtime_error("mod.version must be a semantic version in " + file.string());
    }
    if (const auto* value = root.if_contains("priority")) {
        const auto priority = j::integer(*value, "mod.priority");
        if (priority < -10000 || priority > 10000) throw std::runtime_error("mod.priority out of range");
        mod.priority = static_cast<int>(priority);
    }
    if (const auto* value = root.if_contains("requires")) {
        const auto& deps = j::array(*value, "mod.requires");
        if (deps.size() > 1000) throw std::runtime_error("too many mod dependencies");
        std::set<std::string> seen;
        for (const auto& dependency : deps) {
            auto id = j::string(dependency, "mod.requires item");
            if (!valid_mod_id(id) || id == mod.id || !seen.insert(id).second) {
                throw std::runtime_error("invalid, self or duplicate dependency in " + mod.id);
            }
            mod.requires_mods.push_back(std::move(id));
        }
    }
    if (const auto* value = root.if_contains("patches")) {
        const auto& patches = j::array(*value, "mod.patches");
        if (patches.size() > 10000) throw std::runtime_error("too many patches in " + mod.id);
        std::set<JournalId> seen;
        for (const auto& entry : patches) {
            const auto& obj = j::object(entry, "mod.patch");
            j::fields_only(obj, {"target", "id", "title", "body"}, "mod.patch");
            if (j::string(j::field(obj, "target", "mod.patch"), "mod.patch.target") != "journal") {
                throw std::runtime_error("unsupported patch target in " + mod.id);
            }
            const auto id = j::integer(j::field(obj, "id", "mod.patch"), "mod.patch.id");
            if (id < 1 || id > std::numeric_limits<JournalId>::max()) {
                throw std::runtime_error("patch id out of range in " + mod.id);
            }
            if (!seen.insert(static_cast<JournalId>(id)).second) {
                throw std::runtime_error("duplicate journal patch id within " + mod.id);
            }
            JournalPatch patch{mod.id, static_cast<JournalId>(id), mod.priority, std::nullopt, std::nullopt, std::nullopt};
            if (const auto* title = obj.if_contains("title")) patch.title = j::string(*title, "patch.title");
            if (const auto* body = obj.if_contains("body")) patch.body = j::string(*body, "patch.body");
            if (!patch.title && !patch.body) throw std::runtime_error("empty patch in " + mod.id);
            mod.journal_patches.push_back(std::move(patch));
        }
    }
    return mod;
}

} // namespace

std::vector<ModManifest> discover_mods(const fs::path& mods_root) {
    const fs::path root = fs::canonical(mods_root);
    if (!fs::is_directory(root)) throw std::runtime_error("mods path is not a directory");
    std::vector<ModManifest> mods;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (entry.is_symlink()) {
            throw std::runtime_error("symlinked mod directories are not allowed: " + entry.path().string());
        }
        if (!entry.is_directory()) continue;
        const auto file = entry.path() / "mod.json";
        if (!fs::exists(file)) continue;
        if (fs::is_symlink(file) || !fs::is_regular_file(file)) {
            throw std::runtime_error("mod.json must be a regular non-symlinked file: " + file.string());
        }
        if (fs::canonical(file.parent_path()) != root / entry.path().filename()) {
            throw std::runtime_error("mod escaped mods root: " + entry.path().string());
        }
        mods.push_back(parse_manifest(file));
        if (mods.size() > 1000) throw std::runtime_error("too many installed mods");
    }
    return mods;
}

ModResolution resolve_mods(std::vector<ModManifest> mods, const ImportedContent& content) {
    ModResolution result;
    std::map<std::string, std::size_t> lookup;
    for (std::size_t i = 0; i < mods.size(); ++i) {
        if (!lookup.emplace(mods[i].id, i).second) {
            throw std::runtime_error("duplicate installed mod id: " + mods[i].id);
        }
    }
    std::vector<std::size_t> indegree(mods.size(), 0);
    std::vector<std::vector<std::size_t>> followers(mods.size());
    for (std::size_t i = 0; i < mods.size(); ++i) {
        for (const auto& dependency : mods[i].requires_mods) {
            const auto found = lookup.find(dependency);
            if (found == lookup.end()) {
                throw std::runtime_error("missing required mod '" + dependency + "' for " + mods[i].id);
            }
            ++indegree[i];
            followers[found->second].push_back(i);
        }
    }
    // Highest priority is applied last, unless dependency order requires otherwise.
    struct ByPrecedence {
        const std::vector<ModManifest>* mods;
        bool operator()(std::size_t a, std::size_t b) const {
            const auto& x = (*mods)[a];
            const auto& y = (*mods)[b];
            if (x.priority != y.priority) return x.priority > y.priority;
            return x.id > y.id;
        }
    };
    std::priority_queue<std::size_t, std::vector<std::size_t>, ByPrecedence> ready(ByPrecedence{&mods});
    for (std::size_t i = 0; i < mods.size(); ++i) if (!indegree[i]) ready.push(i);
    while (!ready.empty()) {
        const auto index = ready.top();
        ready.pop();
        result.ordered.push_back(std::move(mods[index]));
        for (const auto next : followers[index]) if (--indegree[next] == 0) ready.push(next);
    }
    if (result.ordered.size() != mods.size()) throw std::runtime_error("cyclic mod dependencies");

    std::set<JournalId> base_ids;
    for (const auto& entry : content.journals) {
        if (!base_ids.insert(entry.id).second) throw std::runtime_error("duplicate journal in imported content");
    }
    std::map<std::pair<JournalId, std::string>, std::string> touched;
    for (const auto& mod : result.ordered) {
        for (const auto& patch : mod.journal_patches) {
            if (!base_ids.contains(patch.id)) {
                throw std::runtime_error("mod " + mod.id + " targets nonexistent journal #" + std::to_string(patch.id));
            }
            for (const auto& field_name : {"title", "body"}) {
                if (std::string_view(field_name) == "title" ? !patch.title : !patch.body) continue;
                const auto key = std::make_pair(patch.id, std::string(field_name));
                if (auto it = touched.find(key); it != touched.end()) {
                    result.warnings.push_back("journal #" + std::to_string(patch.id) + "." + field_name +
                        " changed by " + it->second + " then " + mod.id + "; latter wins");
                }
                touched[key] = mod.id;
            }
        }
    }
    return result;
}

Journal build_journal(const ImportedContent& content, const ModResolution& mods, bool enhanced) {
    Journal journal;
    for (const auto& entry : content.journals) journal.add_base(entry);
    if (enhanced) {
        std::size_t order = 0;
        for (const auto& mod : mods.ordered) {
            for (auto patch : mod.journal_patches) {
                patch.apply_order = order++;
                journal.add_patch(std::move(patch));
            }
        }
    }
    return journal;
}

std::string make_session_metadata(const ImportedContent& content, const ModResolution& mods, bool enhanced) {
    bj::array installed;
    if (enhanced) {
        for (const auto& mod : mods.ordered) {
            bj::object item;
            item["id"] = mod.id;
            item["version"] = mod.version;
            installed.push_back(std::move(item));
        }
    }
    bj::object header;
    header["schemaVersion"] = 1;
    header["gameId"] = content.game_id;
    header["sourceFormat"] = content.source_format;
    header["mode"] = enhanced ? "enhanced" : "classic";
    header["mods"] = std::move(installed);
    return bj::serialize(header);
}

} // namespace goldbox

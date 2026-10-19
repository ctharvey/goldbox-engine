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
    j::fields_only(root, {"schemaVersion", "id", "version", "priority", "requires", "patches", "images"}, "mod manifest");
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
    if (const auto* value = root.if_contains("images")) {
        const auto& images = j::array(*value,"mod.images");
        if (images.size() > 10000) throw std::runtime_error("too many replacement images");
        std::set<ImageKey> seen;
        for (const auto& item : images) {
            const auto& obj = j::object(item,"mod image");
            j::fields_only(obj,{"game", "archive", "kind", "record", "frame", "file"},"mod image");
            ImageKey key;
            key.game = j::string(j::field(obj,"game","mod image"),"image.game");
            if (!valid_mod_id(key.game)) throw std::runtime_error("Invalid image game identity");
            key.archive = image_archive_id(j::string(j::field(obj,"archive","mod image"),"image.archive"));
            key.kind = j::string(j::field(obj,"kind","mod image"),"image.kind");
            const auto record = j::integer(j::field(obj,"record","mod image"),"image.record");
            const auto frame = j::integer(j::field(obj,"frame","mod image"),"image.frame");
            if (record < 0 || record > 254 || frame < 0 || frame > 255 ||
                (key.kind != "picture" && key.kind != "sprite"))
                throw std::runtime_error("Unsupported image source identity");
            key.record = static_cast<std::uint16_t>(record);
            key.frame = static_cast<std::uint16_t>(frame);
            if (!seen.insert(key).second) throw std::runtime_error("Duplicate image binding in " + mod.id);
            const auto name = j::string(j::field(obj,"file","mod image"),"image.file");
            if (std::any_of(name.begin(),name.end(),[](unsigned char c) { return c < 32; }))
                throw std::runtime_error("Control characters in image path");
            const fs::path relative(name);
            if (name.find('\\') != std::string::npos || name.find(':') != std::string::npos ||
                relative.empty() || relative.is_absolute() || relative.extension() != ".bmp")
                throw std::runtime_error("Image file must be a relative .bmp path");
            auto path = file.parent_path();
            for (const auto& part : relative) {
                if (part == "." || part == "..") throw std::runtime_error("Image path traversal rejected");
                path /= part;
                if (fs::is_symlink(path)) throw std::runtime_error("Symlinked image paths are not allowed");
            }
            if (!fs::is_regular_file(path) || fs::file_size(path) > 32u*1024u*1024u)
                throw std::runtime_error("Image must be a regular file at most 32 MiB");
            const auto canonical = fs::canonical(path);
            const auto within = canonical.lexically_relative(fs::canonical(file.parent_path()));
            if (within.empty() || within.is_absolute() || *within.begin() == "..")
                throw std::runtime_error("Image escaped mod directory");
            mod.images.push_back({std::move(key),canonical,mod.id});
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
    std::map<ImageKey,std::string> image_touched;
    for (const auto& mod : result.ordered) {
        for (const auto& image : mod.images) {
            if (image.source.game != content.game_id) continue;
            auto key = image.source;
            key.archive = image_archive_id(key.archive);
            if (const auto found = image_touched.find(key); found != image_touched.end())
                result.warnings.push_back("image " + key.archive + ":" + std::to_string(key.record) +
                    ":" + key.kind + ":" + std::to_string(key.frame) + " changed by " + found->second +
                    " then " + mod.id + "; latter wins");
            image_touched[key] = mod.id;
        }
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

ImageLibrary build_image_library(const ModResolution& mods) {
    ImageLibrary library;
    for (const auto& mod : mods.ordered)
        for (const auto& image : mod.images) library.add(image);
    return library;
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

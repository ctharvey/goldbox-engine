#include "goldbox/content.hpp"
#include "goldbox/mods.hpp"

#include <stdexcept>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>


#define CHECK(expression) do { \
    if (!(expression)) throw std::runtime_error("Test failure: " #expression); \
} while (false)

namespace fs = std::filesystem;

namespace {
struct TempDir {
    fs::path path;
    TempDir() {
        static std::size_t counter = 0;
        path = fs::temp_directory_path() /
            ("goldbox-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(counter++));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};
void write_file(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path);
    if (!stream || !(stream << contents)) throw std::runtime_error("test fixture write failed");
}
void throws(const std::function<void()>& callback, const std::string& contains) {
    try { callback(); }
    catch (const std::exception& error) {
        if (std::string(error.what()).find(contains) != std::string::npos) return;
        throw std::runtime_error("unexpected exception: " + std::string(error.what()));
    }
    throw std::runtime_error("expected exception mentioning: " + contains);
}
const std::string synthetic_content = R"({
  "schemaVersion":1,"gameId":"test-content","sourceFormat":"synthetic-v1",
  "journals":[{"id":17,"title":"Original title","body":"Original body"}]
})";
} // namespace

int main() {
    TempDir t;
    const auto source = t.path / "demo.json";
    const auto root = t.path / "mods";
    fs::create_directory(root);
    write_file(source, synthetic_content);
    const auto content = goldbox::SyntheticJsonImporter{}.load(source);
    CHECK(content.game_id == "test-content");
    CHECK(content.journals.size() == 1);

    write_file(root / "a" / "mod.json", R"({"schemaVersion":1,"id":"alpha","version":"1.0.0","priority":100,
        "patches":[{"target":"journal","id":17,"title":"Alpha title"}]})");
    write_file(root / "b" / "mod.json", R"({"schemaVersion":1,"id":"bravo","version":"1.2.3","priority":0,
        "requires":["alpha"],"patches":[{"target":"journal","id":17,"title":"Bravo title","body":"Bravo body"}]})");
    write_file(root / "c" / "mod.json", R"({"schemaVersion":1,"id":"charlie","version":"1.0.0","priority":-1,
        "patches":[{"target":"journal","id":17,"body":"Charlie body"}]})");

    const auto resolved = goldbox::resolve_mods(goldbox::discover_mods(root), content);
    CHECK(resolved.ordered.size() == 3);
    // Dependencies supersede priority, and the applied order is stored in patches.
    CHECK(resolved.ordered[0].id == "charlie");
    CHECK(resolved.ordered[1].id == "alpha");
    CHECK(resolved.ordered[2].id == "bravo");
    CHECK(resolved.warnings.size() == 2); // One conflict on each journal field.
    const auto enhanced = goldbox::build_journal(content, resolved, true);
    CHECK(enhanced.resolve(17)->title == "Bravo title");
    CHECK(enhanced.resolve(17)->body == "Bravo body");
    const auto classic = goldbox::build_journal(content, resolved, false);
    CHECK(classic.resolve(17)->title == "Original title");
    CHECK(classic.resolve(17)->body == "Original body");
    CHECK(goldbox::make_session_metadata(content, resolved, true).find("\"enhanced\"") != std::string::npos);
    CHECK(goldbox::make_session_metadata(content, resolved, true).find("\"1.2.3\"") != std::string::npos);
    CHECK(goldbox::make_session_metadata(content, resolved, false).find("\"mods\":[]") != std::string::npos);

    // Reject missing dependencies and cycles.
    auto copy = resolved.ordered;
    copy[0].requires_mods = {"missing"};
    throws([&] { (void)goldbox::resolve_mods(copy, content); }, "missing required mod");
    copy = resolved.ordered;
    copy[1].requires_mods = {"bravo"};
    throws([&] { (void)goldbox::resolve_mods(copy, content); }, "cyclic");

    // Reject patches for IDs not present in the imported campaign.
    copy = resolved.ordered;
    copy[0].journal_patches[0].id = 99;
    throws([&] { (void)goldbox::resolve_mods(copy, content); }, "nonexistent journal");

    // Manifest type validation rejects silently dangerous schemas.
    write_file(root / "c" / "mod.json", R"({"schemaVersion":1,"id":"charlie","version":"1.0.0",
      "patches":[{"target":"journal","id":17,"title":null}]})");
    throws([&] { (void)goldbox::discover_mods(root); }, "must be a string");
    write_file(root / "c" / "mod.json", R"({"schemaVersion":1,"id":"charlie","version":"1.0.0",
      "extra":true})");
    throws([&] { (void)goldbox::discover_mods(root); }, "unexpected field");

    // Reject symlinks instead of reading arbitrary files outside the mod root.
    fs::remove(root / "c" / "mod.json");
    std::error_code ec;
    fs::create_directory_symlink(t.path, root / "escape", ec);
    if (!ec) {
        throws([&] { (void)goldbox::discover_mods(root); }, "symlinked mod");
        fs::remove(root / "escape");
    }

    // Campaigns cannot masquerade as imported SSI data with this importer.
    write_file(source, R"({"schemaVersion":1,"gameId":"test","sourceFormat":"ssi-goldbox",
       "journals":[{"id":17,"title":"A","body":"B"}]})");
    throws([&] { (void)goldbox::SyntheticJsonImporter{}.load(source); }, "synthetic-v1");
    write_file(source, R"({"schemaVersion":1,"gameId":"test","sourceFormat":"synthetic-v1",
      "journals":[{"id":17,"title":"A","body":"B"},{"id":17,"title":"C","body":"D"}]})");
    throws([&] { (void)goldbox::SyntheticJsonImporter{}.load(source); }, "duplicate base journal");
    write_file(source, "{ bad json");
    throws([&] { (void)goldbox::SyntheticJsonImporter{}.load(source); }, "Invalid JSON");
    return 0;
}

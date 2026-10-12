#include "goldbox/journal.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAILED: " << #condition << " at " << __FILE__ << ':' << __LINE__ << '\n'; \
        return 1; \
    } \
} while (false)


#define CHECK(expression) do { \
    if (!(expression)) throw std::runtime_error("Test failure: " #expression); \
} while (false)

int main() {
    goldbox::Journal journal;
    journal.add_base({17, "Base title", "Original body"});
    REQUIRE(journal.resolve(17).has_value());
    REQUIRE(journal.resolve(17)->body == "Original body");
    REQUIRE(!journal.resolve(18).has_value());

    journal.add_patch({"low", 17, 10, std::string("Low priority"), std::nullopt});
    journal.add_patch({"high", 17, 20, std::nullopt, std::string("New body")});
    journal.add_patch({"higher", 17, 30, std::string("High priority"), std::nullopt});
    REQUIRE(journal.resolve(17)->title == "High priority");
    REQUIRE(journal.resolve(17)->body == "New body");

    // Base game data is never mutated by mods.
    journal.add_patch({"next", 17, 40, std::nullopt, std::string("Latest body")});
    REQUIRE(journal.resolve(17)->body == "Latest body");
    REQUIRE(!journal.resolve(19).has_value());

    // Identical priority has deterministic lexical mod-id precedence.
    journal.add_patch({"alpha", 17, 50, std::string("Alpha"), std::nullopt});
    journal.add_patch({"zeta", 17, 50, std::string("Zeta"), std::nullopt});
    REQUIRE(journal.resolve(17)->title == "Zeta");

    bool threw = false;
    try { journal.add_base({17, "Duplicate", "Bad"}); }
    catch (const std::invalid_argument&) { threw = true; }
    REQUIRE(threw);

    return 0;
}

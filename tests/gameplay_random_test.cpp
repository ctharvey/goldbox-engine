// SPDX-License-Identifier: GPL-3.0-or-later
// Numeric observations only; original routines and resources remain private.
#include "goldbox/game_profile.hpp"
#include "goldbox/exploration.hpp"
#include <boost/json.hpp>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <functional>
#include <fstream>

using namespace goldbox;
namespace {
struct Host : EclHost { void text(std::string_view, bool) override {} };
void rejects(const std::function<void()>& action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
ExplorationSession session_for(const GameProfile& profile) {
    ExplorationSession session;
    session.map = formats::decode_geo16_record(std::vector<std::uint8_t>(1026),7);
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f};
    session.opcodes = minimal_vm_opcodes();
    session.initial_random = profile.random;
    session.random_dialect = profile.random_dialect;
    session.state.random = profile.random;
    session.character_rules = profile.character_rules;
    return session;
}
void program(ExplorationSession& session, std::initializer_list<std::uint8_t> body) {
    session.ecl_record.clear();
    for (int i = 0; i < 5; ++i) session.ecl_record.insert(session.ecl_record.end(),{1,1,20,0x99});
    session.ecl_record.insert(session.ecl_record.end(),body);
}
EclExecution run(ExplorationSession& session, EclLimits limits = {}) {
    Host host;
    return session.trigger(formats::EclEventKind::move,host,limits);
}
void same(const std::vector<AbilityValue>& a, const std::vector<AbilityValue>& b) {
    assert(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        assert(a[i].value == b[i].value && a[i].percentile == b[i].percentile);
}
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto profile = read_game_profile(argv[1]);
    assert(profile.random && profile.random->state() == 1);
    assert(profile.random_dialect == EclRandomDialect::byte_upper);
    const auto& rules = *profile.character_rules;
    assert(rules.dice_count == 3 && rules.dice_sides == 6 && rules.drop_lowest == 0 && rules.keep_best_of == 6);
    std::ifstream source(argv[1]);
    const std::string profile_json{std::istreambuf_iterator<char>(source),std::istreambuf_iterator<char>()};
    auto fixture = boost::json::parse(profile_json).as_object();
    fixture["characterCreationRules"] =
        std::filesystem::absolute(std::filesystem::path(argv[1]).parent_path()/
            "../../rules/adnd-first-edition-creation.json").generic_string();
    const auto fixture_path = std::filesystem::temp_directory_path()/"goldbox_random_profile_fixture.json";
    const auto invalid_profile = [&](const char* section, const char* field, boost::json::value value) {
        auto bad = fixture;
        bad[section].as_object()[field] = std::move(value);
        { std::ofstream file(fixture_path); file << boost::json::serialize(bad); }
        rejects([&] { (void)read_game_profile(fixture_path); });
        std::filesystem::remove(fixture_path);
    };
    invalid_profile("random","algorithm","unknown");
    invalid_profile("random","eclDialect","unknown");
    invalid_profile("random","seed",-1);
    invalid_profile("random","multiplier",4294967296ULL);
    invalid_profile("random","increment",true);
    invalid_profile("characterCreationDice","count",0);
    invalid_profile("characterCreationDice","sides",101);
    invalid_profile("characterCreationDice","dropLowest",3);
    invalid_profile("characterCreationDice","keepBestOf",17);
    auto bad_rules = rules;
    bad_rules.dice_sides = 0;
    auto unchanged = *profile.random;
    rejects([&] { (void)roll_abilities(bad_rules,"human","male",unchanged); });
    assert(unchanged.state() == profile.random->state());
    // Original human raw loop, stopped before age/class/gender caps and percentile handling.
    struct Observation { std::uint32_t seed; std::array<int,6> scores; std::uint32_t after; };
    const Observation observations[]{
        {0,{14,11,13,14,15,15},1638965300},
        {1,{16,13,14,16,17,14},2628645125},
        {0xffffffff,{16,13,14,13,16,13},649285475},
        {0x12345678,{15,15,14,15,15,16},1040758828},
        {0x2e4d0932,{15,16,14,15,17,14},2883502598}
    };
    for (const auto& observed : observations) {
        HighWordLcg32 rng(observed.seed,profile.random->multiplier(),profile.random->increment());
        const auto rolled = roll_abilities(rules,"human","male",rng);
        for (std::size_t i = 0; i < 6; ++i) assert(rolled[i].value == observed.scores[i]);
        assert(rng.state() == observed.after); // Exactly 108 original runtime draws.
    }
    // Observed original racial deltas; controls have no modifier. Same draw consumption.
    struct RacialObservation { const char* race; std::array<int,6> delta; };
    const RacialObservation races[]{
        {"dwarf",{0,0,0,0,1,-1}}, {"elf",{0,0,0,1,-1,0}}, {"gnome",{}},
        {"half-elf",{}}, {"halfling",{-1,0,0,1,0,0}}, {"human",{}}
    };
    for (const auto& race : races) for (const auto& observed : observations) {
        HighWordLcg32 rng(observed.seed,profile.random->multiplier(),profile.random->increment());
        const auto rolled = roll_abilities(rules,race.race,"male",rng);
        for (std::size_t i = 0; i < 6; ++i) assert(rolled[i].value == observed.scores[i]+race.delta[i]);
        assert(rng.state() == observed.after);
    }
    struct ExceptionalRacialObservation {
        const char* race; std::uint32_t seed; std::array<int,6> scores; std::uint32_t after;
    };
    for (const auto& observed : {
        ExceptionalRacialObservation{"dwarf",7,{13,11,15,12,19,16},4271756779u},
        ExceptionalRacialObservation{"elf",17,{14,15,13,19,15,18},1283653141u}}) {
        HighWordLcg32 rng(observed.seed,profile.random->multiplier(),profile.random->increment());
        const auto rolled = roll_abilities(rules,observed.race,"male",rng);
        for (std::size_t i = 0; i < 6; ++i) assert(rolled[i].value == observed.scores[i]);
        assert(rng.state() == observed.after);
        Party party;
        add_character(party,rules,finish_character(rules,
            {"Nineteen",observed.race,"male","fighter","lawful-good",rolled},rng));
        const auto restored = read_party(serialize_party(party,rules),rules);
        same(rolled,restored[0].abilities); // Legal 19 survives validation and persistence.
    }
    auto session = session_for(profile);
    const auto first_scores = roll_abilities(rules,"human","male",*session.state.random);
    Character created{"Seeded","human","male","fighter","lawful-good",first_scores};
    add_character(session.party,rules,finish_character(rules,created,*session.state.random));
    const auto saved = serialize_exploration(session,"synthetic-rng");
    const auto expected = roll_abilities(rules,"human","male",*session.state.random);
    auto fresh = session_for(profile);
    restore_exploration(fresh,saved,"synthetic-rng");
    assert(fresh.party.size() == 1 && fresh.party[0].name == "Seeded" && fresh.party[0].status == "okay");
    same(first_scores,fresh.party[0].abilities);
    same(expected,roll_abilities(rules,"human","male",*fresh.state.random));
    assert(fresh.state.random->state() == session.state.random->state());
    auto interleaved = *fresh.state.random;
    (void)interleaved.draw(1);
    program(fresh,{8,0,0,1,0,0x4a,0});
    assert(run(fresh).exited && fresh.state.random->state() == interleaved.state());
    same(roll_abilities(rules,"human","male",interleaved),
        roll_abilities(rules,"human","male",*fresh.state.random));
    assert(fresh.state.random->state() == interleaved.state());
    Character fighter{"Seed","human","male","fighter","lawful-good",{{18,0},{10,0},{10,0},{10,0},{10,0},{10,0}}};
    HighWordLcg32 percentile(0x12345678,profile.random->multiplier(),profile.random->increment());
    assert(finish_character(rules,fighter,percentile).abilities[0].percentile == 60);
    assert(percentile.state() == 0xcb5b9059);

    // Typed ECL reads, original inclusive byte bounds, zero draw and 255 edge.
    for (const auto upper : {0,20,254,255}) {
        session = session_for(profile);
        session.state.random.emplace(0x12345678,profile.random->multiplier(),1);
        program(session,{8,0,static_cast<std::uint8_t>(upper),1,0,0x4a,0});
        assert(run(session).exited);
        assert(session.state.memory[0x4a00] == (upper == 0 || upper == 20 ? 0 : 39));
        assert(session.state.random->state() == 0xcb5b9059);
    }
    session = session_for(profile);
    session.state.random.emplace(0x12345678,profile.random->multiplier(),1);
    session.state.memory[0x4b01] = 1; // Word 256 narrows to zero.
    program(session,{8,3,0,0x4b,3,0,0x4a,0});
    assert(run(session).exited && session.state.memory[0x4a00] == 0 && session.state.memory[0x4a01] == 0);
    assert(session.state.random->state() == 0xcb5b9059);
    session = session_for(profile);
    program(session,{8,0,20,1,20,0x99,0}); // Self-modifying destination rejected without consuming RNG.
    assert(!run(session).exited && session.state.random->state() == 1);
    program(session,{8,0,20,1,0,0x4a,0});
    session.random_dialect = EclRandomDialect::disabled;
    assert(!run(session).exited && session.state.random->state() == 1);
    session.random_dialect = EclRandomDialect::byte_upper;
    session.state.random.reset();
    assert(run(session).diagnostic.find("RNG state unavailable") != std::string::npos);
    session = session_for(profile);
    session.state.random.emplace(0x12345678,profile.random->multiplier(),1);
    program(session,{8,0,20,1,0,0x4a,1,1,20,0x99});
    const auto limited = run(session,{12,4});
    assert(!limited.exited && limited.steps == 12 && session.state.random->state() == 0xe28f6d7a);

    // Save validation is atomic, parameter-bound and backwards compatible.
    for (const auto field : {"algorithm","increment","state","eclDialect"}) {
        auto bad = boost::json::parse(saved).as_object();
        bad["random"].as_object()[field] = false;
        const auto before = fresh.state.random->state();
        rejects([&] { restore_exploration(fresh,boost::json::serialize(bad),"synthetic-rng"); });
        assert(fresh.state.random->state() == before);
    }
    auto legacy = boost::json::parse(saved).as_object();
    legacy.erase("random");
    restore_exploration(fresh,boost::json::serialize(legacy),"synthetic-rng");
    assert(!fresh.state.random); // Unknown legacy state is not silently reseeded.
    rejects([&] { auto bad = boost::json::parse(saved).as_object();
        bad["random"].as_object()["increment"] = 3;
        restore_exploration(fresh,boost::json::serialize(bad),"synthetic-rng"); });
    fresh = session_for(profile);
    fresh.random_dialect = static_cast<EclRandomDialect>(99);
    rejects([&] { (void)serialize_exploration(fresh,"synthetic-rng"); });
}

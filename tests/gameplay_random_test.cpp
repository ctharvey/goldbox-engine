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
#include <algorithm>
#include <iostream>
#include <source_location>

using namespace goldbox;
namespace {
struct Host : EclHost { void text(std::string_view, bool) override {} };
void rejects(const std::function<void()>& action, const std::source_location location = std::source_location::current()) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    if (!failed) std::cerr << "Expected rejection at " << location.line() << '\n';
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
    auto rules = *profile.character_rules;
    const auto complete_rules = rules;
    // Keep the earlier ability-only observations at their independently captured stopping point.
    for (auto& choice : rules.classes) { choice.hit_points.reset(); choice.starting_money.reset(); }
    // Numeric observations from the original combined creation routines. No original code/data bytes.
    struct CreationObservation {
        const char* race; const char* character_class; const char* gender; std::uint32_t seed;
        int age; std::array<int,6> scores; int percentile; std::uint32_t after; bool fixed_age{};
    };
    const CreationObservation creation_observations[]{
        {"dwarf","fighter","male",0x0u,49,{14,13,13,11,16,14},0,3517051158u},
        {"dwarf","fighter","male",0x1u,50,{12,13,15,14,18,13},0,3509128559u},
        {"dwarf","fighter","male",0xffffffffu,55,{17,13,14,16,16,13},0,3524973757u},
        {"dwarf","fighter","male",0x12345678u,52,{16,14,14,15,16,15},0,9511118u},
        {"dwarf","fighter","male",0x2e4d0932u,50,{16,12,13,15,18,13},0,4274625400u},
        {"dwarf","fighter","male",0x17u,58,{14,13,12,15,19,16},0,3334831381u},
        {"dwarf","fighter","female",0x0u,49,{14,13,13,11,16,14},0,3517051158u},
        {"dwarf","fighter","female",0x1u,50,{12,13,15,14,18,13},0,3509128559u},
        {"dwarf","fighter","female",0xffffffffu,55,{17,13,14,16,16,13},0,3524973757u},
        {"dwarf","fighter","female",0x12345678u,52,{16,14,14,15,16,15},0,9511118u},
        {"dwarf","fighter","female",0x2e4d0932u,50,{16,12,13,15,18,13},0,4274625400u},
        {"dwarf","fighter","female",0x17u,58,{14,13,12,15,19,16},0,3334831381u},
        {"dwarf","thief","male",0x0u,91,{13,13,16,13,18,13},0,2121381776u},
        {"dwarf","thief","male",0x1u,87,{15,13,15,11,18,15},0,1465832913u},
        {"dwarf","thief","male",0xffffffffu,83,{16,14,13,15,16,12},0,2776930639u},
        {"dwarf","thief","male",0x12345678u,83,{14,14,15,14,16,14},0,2364914696u},
        {"dwarf","thief","male",0x2e4d0932u,86,{16,12,15,15,16,16},0,4200524098u},
        {"dwarf","thief","male",0x17u,86,{15,14,11,17,18,15},0,4223627111u},
        {"dwarf","thief","female",0x0u,91,{13,13,16,13,18,13},0,2121381776u},
        {"dwarf","thief","female",0x1u,87,{15,13,15,11,18,15},0,1465832913u},
        {"dwarf","thief","female",0xffffffffu,83,{16,14,13,15,16,12},0,2776930639u},
        {"dwarf","thief","female",0x12345678u,83,{14,14,15,14,16,14},0,2364914696u},
        {"dwarf","thief","female",0x2e4d0932u,86,{16,12,15,15,16,16},0,4200524098u},
        {"dwarf","thief","female",0x17u,86,{15,14,11,17,18,15},0,4223627111u},
        {"elf","fighter","male",0x0u,149,{14,13,13,12,14,15},0,3517051158u},
        {"elf","fighter","male",0x1u,150,{12,13,15,15,16,14},0,3509128559u},
        {"elf","fighter","male",0xffffffffu,147,{16,13,13,17,14,14},0,3524973757u},
        {"elf","fighter","male",0x12345678u,148,{15,14,13,16,14,16},0,9511118u},
        {"elf","fighter","male",0x2e4d0932u,148,{16,12,13,16,16,14},0,4274625400u},
        {"elf","fighter","male",0x17u,148,{13,13,11,16,17,17},0,3334831381u},
        {"elf","fighter","female",0x0u,149,{14,13,13,12,14,15},0,3517051158u},
        {"elf","fighter","female",0x1u,150,{12,13,15,15,16,14},0,3509128559u},
        {"elf","fighter","female",0xffffffffu,147,{16,13,13,17,14,14},0,3524973757u},
        {"elf","fighter","female",0x12345678u,148,{15,14,13,16,14,16},0,9511118u},
        {"elf","fighter","female",0x2e4d0932u,148,{16,12,13,16,16,14},0,4274625400u},
        {"elf","fighter","female",0x17u,148,{13,13,11,16,17,17},0,3334831381u},
        {"elf","magic-user","male",0x0u,169,{14,13,13,12,14,15},0,3517051158u},
        {"elf","magic-user","male",0x1u,170,{12,13,15,15,16,14},0,3509128559u},
        {"elf","magic-user","male",0xffffffffu,167,{16,13,13,17,14,14},0,3524973757u},
        {"elf","magic-user","male",0x12345678u,168,{15,14,13,16,14,16},0,9511118u},
        {"elf","magic-user","male",0x2e4d0932u,168,{16,12,13,16,16,14},0,4274625400u},
        {"elf","magic-user","male",0x17u,168,{13,13,11,16,17,17},0,3334831381u},
        {"elf","magic-user","female",0x0u,169,{14,13,13,12,14,15},0,3517051158u},
        {"elf","magic-user","female",0x1u,170,{12,13,15,15,16,14},0,3509128559u},
        {"elf","magic-user","female",0xffffffffu,167,{16,13,13,17,14,14},0,3524973757u},
        {"elf","magic-user","female",0x12345678u,168,{15,14,13,16,14,16},0,9511118u},
        {"elf","magic-user","female",0x2e4d0932u,168,{16,12,13,16,16,14},0,4274625400u},
        {"elf","magic-user","female",0x17u,168,{13,13,11,16,17,17},0,3334831381u},
        {"elf","thief","male",0x0u,119,{14,13,13,12,14,15},0,3517051158u},
        {"elf","thief","male",0x1u,120,{12,13,15,15,16,14},0,3509128559u},
        {"elf","thief","male",0xffffffffu,117,{16,13,13,17,14,14},0,3524973757u},
        {"elf","thief","male",0x12345678u,118,{15,14,13,16,14,16},0,9511118u},
        {"elf","thief","male",0x2e4d0932u,118,{16,12,13,16,16,14},0,4274625400u},
        {"elf","thief","male",0x17u,118,{13,13,11,16,17,17},0,3334831381u},
        {"elf","thief","female",0x0u,119,{14,13,13,12,14,15},0,3517051158u},
        {"elf","thief","female",0x1u,120,{12,13,15,15,16,14},0,3509128559u},
        {"elf","thief","female",0xffffffffu,117,{16,13,13,17,14,14},0,3524973757u},
        {"elf","thief","female",0x12345678u,118,{15,14,13,16,14,16},0,9511118u},
        {"elf","thief","female",0x2e4d0932u,118,{16,12,13,16,16,14},0,4274625400u},
        {"elf","thief","female",0x17u,118,{13,13,11,16,17,17},0,3334831381u},
        {"gnome","fighter","male",0x0u,69,{14,13,13,11,15,15},0,3517051158u},
        {"gnome","fighter","male",0x1u,70,{12,13,15,14,17,14},0,3509128559u},
        {"gnome","fighter","male",0xffffffffu,75,{16,13,13,16,15,14},0,3524973757u},
        {"gnome","fighter","male",0x12345678u,72,{15,14,13,15,15,16},0,9511118u},
        {"gnome","fighter","male",0x2e4d0932u,70,{16,12,13,15,17,14},0,4274625400u},
        {"gnome","fighter","male",0x17u,78,{13,13,11,15,18,17},0,3334831381u},
        {"gnome","fighter","female",0x0u,69,{14,13,13,11,15,15},0,3517051158u},
        {"gnome","fighter","female",0x1u,70,{12,13,15,14,17,14},0,3509128559u},
        {"gnome","fighter","female",0xffffffffu,75,{15,13,13,16,15,14},0,3524973757u},
        {"gnome","fighter","female",0x12345678u,72,{15,14,13,15,15,16},0,9511118u},
        {"gnome","fighter","female",0x2e4d0932u,70,{15,12,13,15,17,14},0,4274625400u},
        {"gnome","fighter","female",0x17u,78,{13,13,11,15,18,17},0,3334831381u},
        {"gnome","thief","male",0x0u,89,{14,13,13,11,15,15},0,3517051158u},
        {"gnome","thief","male",0x1u,90,{12,13,15,14,17,14},0,3509128559u},
        {"gnome","thief","male",0xffffffffu,95,{17,13,14,16,15,14},0,3524973757u},
        {"gnome","thief","male",0x12345678u,92,{16,14,14,15,15,16},0,9511118u},
        {"gnome","thief","male",0x2e4d0932u,90,{16,12,13,15,17,14},0,4274625400u},
        {"gnome","thief","male",0x17u,98,{14,13,12,15,18,17},0,3334831381u},
        {"gnome","thief","female",0x0u,89,{14,13,13,11,15,15},0,3517051158u},
        {"gnome","thief","female",0x1u,90,{12,13,15,14,17,14},0,3509128559u},
        {"gnome","thief","female",0xffffffffu,95,{15,13,14,16,15,14},0,3524973757u},
        {"gnome","thief","female",0x12345678u,92,{15,14,14,15,15,16},0,9511118u},
        {"gnome","thief","female",0x2e4d0932u,90,{15,12,13,15,17,14},0,4274625400u},
        {"gnome","thief","female",0x17u,98,{14,13,12,15,18,17},0,3334831381u},
        {"half-elf","cleric","male",0x0u,43,{15,10,13,14,15,15},0,3181827459u},
        {"half-elf","cleric","male",0x1u,43,{13,13,16,13,17,14},0,2121381776u},
        {"half-elf","cleric","male",0xffffffffu,47,{17,13,14,16,15,14},0,4242273142u},
        {"half-elf","cleric","male",0x12345678u,45,{16,15,14,15,15,16},0,425011611u},
        {"half-elf","cleric","male",0x2e4d0932u,42,{17,11,14,15,17,14},0,3676467469u},
        {"half-elf","cleric","male",0x17u,48,{14,13,13,15,18,17},0,266413230u},
        {"half-elf","cleric","female",0x0u,43,{15,10,13,14,15,15},0,3181827459u},
        {"half-elf","cleric","female",0x1u,43,{13,13,16,13,17,14},0,2121381776u},
        {"half-elf","cleric","female",0xffffffffu,47,{17,13,14,16,15,14},0,4242273142u},
        {"half-elf","cleric","female",0x12345678u,45,{16,15,14,15,15,16},0,425011611u},
        {"half-elf","cleric","female",0x2e4d0932u,42,{17,11,14,15,17,14},0,3676467469u},
        {"half-elf","cleric","female",0x17u,48,{14,13,13,15,18,17},0,266413230u},
        {"half-elf","fighter","male",0x0u,26,{12,13,15,13,17,14},0,2121381776u},
        {"half-elf","fighter","male",0x1u,28,{14,13,14,11,17,16},0,1465832913u},
        {"half-elf","fighter","male",0xffffffffu,32,{15,14,12,15,15,13},0,2776930639u},
        {"half-elf","fighter","male",0x12345678u,28,{13,14,14,14,15,15},0,2364914696u},
        {"half-elf","fighter","male",0x2e4d0932u,27,{15,12,14,15,15,17},0,4200524098u},
        {"half-elf","fighter","male",0x17u,33,{14,14,10,17,17,16},0,4223627111u},
        {"half-elf","fighter","female",0x0u,26,{12,13,15,13,17,14},0,2121381776u},
        {"half-elf","fighter","female",0x1u,28,{14,13,14,11,17,16},0,1465832913u},
        {"half-elf","fighter","female",0xffffffffu,32,{15,14,12,15,15,13},0,2776930639u},
        {"half-elf","fighter","female",0x12345678u,28,{13,14,14,14,15,15},0,2364914696u},
        {"half-elf","fighter","female",0x2e4d0932u,27,{15,12,14,15,15,17},0,4200524098u},
        {"half-elf","fighter","female",0x17u,33,{14,14,10,17,17,16},0,4223627111u},
        {"half-elf","magic-user","male",0x0u,37,{14,10,12,14,15,15},0,3181827459u},
        {"half-elf","magic-user","male",0x1u,41,{13,13,16,13,17,14},0,2121381776u},
        {"half-elf","magic-user","male",0xffffffffu,41,{17,13,14,16,15,14},0,4242273142u},
        {"half-elf","magic-user","male",0x12345678u,43,{16,15,14,15,15,16},0,425011611u},
        {"half-elf","magic-user","male",0x2e4d0932u,36,{16,11,13,15,17,14},0,3676467469u},
        {"half-elf","magic-user","male",0x17u,46,{14,13,13,15,18,17},0,266413230u},
        {"half-elf","magic-user","female",0x0u,37,{14,10,12,14,15,15},0,3181827459u},
        {"half-elf","magic-user","female",0x1u,41,{13,13,16,13,17,14},0,2121381776u},
        {"half-elf","magic-user","female",0xffffffffu,41,{17,13,14,16,15,14},0,4242273142u},
        {"half-elf","magic-user","female",0x12345678u,43,{16,15,14,15,15,16},0,425011611u},
        {"half-elf","magic-user","female",0x2e4d0932u,36,{16,11,13,15,17,14},0,3676467469u},
        {"half-elf","magic-user","female",0x17u,46,{14,13,13,15,18,17},0,266413230u},
        {"half-elf","thief","male",0x0u,34,{12,13,15,13,17,14},0,2121381776u},
        {"half-elf","thief","male",0x1u,40,{14,13,14,11,17,16},0,1465832913u},
        {"half-elf","thief","male",0xffffffffu,36,{15,14,12,15,15,13},0,2776930639u},
        {"half-elf","thief","male",0x12345678u,40,{13,14,14,14,15,15},0,2364914696u},
        {"half-elf","thief","male",0x2e4d0932u,31,{15,12,14,15,15,17},0,4200524098u},
        {"half-elf","thief","male",0x17u,45,{15,14,11,17,17,16},0,4223627111u},
        {"half-elf","thief","female",0x0u,34,{12,13,15,13,17,14},0,2121381776u},
        {"half-elf","thief","female",0x1u,40,{14,13,14,11,17,16},0,1465832913u},
        {"half-elf","thief","female",0xffffffffu,36,{15,14,12,15,15,13},0,2776930639u},
        {"half-elf","thief","female",0x12345678u,40,{13,14,14,14,15,15},0,2364914696u},
        {"half-elf","thief","female",0x2e4d0932u,31,{15,12,14,15,15,17},0,4200524098u},
        {"half-elf","thief","female",0x17u,45,{15,14,11,17,17,16},0,4223627111u},
        {"halfling","fighter","male",0x0u,24,{11,13,15,14,17,14},0,2121381776u},
        {"halfling","fighter","male",0x1u,26,{13,13,14,12,17,16},0,1465832913u},
        {"halfling","fighter","male",0xffffffffu,30,{14,14,12,16,15,13},0,2776930639u},
        {"halfling","fighter","male",0x12345678u,26,{12,14,14,15,15,15},0,2364914696u},
        {"halfling","fighter","male",0x2e4d0932u,25,{14,12,14,16,15,17},0,4200524098u},
        {"halfling","fighter","male",0x17u,31,{13,14,10,18,17,16},0,4223627111u},
        {"halfling","fighter","female",0x0u,24,{11,13,15,14,17,14},0,2121381776u},
        {"halfling","fighter","female",0x1u,26,{13,13,14,12,17,16},0,1465832913u},
        {"halfling","fighter","female",0xffffffffu,30,{14,14,12,16,15,13},0,2776930639u},
        {"halfling","fighter","female",0x12345678u,26,{12,14,14,15,15,15},0,2364914696u},
        {"halfling","fighter","female",0x2e4d0932u,25,{14,12,14,16,15,17},0,4200524098u},
        {"halfling","fighter","female",0x17u,31,{13,14,10,18,17,16},0,4223627111u},
        {"halfling","thief","male",0x0u,43,{14,10,13,15,15,15},0,3181827459u},
        {"halfling","thief","male",0x1u,43,{12,13,16,14,17,14},0,2121381776u},
        {"halfling","thief","male",0xffffffffu,47,{16,13,14,17,15,14},0,4242273142u},
        {"halfling","thief","male",0x12345678u,45,{15,15,14,16,15,16},0,425011611u},
        {"halfling","thief","male",0x2e4d0932u,42,{16,11,14,16,17,14},0,3676467469u},
        {"halfling","thief","male",0x17u,48,{13,13,13,16,18,17},0,266413230u},
        {"halfling","thief","female",0x0u,43,{14,10,13,15,15,15},0,3181827459u},
        {"halfling","thief","female",0x1u,43,{12,13,16,14,17,14},0,2121381776u},
        {"halfling","thief","female",0xffffffffu,47,{14,13,14,17,15,14},0,4242273142u},
        {"halfling","thief","female",0x12345678u,45,{14,15,14,16,15,16},0,425011611u},
        {"halfling","thief","female",0x2e4d0932u,42,{14,11,14,16,17,14},0,3676467469u},
        {"halfling","thief","female",0x17u,48,{13,13,13,16,18,17},0,266413230u},
        {"human","cleric","male",0x0u,19,{14,13,13,15,17,15},0,1876183322u},
        {"human","cleric","male",0x1u,20,{14,10,12,14,15,15},0,3181827459u},
        {"human","cleric","male",0xffffffffu,22,{14,13,16,13,18,12},0,570539185u},
        {"human","cleric","male",0x12345678u,22,{14,17,17,15,16,14},0,2365939794u},
        {"human","cleric","male",0x2e4d0932u,19,{14,12,14,14,15,16},0,595093148u},
        {"human","cleric","male",0x17u,22,{14,13,14,12,16,13},0,1841227401u},
        {"human","cleric","female",0x0u,19,{14,13,13,15,17,15},0,1876183322u},
        {"human","cleric","female",0x1u,20,{14,10,12,14,15,15},0,3181827459u},
        {"human","cleric","female",0xffffffffu,22,{14,13,16,13,18,12},0,570539185u},
        {"human","cleric","female",0x12345678u,22,{14,17,17,15,16,14},0,2365939794u},
        {"human","cleric","female",0x2e4d0932u,19,{14,12,14,14,15,16},0,595093148u},
        {"human","cleric","female",0x17u,22,{14,13,14,12,16,13},0,1841227401u},
        {"human","fighter","male",0x0u,16,{14,13,13,15,17,15},0,1876183322u},
        {"human","fighter","male",0x1u,17,{14,10,12,14,15,15},0,3181827459u},
        {"human","fighter","male",0xffffffffu,19,{13,13,15,13,18,12},0,570539185u},
        {"human","fighter","male",0x12345678u,19,{13,17,16,15,16,14},0,2365939794u},
        {"human","fighter","male",0x2e4d0932u,16,{14,12,14,14,15,16},0,595093148u},
        {"human","fighter","male",0x17u,19,{13,13,13,12,16,13},0,1841227401u},
        {"human","fighter","female",0x0u,16,{14,13,13,15,17,15},0,1876183322u},
        {"human","fighter","female",0x1u,17,{14,10,12,14,15,15},0,3181827459u},
        {"human","fighter","female",0xffffffffu,19,{13,13,15,13,18,12},0,570539185u},
        {"human","fighter","female",0x12345678u,19,{13,17,16,15,16,14},0,2365939794u},
        {"human","fighter","female",0x2e4d0932u,16,{14,12,14,14,15,16},0,595093148u},
        {"human","fighter","female",0x17u,19,{13,13,13,12,16,13},0,1841227401u},
        {"human","magic-user","male",0x0u,27,{15,10,13,14,15,15},0,3181827459u},
        {"human","magic-user","male",0x1u,27,{13,13,16,13,17,14},0,2121381776u},
        {"human","magic-user","male",0xffffffffu,31,{17,13,14,16,15,14},0,4242273142u},
        {"human","magic-user","male",0x12345678u,29,{16,15,14,15,15,16},0,425011611u},
        {"human","magic-user","male",0x2e4d0932u,26,{17,11,14,15,17,14},0,3676467469u},
        {"human","magic-user","male",0x17u,32,{14,13,13,15,18,17},0,266413230u},
        {"human","magic-user","female",0x0u,27,{15,10,13,14,15,15},0,3181827459u},
        {"human","magic-user","female",0x1u,27,{13,13,16,13,17,14},0,2121381776u},
        {"human","magic-user","female",0xffffffffu,31,{17,13,14,16,15,14},0,4242273142u},
        {"human","magic-user","female",0x12345678u,29,{16,15,14,15,15,16},0,425011611u},
        {"human","magic-user","female",0x2e4d0932u,26,{17,11,14,15,17,14},0,3676467469u},
        {"human","magic-user","female",0x17u,32,{14,13,13,15,18,17},0,266413230u},
        {"human","thief","male",0x0u,19,{14,13,13,15,17,15},0,1876183322u},
        {"human","thief","male",0x1u,20,{14,10,12,14,15,15},0,3181827459u},
        {"human","thief","male",0xffffffffu,22,{14,13,16,13,18,12},0,570539185u},
        {"human","thief","male",0x12345678u,22,{14,17,17,15,16,14},0,2365939794u},
        {"human","thief","male",0x2e4d0932u,19,{14,12,14,14,15,16},0,595093148u},
        {"human","thief","male",0x17u,22,{14,13,14,12,16,13},0,1841227401u},
        {"human","thief","female",0x0u,19,{14,13,13,15,17,15},0,1876183322u},
        {"human","thief","female",0x1u,20,{14,10,12,14,15,15},0,3181827459u},
        {"human","thief","female",0xffffffffu,22,{14,13,16,13,18,12},0,570539185u},
        {"human","thief","female",0x12345678u,22,{14,17,17,15,16,14},0,2365939794u},
        {"human","thief","female",0x2e4d0932u,19,{14,12,14,14,15,16},0,595093148u},
        {"human","thief","female",0x17u,22,{14,13,14,12,16,13},0,1841227401u},
        {"human","fighter","male",0x12u,17,{18,14,13,12,16,13},29,1273674349u},
        {"human","fighter","male",0x19u,17,{18,16,12,13,14,12},98,2440489160u},
        {"human","fighter","male",0x26u,16,{18,15,12,16,18,12},36,1539597169u},
        {"human","fighter","male",0x31u,17,{18,14,15,13,13,12},9,2759596544u},
        {"human","fighter","male",0xc1u,19,{18,16,10,13,14,15},62,379273552u},
        {"dwarf","fighter","male",0x12345678u,49,{15,15,13,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,50,{15,15,13,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,51,{16,15,14,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,149,{16,15,14,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,150,{16,15,14,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,151,{15,16,15,15,15,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,249,{15,16,15,15,15,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,250,{15,16,15,15,15,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,251,{13,16,16,13,14,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,349,{13,16,16,13,14,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,350,{13,16,16,13,14,15},0,1040758828u,true},
        {"dwarf","fighter","male",0x12345678u,351,{12,17,17,12,13,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,49,{15,15,13,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,50,{15,15,13,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,51,{16,15,14,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,149,{16,15,14,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,150,{16,15,14,15,16,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,151,{15,16,15,15,15,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,249,{15,16,15,15,15,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,250,{15,16,15,15,15,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,251,{13,16,16,13,14,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,349,{13,16,16,13,14,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,350,{13,16,16,13,14,15},0,1040758828u,true},
        {"dwarf","fighter","female",0x12345678u,351,{12,17,17,12,13,15},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,174,{15,15,13,16,14,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,175,{15,15,13,16,14,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,176,{16,15,14,16,14,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,549,{16,15,14,16,14,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,550,{16,15,14,16,14,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,551,{15,16,15,16,13,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,874,{15,16,15,16,13,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,875,{15,16,15,16,13,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,876,{13,16,16,14,12,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,1199,{13,16,16,14,12,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,1200,{13,16,16,14,12,16},0,1040758828u,true},
        {"elf","fighter","male",0x12345678u,1201,{12,17,17,13,11,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,174,{15,15,13,16,14,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,175,{15,15,13,16,14,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,176,{16,15,14,16,14,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,549,{16,15,14,16,14,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,550,{16,15,14,16,14,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,551,{15,16,15,16,13,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,874,{15,16,15,16,13,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,875,{15,16,15,16,13,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,876,{13,16,16,14,12,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,1199,{13,16,16,14,12,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,1200,{13,16,16,14,12,16},0,1040758828u,true},
        {"elf","fighter","female",0x12345678u,1201,{12,17,17,13,11,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,89,{15,15,13,15,15,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,90,{15,15,13,15,15,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,91,{16,15,14,15,15,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,299,{16,15,14,15,15,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,300,{16,15,14,15,15,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,301,{15,16,15,15,14,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,449,{15,16,15,15,14,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,450,{15,16,15,15,14,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,451,{13,16,16,13,13,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,599,{13,16,16,13,13,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,600,{13,16,16,13,13,16},0,1040758828u,true},
        {"gnome","fighter","male",0x12345678u,601,{12,17,17,12,12,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,89,{15,15,13,15,15,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,90,{15,15,13,15,15,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,91,{15,15,14,15,15,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,299,{15,15,14,15,15,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,300,{15,15,14,15,15,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,301,{15,16,15,15,14,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,449,{15,16,15,15,14,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,450,{15,16,15,15,14,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,451,{13,16,16,13,13,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,599,{13,16,16,13,13,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,600,{13,16,16,13,13,16},0,1040758828u,true},
        {"gnome","fighter","female",0x12345678u,601,{12,17,17,12,12,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,39,{15,15,13,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,40,{15,15,13,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,41,{16,15,14,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,99,{16,15,14,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,100,{16,15,14,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,101,{15,16,15,15,14,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,174,{15,16,15,15,14,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,175,{15,16,15,15,14,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,176,{13,16,16,13,13,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,249,{13,16,16,13,13,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,250,{13,16,16,13,13,16},0,1040758828u,true},
        {"half-elf","fighter","male",0x12345678u,251,{12,17,17,12,12,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,39,{15,15,13,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,40,{15,15,13,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,41,{16,15,14,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,99,{16,15,14,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,100,{16,15,14,15,15,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,101,{15,16,15,15,14,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,174,{15,16,15,15,14,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,175,{15,16,15,15,14,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,176,{13,16,16,13,13,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,249,{13,16,16,13,13,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,250,{13,16,16,13,13,16},0,1040758828u,true},
        {"half-elf","fighter","female",0x12345678u,251,{12,17,17,12,12,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,32,{14,15,13,16,15,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,33,{14,15,13,16,15,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,34,{15,15,14,16,15,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,67,{15,15,14,16,15,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,68,{15,15,14,16,15,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,69,{14,16,15,16,14,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,100,{14,16,15,16,14,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,101,{14,16,15,16,14,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,102,{12,16,16,14,13,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,143,{12,16,16,14,13,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,144,{12,16,16,14,13,16},0,1040758828u,true},
        {"halfling","fighter","male",0x12345678u,145,{11,17,17,13,12,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,32,{14,15,13,16,15,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,33,{14,15,13,16,15,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,34,{14,15,14,16,15,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,67,{14,15,14,16,15,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,68,{14,15,14,16,15,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,69,{14,16,15,16,14,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,100,{14,16,15,16,14,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,101,{14,16,15,16,14,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,102,{12,16,16,14,13,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,143,{12,16,16,14,13,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,144,{12,16,16,14,13,16},0,1040758828u,true},
        {"halfling","fighter","female",0x12345678u,145,{11,17,17,13,12,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,19,{15,15,13,15,15,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,20,{15,15,13,15,15,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,21,{16,15,14,15,15,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,39,{16,15,14,15,15,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,40,{16,15,14,15,15,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,41,{15,16,15,15,14,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,59,{15,16,15,15,14,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,60,{15,16,15,15,14,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,61,{13,16,16,13,13,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,89,{13,16,16,13,13,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,90,{13,16,16,13,13,16},0,1040758828u,true},
        {"human","fighter","male",0x12345678u,91,{12,17,17,12,12,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,19,{15,15,13,15,15,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,20,{15,15,13,15,15,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,21,{16,15,14,15,15,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,39,{16,15,14,15,15,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,40,{16,15,14,15,15,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,41,{15,16,15,15,14,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,59,{15,16,15,15,14,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,60,{15,16,15,15,14,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,61,{13,16,16,13,13,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,89,{13,16,16,13,13,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,90,{13,16,16,13,13,16},0,1040758828u,true},
        {"human","fighter","female",0x12345678u,91,{12,17,17,12,12,16},0,1040758828u,true},
    };
    for (const auto& observed : creation_observations) {
        Character draft{"Oracle",observed.race,observed.gender,observed.character_class,
            std::string_view(observed.character_class) == "thief" ? "neutral-good" : "lawful-good",{}};
        HighWordLcg32 rng(observed.seed,profile.random->multiplier(),profile.random->increment());
        if (observed.fixed_age) draft.age = static_cast<std::uint16_t>(observed.age);
        const auto rolled = roll_character(rules,draft,rng);
        assert(rolled.age == observed.age && rolled.status == "okay");
        for (std::size_t i = 0; i < 6; ++i) {
            assert(rolled.abilities[i].value == observed.scores[i]);
            assert(rolled.abilities[i].percentile == (i == 0 ? observed.percentile : 0));
        }
        assert(rng.state() == observed.after);
        validate_character(rules,rolled);
        const auto loaded = read_party(serialize_party({rolled},rules),rules);
        same(rolled.abilities,loaded[0].abilities); assert(loaded[0].age == rolled.age);
    }
    struct VitalsObservation { int gold, base, hp; std::uint32_t after; };
    // Original post-ability initialization, in the same order as the first 204 observations above.
    const VitalsObservation vitals_observations[]{
        {150,9,11,832869128u},
        {140,6,10,2890333993u},
        {110,6,8,3070371559u},
        {140,6,8,2261670016u},
        {140,8,12,1326963322u},
        {150,6,10,909920767u},
        {150,9,11,832869128u},
        {140,6,10,2890333993u},
        {110,6,8,3070371559u},
        {140,6,8,2261670016u},
        {140,8,12,1326963322u},
        {150,6,10,909920767u},
        {70,4,6,3509128559u},
        {40,4,6,607289132u},
        {90,4,6,2116000690u},
        {30,6,8,1311759367u},
        {110,4,6,2312759641u},
        {90,5,7,1191331178u},
        {70,4,6,3509128559u},
        {40,4,6,607289132u},
        {90,4,6,2116000690u},
        {30,6,8,1311759367u},
        {110,4,6,2312759641u},
        {90,5,7,1191331178u},
        {150,9,9,832869128u},
        {140,6,8,2890333993u},
        {110,6,6,3070371559u},
        {140,6,6,2261670016u},
        {140,8,10,1326963322u},
        {150,6,9,909920767u},
        {150,9,9,832869128u},
        {140,6,8,2890333993u},
        {110,6,6,3070371559u},
        {140,6,6,2261670016u},
        {140,8,10,1326963322u},
        {150,6,9,909920767u},
        {50,4,4,401550301u},
        {70,2,4,288477010u},
        {60,2,2,514623592u},
        {70,2,2,3218675893u},
        {60,4,6,1323513271u},
        {60,4,6,2095831904u},
        {50,4,4,401550301u},
        {70,2,4,288477010u},
        {60,2,2,514623592u},
        {70,2,2,3218675893u},
        {60,4,6,1323513271u},
        {60,4,6,2095831904u},
        {50,4,4,401550301u},
        {50,4,6,288477010u},
        {60,5,5,514623592u},
        {90,4,4,3218675893u},
        {80,6,8,1323513271u},
        {60,6,8,2095831904u},
        {50,4,4,401550301u},
        {50,4,6,288477010u},
        {60,5,5,514623592u},
        {90,4,4,3218675893u},
        {80,6,8,1323513271u},
        {60,6,8,2095831904u},
        {150,9,10,832869128u},
        {140,6,9,2890333993u},
        {110,6,7,3070371559u},
        {140,6,7,2261670016u},
        {140,8,11,1326963322u},
        {150,6,10,909920767u},
        {150,9,10,832869128u},
        {140,6,9,2890333993u},
        {110,6,7,3070371559u},
        {140,6,7,2261670016u},
        {140,8,11,1326963322u},
        {150,6,10,909920767u},
        {50,4,5,401550301u},
        {50,4,6,288477010u},
        {60,5,6,514623592u},
        {90,4,5,3218675893u},
        {80,6,8,1323513271u},
        {60,6,8,2095831904u},
        {50,4,5,401550301u},
        {50,4,6,288477010u},
        {60,5,6,514623592u},
        {90,4,5,3218675893u},
        {80,6,8,1323513271u},
        {60,6,8,2095831904u},
        {130,5,6,3509128559u},
        {90,5,7,607289132u},
        {100,8,9,2116000690u},
        {50,8,9,1311759367u},
        {140,5,7,2312759641u},
        {110,5,7,1191331178u},
        {130,5,6,3509128559u},
        {90,5,7,607289132u},
        {100,8,9,2116000690u},
        {50,8,9,1311759367u},
        {140,5,7,2312759641u},
        {110,5,7,1191331178u},
        {140,6,9,288477010u},
        {130,6,9,258954395u},
        {140,6,7,317999625u},
        {120,7,8,497209226u},
        {130,7,8,400189588u},
        {150,7,10,3904424161u},
        {140,6,9,288477010u},
        {130,6,9,258954395u},
        {140,6,7,317999625u},
        {120,7,8,497209226u},
        {130,7,8,400189588u},
        {150,7,10,3904424161u},
        {50,2,3,3517051158u},
        {50,2,4,3509128559u},
        {50,3,4,3524973757u},
        {30,2,3,9511118u},
        {40,2,4,4274625400u},
        {70,2,4,3334831381u},
        {50,2,3,3517051158u},
        {50,2,4,3509128559u},
        {50,3,4,3524973757u},
        {30,2,3,9511118u},
        {40,2,4,4274625400u},
        {70,2,4,3334831381u},
        {70,4,6,3509128559u},
        {40,4,6,607289132u},
        {90,4,5,2116000690u},
        {30,6,7,1311759367u},
        {110,4,5,2312759641u},
        {90,5,7,1191331178u},
        {70,4,6,3509128559u},
        {40,4,6,607289132u},
        {90,4,5,2116000690u},
        {30,6,7,1311759367u},
        {110,4,5,2312759641u},
        {90,5,7,1191331178u},
        {140,6,9,288477010u},
        {130,6,9,258954395u},
        {140,6,7,317999625u},
        {120,7,8,497209226u},
        {130,7,8,400189588u},
        {150,7,10,3904424161u},
        {140,6,9,288477010u},
        {130,6,9,258954395u},
        {140,6,7,317999625u},
        {120,7,8,497209226u},
        {130,7,8,400189588u},
        {150,7,10,3904424161u},
        {110,4,5,3517051158u},
        {70,4,6,3509128559u},
        {70,4,5,3524973757u},
        {30,4,5,9511118u},
        {80,6,8,4274625400u},
        {50,6,8,3334831381u},
        {110,4,5,3517051158u},
        {70,4,6,3509128559u},
        {70,4,5,3524973757u},
        {30,4,5,9511118u},
        {80,6,8,4274625400u},
        {50,6,8,3334831381u},
        {160,5,7,3517051158u},
        {130,5,6,3509128559u},
        {110,5,7,3524973757u},
        {90,5,7,9511118u},
        {130,5,6,4274625400u},
        {90,6,8,3334831381u},
        {160,5,7,3517051158u},
        {130,5,6,3509128559u},
        {110,5,7,3524973757u},
        {90,5,7,9511118u},
        {130,5,6,4274625400u},
        {90,6,8,3334831381u},
        {120,7,10,607289132u},
        {120,8,9,401550301u},
        {160,6,10,813027963u},
        {110,6,8,1285220388u},
        {110,6,7,542864062u},
        {140,9,11,170263315u},
        {120,7,10,607289132u},
        {120,8,9,401550301u},
        {160,6,10,813027963u},
        {110,6,8,1285220388u},
        {110,6,7,542864062u},
        {140,9,11,170263315u},
        {50,2,3,3517051158u},
        {50,2,4,3509128559u},
        {50,3,4,3524973757u},
        {30,2,3,9511118u},
        {40,2,4,4274625400u},
        {70,2,4,3334831381u},
        {50,2,3,3517051158u},
        {50,2,4,3509128559u},
        {50,3,4,3524973757u},
        {30,2,3,9511118u},
        {40,2,4,4274625400u},
        {70,2,4,3334831381u},
        {110,5,7,1465832913u},
        {110,4,5,3517051158u},
        {50,6,8,3709581964u},
        {80,4,6,2516634665u},
        {80,5,6,774936139u},
        {60,4,6,1399212292u},
        {110,5,7,1465832913u},
        {110,4,5,3517051158u},
        {50,6,8,3709581964u},
        {80,4,6,2516634665u},
        {80,5,6,774936139u},
        {60,4,6,1399212292u},
    };
    for (std::size_t n = 0; n < std::size(vitals_observations); ++n) {
        const auto& source = creation_observations[n]; const auto& expected = vitals_observations[n];
        Character draft{"Vitals",source.race,source.gender,source.character_class,
            std::string_view(source.character_class) == "thief" ? "neutral-good" : "lawful-good",{}};
        HighWordLcg32 rng(source.seed,profile.random->multiplier(),1);
        const auto rolled = roll_character(complete_rules,draft,rng);
        assert(rolled.age == source.age && rolled.hit_points);
        assert(rolled.portrait && rolled.portrait->head == 0 && rolled.portrait->body == 1);
        for (std::size_t i = 0; i < 6; ++i) assert(rolled.abilities[i].value == source.scores[i]);
        assert(rolled.abilities[0].percentile == source.percentile);
        assert(rolled.money.at("gold") == expected.gold && rolled.hit_points->base == expected.base);
        assert(rolled.hit_points->current == expected.hp && rolled.hit_points->maximum == expected.hp);
        assert(rolled.spellbook);
        const std::vector<std::string> divine{"divine.bless","divine.curse","divine.cure-light-wounds",
            "divine.cause-light-wounds","divine.detect-magic","divine.protection-from-evil",
            "divine.protection-from-good","divine.resist-cold"};
        const std::vector<std::string> arcane{"arcane.detect-magic","arcane.read-magic","arcane.shield","arcane.sleep"};
        if (draft.character_class == "cleric") {
            assert(rolled.spellbook->known == divine);
            const int original_slots[]{1,1,1,1,1,1,1,1,1,1,2,3,3,3,3,3,3,3}; // Original WIS 3..20 probe.
            assert(rolled.spellbook->first_level_slots == original_slots[rolled.abilities[2].value-3]);
        } else if (draft.character_class == "magic-user") {
            assert(rolled.spellbook->known == arcane && rolled.spellbook->first_level_slots == 1);
        } else assert(rolled.spellbook->known.empty() && rolled.spellbook->first_level_slots == 0);
        assert(rng.state() == expected.after);
        const auto loaded = read_party(serialize_party({rolled},complete_rules),complete_rules)[0];
        assert(loaded.money == rolled.money && loaded.hit_points->base == expected.base);
        assert(loaded.hit_points->current == expected.hp && loaded.hit_points->maximum == expected.hp);
        assert(loaded.spellbook->known == rolled.spellbook->known &&
            loaded.spellbook->first_level_slots == rolled.spellbook->first_level_slots);
    }
    struct CompoundObservation {
        const char* race; const char* character_class; const char* gender; std::uint32_t seed;
        int age; std::array<int,6> scores; int percentile, gold, base, hp; std::uint32_t after;
        int divine_slots, arcane_slots;
    };
    const CompoundObservation compound_observations[]{
        {"dwarf","fighter-thief","male",0u,60,{17,13,14,16,18,13},0,80,6,8,288477010u,0,0},
        {"dwarf","fighter-thief","male",1u,60,{15,13,14,15,18,14},0,85,5,7,258954395u,0,0},
        {"dwarf","fighter-thief","male",4294967295u,60,{15,15,13,14,16,12},0,100,5,7,317999625u,0,0},
        {"dwarf","fighter-thief","male",305419896u,60,{14,14,15,14,16,14},0,90,5,7,497209226u,0,0},
        {"dwarf","fighter-thief","male",776800562u,60,{14,15,15,15,16,16},0,90,5,7,400189588u,0,0},
        {"dwarf","fighter-thief","male",23u,60,{15,14,12,17,18,15},0,100,7,9,3904424161u,0,0},
        {"dwarf","fighter-thief","female",0u,60,{17,13,14,16,18,13},0,80,6,8,288477010u,0,0},
        {"dwarf","fighter-thief","female",1u,60,{15,13,14,15,18,14},0,85,5,7,258954395u,0,0},
        {"dwarf","fighter-thief","female",4294967295u,60,{15,15,13,14,16,12},0,100,5,7,317999625u,0,0},
        {"dwarf","fighter-thief","female",305419896u,60,{14,14,15,14,16,14},0,90,5,7,497209226u,0,0},
        {"dwarf","fighter-thief","female",776800562u,60,{14,15,15,15,16,16},0,90,5,7,400189588u,0,0},
        {"dwarf","fighter-thief","female",23u,60,{15,14,12,17,18,15},0,100,7,9,3904424161u,0,0},
        {"elf","fighter-magic-user","male",0u,180,{17,13,14,17,16,14},0,80,5,7,288477010u,0,1},
        {"elf","fighter-magic-user","male",1u,180,{15,13,14,16,16,15},0,95,5,7,258954395u,0,1},
        {"elf","fighter-magic-user","male",4294967295u,180,{15,15,13,15,14,13},0,100,4,4,317999625u,0,1},
        {"elf","fighter-magic-user","male",305419896u,180,{14,14,15,15,14,15},0,80,4,4,497209226u,0,1},
        {"elf","fighter-magic-user","male",776800562u,180,{14,15,15,16,14,17},0,80,4,4,400189588u,0,1},
        {"elf","fighter-magic-user","male",23u,180,{15,14,12,18,16,16},0,100,6,8,3904424161u,0,1},
        {"elf","fighter-magic-user","female",0u,180,{16,13,14,17,16,14},0,80,5,7,288477010u,0,1},
        {"elf","fighter-magic-user","female",1u,180,{15,13,14,16,16,15},0,95,5,7,258954395u,0,1},
        {"elf","fighter-magic-user","female",4294967295u,180,{15,15,13,15,14,13},0,100,4,4,317999625u,0,1},
        {"elf","fighter-magic-user","female",305419896u,180,{14,14,15,15,14,15},0,80,4,4,497209226u,0,1},
        {"elf","fighter-magic-user","female",776800562u,180,{14,15,15,16,14,17},0,80,4,4,400189588u,0,1},
        {"elf","fighter-magic-user","female",23u,180,{15,14,12,18,16,16},0,100,6,8,3904424161u,0,1},
        {"elf","fighter-thief","male",0u,160,{16,13,13,17,16,14},0,80,6,8,288477010u,0,0},
        {"elf","fighter-thief","male",1u,160,{14,13,13,16,16,15},0,85,5,7,258954395u,0,0},
        {"elf","fighter-thief","male",4294967295u,160,{14,15,12,15,14,13},0,100,5,5,317999625u,0,0},
        {"elf","fighter-thief","male",305419896u,160,{13,14,14,15,14,15},0,90,5,5,497209226u,0,0},
        {"elf","fighter-thief","male",776800562u,160,{13,15,14,16,14,17},0,90,5,5,400189588u,0,0},
        {"elf","fighter-thief","male",23u,160,{14,14,11,18,16,16},0,100,7,9,3904424161u,0,0},
        {"elf","fighter-thief","female",0u,160,{16,13,13,17,16,14},0,80,6,8,288477010u,0,0},
        {"elf","fighter-thief","female",1u,160,{14,13,13,16,16,15},0,85,5,7,258954395u,0,0},
        {"elf","fighter-thief","female",4294967295u,160,{14,15,12,15,14,13},0,100,5,5,317999625u,0,0},
        {"elf","fighter-thief","female",305419896u,160,{13,14,14,15,14,15},0,90,5,5,497209226u,0,0},
        {"elf","fighter-thief","female",776800562u,160,{13,15,14,16,14,17},0,90,5,5,400189588u,0,0},
        {"elf","fighter-thief","female",23u,160,{14,14,11,18,16,16},0,100,7,9,3904424161u,0,0},
        {"elf","fighter-magic-user-thief","male",0u,180,{17,13,14,17,16,14},0,73,4,6,2890333993u,0,1},
        {"elf","fighter-magic-user-thief","male",1u,180,{15,13,14,16,16,15},0,90,5,7,977018830u,0,1},
        {"elf","fighter-magic-user-thief","male",4294967295u,180,{15,15,13,15,14,13},0,100,4,4,508681860u,0,1},
        {"elf","fighter-magic-user-thief","male",305419896u,180,{14,14,15,15,14,15},0,76,5,5,3416898177u,0,1},
        {"elf","fighter-magic-user-thief","male",776800562u,180,{14,15,15,16,14,17},0,90,5,5,2079206499u,0,1},
        {"elf","fighter-magic-user-thief","male",23u,180,{15,14,12,18,16,16},0,96,5,7,1833758204u,0,1},
        {"elf","fighter-magic-user-thief","female",0u,180,{16,13,14,17,16,14},0,73,4,6,2890333993u,0,1},
        {"elf","fighter-magic-user-thief","female",1u,180,{15,13,14,16,16,15},0,90,5,7,977018830u,0,1},
        {"elf","fighter-magic-user-thief","female",4294967295u,180,{15,15,13,15,14,13},0,100,4,4,508681860u,0,1},
        {"elf","fighter-magic-user-thief","female",305419896u,180,{14,14,15,15,14,15},0,76,5,5,3416898177u,0,1},
        {"elf","fighter-magic-user-thief","female",776800562u,180,{14,15,15,16,14,17},0,90,5,5,2079206499u,0,1},
        {"elf","fighter-magic-user-thief","female",23u,180,{15,14,12,18,16,16},0,96,5,7,1833758204u,0,1},
        {"elf","magic-user-thief","male",0u,180,{17,13,14,17,16,14},0,75,3,5,3509128559u,0,1},
        {"elf","magic-user-thief","male",1u,180,{15,13,14,16,16,15},0,60,3,5,607289132u,0,1},
        {"elf","magic-user-thief","male",4294967295u,180,{15,15,13,15,14,13},0,65,3,3,2116000690u,0,1},
        {"elf","magic-user-thief","male",305419896u,180,{14,14,15,15,14,15},0,35,4,4,1311759367u,0,1},
        {"elf","magic-user-thief","male",776800562u,180,{14,15,15,16,14,17},0,60,3,3,2312759641u,0,1},
        {"elf","magic-user-thief","male",23u,180,{15,14,12,18,16,16},0,50,3,5,1191331178u,0,1},
        {"elf","magic-user-thief","female",0u,180,{16,13,14,17,16,14},0,75,3,5,3509128559u,0,1},
        {"elf","magic-user-thief","female",1u,180,{15,13,14,16,16,15},0,60,3,5,607289132u,0,1},
        {"elf","magic-user-thief","female",4294967295u,180,{15,15,13,15,14,13},0,65,3,3,2116000690u,0,1},
        {"elf","magic-user-thief","female",305419896u,180,{14,14,15,15,14,15},0,35,4,4,1311759367u,0,1},
        {"elf","magic-user-thief","female",776800562u,180,{14,15,15,16,14,17},0,60,3,3,2312759641u,0,1},
        {"elf","magic-user-thief","female",23u,180,{15,14,12,18,16,16},0,50,3,5,1191331178u,0,1},
        {"gnome","fighter-thief","male",0u,80,{16,13,13,16,17,14},0,80,6,8,288477010u,0,0},
        {"gnome","fighter-thief","male",1u,80,{14,13,13,15,17,15},0,85,5,7,258954395u,0,0},
        {"gnome","fighter-thief","male",4294967295u,80,{14,15,12,14,15,13},0,100,5,6,317999625u,0,0},
        {"gnome","fighter-thief","male",305419896u,80,{13,14,14,14,15,15},0,90,5,6,497209226u,0,0},
        {"gnome","fighter-thief","male",776800562u,80,{13,15,14,15,15,17},0,90,5,6,400189588u,0,0},
        {"gnome","fighter-thief","male",23u,80,{14,14,11,17,17,16},0,100,7,9,3904424161u,0,0},
        {"gnome","fighter-thief","female",0u,80,{15,13,13,16,17,14},0,80,6,8,288477010u,0,0},
        {"gnome","fighter-thief","female",1u,80,{14,13,13,15,17,15},0,85,5,7,258954395u,0,0},
        {"gnome","fighter-thief","female",4294967295u,80,{14,15,12,14,15,13},0,100,5,6,317999625u,0,0},
        {"gnome","fighter-thief","female",305419896u,80,{13,14,14,14,15,15},0,90,5,6,497209226u,0,0},
        {"gnome","fighter-thief","female",776800562u,80,{13,15,14,15,15,17},0,90,5,6,400189588u,0,0},
        {"gnome","fighter-thief","female",23u,80,{14,14,11,17,17,16},0,100,7,9,3904424161u,0,0},
        {"half-elf","cleric-fighter","male",0u,48,{17,13,14,16,17,14},0,140,5,7,258954395u,3,0},
        {"half-elf","cleric-fighter","male",1u,48,{15,13,14,15,17,15},0,145,8,10,832869128u,3,0},
        {"half-elf","cleric-fighter","male",4294967295u,48,{15,15,13,14,15,13},0,125,5,6,3980006958u,2,0},
        {"half-elf","cleric-fighter","male",305419896u,48,{14,14,15,14,15,15},0,130,6,7,1400976819u,3,0},
        {"half-elf","cleric-fighter","male",776800562u,48,{14,15,15,15,15,17},0,120,5,6,4191181541u,3,0},
        {"half-elf","cleric-fighter","male",23u,48,{15,14,13,17,17,16},0,120,7,9,574091366u,2,0},
        {"half-elf","cleric-fighter","female",0u,48,{17,13,14,16,17,14},0,140,5,7,258954395u,3,0},
        {"half-elf","cleric-fighter","female",1u,48,{15,13,14,15,17,15},0,145,8,10,832869128u,3,0},
        {"half-elf","cleric-fighter","female",4294967295u,48,{15,15,13,14,15,13},0,125,5,6,3980006958u,2,0},
        {"half-elf","cleric-fighter","female",305419896u,48,{14,14,15,14,15,15},0,130,6,7,1400976819u,3,0},
        {"half-elf","cleric-fighter","female",776800562u,48,{14,15,15,15,15,17},0,120,5,6,4191181541u,3,0},
        {"half-elf","cleric-fighter","female",23u,48,{15,14,13,17,17,16},0,120,7,9,574091366u,2,0},
        {"half-elf","cleric-fighter-magic-user","male",0u,48,{17,13,14,16,17,14},0,113,4,6,977018830u,3,1},
        {"half-elf","cleric-fighter-magic-user","male",1u,48,{15,13,14,15,17,15},0,113,6,8,2449659655u,3,1},
        {"half-elf","cleric-fighter-magic-user","male",4294967295u,48,{15,15,13,14,15,13},0,96,4,5,3799345301u,2,1},
        {"half-elf","cleric-fighter-magic-user","male",305419896u,48,{14,14,15,14,15,15},0,103,6,7,1518545030u,3,1},
        {"half-elf","cleric-fighter-magic-user","male",776800562u,48,{14,15,15,15,15,17},0,93,5,6,3047287280u,3,1},
        {"half-elf","cleric-fighter-magic-user","male",23u,48,{15,14,13,17,17,16},0,96,4,6,488019437u,2,1},
        {"half-elf","cleric-fighter-magic-user","female",0u,48,{17,13,14,16,17,14},0,113,4,6,977018830u,3,1},
        {"half-elf","cleric-fighter-magic-user","female",1u,48,{15,13,14,15,17,15},0,113,6,8,2449659655u,3,1},
        {"half-elf","cleric-fighter-magic-user","female",4294967295u,48,{15,15,13,14,15,13},0,96,4,5,3799345301u,2,1},
        {"half-elf","cleric-fighter-magic-user","female",305419896u,48,{14,14,15,14,15,15},0,103,6,7,1518545030u,3,1},
        {"half-elf","cleric-fighter-magic-user","female",776800562u,48,{14,15,15,15,15,17},0,93,5,6,3047287280u,3,1},
        {"half-elf","cleric-fighter-magic-user","female",23u,48,{15,14,13,17,17,16},0,96,4,6,488019437u,2,1},
        {"half-elf","cleric-magic-user","male",0u,48,{17,13,14,16,17,14},0,95,4,6,607289132u,3,1},
        {"half-elf","cleric-magic-user","male",1u,48,{15,13,14,15,17,15},0,100,4,6,401550301u,3,1},
        {"half-elf","cleric-magic-user","male",4294967295u,48,{15,15,13,14,15,13},0,90,5,6,813027963u,2,1},
        {"half-elf","cleric-magic-user","male",305419896u,48,{14,14,15,14,15,15},0,85,5,6,1285220388u,3,1},
        {"half-elf","cleric-magic-user","male",776800562u,48,{14,15,15,15,15,17},0,70,4,5,542864062u,3,1},
        {"half-elf","cleric-magic-user","male",23u,48,{15,14,13,17,17,16},0,70,4,6,170263315u,2,1},
        {"half-elf","cleric-magic-user","female",0u,48,{17,13,14,16,17,14},0,95,4,6,607289132u,3,1},
        {"half-elf","cleric-magic-user","female",1u,48,{15,13,14,15,17,15},0,100,4,6,401550301u,3,1},
        {"half-elf","cleric-magic-user","female",4294967295u,48,{15,15,13,14,15,13},0,90,5,6,813027963u,2,1},
        {"half-elf","cleric-magic-user","female",305419896u,48,{14,14,15,14,15,15},0,85,5,6,1285220388u,3,1},
        {"half-elf","cleric-magic-user","female",776800562u,48,{14,15,15,15,15,17},0,70,4,5,542864062u,3,1},
        {"half-elf","cleric-magic-user","female",23u,48,{15,14,13,17,17,16},0,70,4,6,170263315u,2,1},
        {"half-elf","fighter-magic-user","male",0u,46,{17,13,14,16,17,14},0,80,5,7,288477010u,0,1},
        {"half-elf","fighter-magic-user","male",1u,46,{15,13,14,15,17,15},0,95,5,7,258954395u,0,1},
        {"half-elf","fighter-magic-user","male",4294967295u,46,{15,15,13,14,15,13},0,100,4,5,317999625u,0,1},
        {"half-elf","fighter-magic-user","male",305419896u,46,{14,14,15,14,15,15},0,80,4,5,497209226u,0,1},
        {"half-elf","fighter-magic-user","male",776800562u,46,{14,15,15,15,15,17},0,80,4,5,400189588u,0,1},
        {"half-elf","fighter-magic-user","male",23u,46,{15,14,12,17,17,16},0,100,6,8,3904424161u,0,1},
        {"half-elf","fighter-magic-user","female",0u,46,{17,13,14,16,17,14},0,80,5,7,288477010u,0,1},
        {"half-elf","fighter-magic-user","female",1u,46,{15,13,14,15,17,15},0,95,5,7,258954395u,0,1},
        {"half-elf","fighter-magic-user","female",4294967295u,46,{15,15,13,14,15,13},0,100,4,5,317999625u,0,1},
        {"half-elf","fighter-magic-user","female",305419896u,46,{14,14,15,14,15,15},0,80,4,5,497209226u,0,1},
        {"half-elf","fighter-magic-user","female",776800562u,46,{14,15,15,15,15,17},0,80,4,5,400189588u,0,1},
        {"half-elf","fighter-magic-user","female",23u,46,{15,14,12,17,17,16},0,100,6,8,3904424161u,0,1},
        {"half-elf","fighter-thief","male",0u,34,{16,13,13,16,17,14},0,80,6,8,288477010u,0,0},
        {"half-elf","fighter-thief","male",1u,34,{14,13,13,15,17,15},0,85,5,7,258954395u,0,0},
        {"half-elf","fighter-thief","male",4294967295u,34,{14,15,12,14,15,13},0,100,5,6,317999625u,0,0},
        {"half-elf","fighter-thief","male",305419896u,34,{13,14,14,14,15,15},0,90,5,6,497209226u,0,0},
        {"half-elf","fighter-thief","male",776800562u,34,{13,15,14,15,15,17},0,90,5,6,400189588u,0,0},
        {"half-elf","fighter-thief","male",23u,34,{14,14,11,17,17,16},0,100,7,9,3904424161u,0,0},
        {"half-elf","fighter-thief","female",0u,34,{16,13,13,16,17,14},0,80,6,8,288477010u,0,0},
        {"half-elf","fighter-thief","female",1u,34,{14,13,13,15,17,15},0,85,5,7,258954395u,0,0},
        {"half-elf","fighter-thief","female",4294967295u,34,{14,15,12,14,15,13},0,100,5,6,317999625u,0,0},
        {"half-elf","fighter-thief","female",305419896u,34,{13,14,14,14,15,15},0,90,5,6,497209226u,0,0},
        {"half-elf","fighter-thief","female",776800562u,34,{13,15,14,15,15,17},0,90,5,6,400189588u,0,0},
        {"half-elf","fighter-thief","female",23u,34,{14,14,11,17,17,16},0,100,7,9,3904424161u,0,0},
        {"half-elf","fighter-magic-user-thief","male",0u,46,{17,13,14,16,17,14},0,73,4,6,2890333993u,0,1},
        {"half-elf","fighter-magic-user-thief","male",1u,46,{15,13,14,15,17,15},0,90,5,7,977018830u,0,1},
        {"half-elf","fighter-magic-user-thief","male",4294967295u,46,{15,15,13,14,15,13},0,100,4,5,508681860u,0,1},
        {"half-elf","fighter-magic-user-thief","male",305419896u,46,{14,14,15,14,15,15},0,76,5,6,3416898177u,0,1},
        {"half-elf","fighter-magic-user-thief","male",776800562u,46,{14,15,15,15,15,17},0,90,5,6,2079206499u,0,1},
        {"half-elf","fighter-magic-user-thief","male",23u,46,{15,14,12,17,17,16},0,96,5,7,1833758204u,0,1},
        {"half-elf","fighter-magic-user-thief","female",0u,46,{17,13,14,16,17,14},0,73,4,6,2890333993u,0,1},
        {"half-elf","fighter-magic-user-thief","female",1u,46,{15,13,14,15,17,15},0,90,5,7,977018830u,0,1},
        {"half-elf","fighter-magic-user-thief","female",4294967295u,46,{15,15,13,14,15,13},0,100,4,5,508681860u,0,1},
        {"half-elf","fighter-magic-user-thief","female",305419896u,46,{14,14,15,14,15,15},0,76,5,6,3416898177u,0,1},
        {"half-elf","fighter-magic-user-thief","female",776800562u,46,{14,15,15,15,15,17},0,90,5,6,2079206499u,0,1},
        {"half-elf","fighter-magic-user-thief","female",23u,46,{15,14,12,17,17,16},0,96,5,7,1833758204u,0,1},
        {"half-elf","magic-user-thief","male",0u,46,{17,13,14,16,17,14},0,75,3,5,3509128559u,0,1},
        {"half-elf","magic-user-thief","male",1u,46,{15,13,14,15,17,15},0,60,3,5,607289132u,0,1},
        {"half-elf","magic-user-thief","male",4294967295u,46,{15,15,13,14,15,13},0,65,3,4,2116000690u,0,1},
        {"half-elf","magic-user-thief","male",305419896u,46,{14,14,15,14,15,15},0,35,4,5,1311759367u,0,1},
        {"half-elf","magic-user-thief","male",776800562u,46,{14,15,15,15,15,17},0,60,3,4,2312759641u,0,1},
        {"half-elf","magic-user-thief","male",23u,46,{15,14,12,17,17,16},0,50,3,5,1191331178u,0,1},
        {"half-elf","magic-user-thief","female",0u,46,{17,13,14,16,17,14},0,75,3,5,3509128559u,0,1},
        {"half-elf","magic-user-thief","female",1u,46,{15,13,14,15,17,15},0,60,3,5,607289132u,0,1},
        {"half-elf","magic-user-thief","female",4294967295u,46,{15,15,13,14,15,13},0,65,3,4,2116000690u,0,1},
        {"half-elf","magic-user-thief","female",305419896u,46,{14,14,15,14,15,15},0,35,4,5,1311759367u,0,1},
        {"half-elf","magic-user-thief","female",776800562u,46,{14,15,15,15,15,17},0,60,3,4,2312759641u,0,1},
        {"half-elf","magic-user-thief","female",23u,46,{15,14,12,17,17,16},0,50,3,5,1191331178u,0,1},
        {"halfling","fighter-thief","male",0u,32,{15,13,13,17,17,14},0,80,6,8,288477010u,0,0},
        {"halfling","fighter-thief","male",1u,32,{13,13,13,16,17,15},0,85,5,7,258954395u,0,0},
        {"halfling","fighter-thief","male",4294967295u,32,{13,15,12,15,15,13},0,100,5,6,317999625u,0,0},
        {"halfling","fighter-thief","male",305419896u,32,{12,14,14,15,15,15},0,90,5,6,497209226u,0,0},
        {"halfling","fighter-thief","male",776800562u,32,{12,15,14,16,15,17},0,90,5,6,400189588u,0,0},
        {"halfling","fighter-thief","male",23u,32,{13,14,11,18,17,16},0,100,7,9,3904424161u,0,0},
        {"halfling","fighter-thief","female",0u,32,{14,13,13,17,17,14},0,80,6,8,288477010u,0,0},
        {"halfling","fighter-thief","female",1u,32,{13,13,13,16,17,15},0,85,5,7,258954395u,0,0},
        {"halfling","fighter-thief","female",4294967295u,32,{13,15,12,15,15,13},0,100,5,6,317999625u,0,0},
        {"halfling","fighter-thief","female",305419896u,32,{12,14,14,15,15,15},0,90,5,6,497209226u,0,0},
        {"halfling","fighter-thief","female",776800562u,32,{12,15,14,16,15,17},0,90,5,6,400189588u,0,0},
        {"halfling","fighter-thief","female",23u,32,{13,14,11,18,17,16},0,100,7,9,3904424161u,0,0},
        {"half-elf","cleric-fighter","male",8u,48,{18,15,17,10,15,14},39,90,7,8,112718864u,3,0},
        {"half-elf","cleric-fighter","male",15u,48,{18,15,16,14,16,17},90,85,6,8,1630071031u,3,0},
        {"half-elf","cleric-fighter","male",18u,48,{18,14,13,16,16,13},50,110,6,8,3507498330u,2,0},
        {"half-elf","cleric-fighter","female",8u,48,{17,15,15,11,14,15},0,105,6,6,555304963u,3,0},
        {"half-elf","cleric-fighter","female",15u,48,{17,15,17,16,17,16},0,65,6,8,277740798u,3,0},
        {"half-elf","cleric-fighter","female",18u,48,{17,14,13,15,15,15},0,115,8,9,1999484997u,2,0},
        {"half-elf","cleric-fighter-magic-user","male",8u,48,{18,15,17,10,15,14},39,80,4,5,186292207u,3,1},
        {"half-elf","cleric-fighter-magic-user","male",15u,48,{18,15,16,14,16,17},90,66,6,8,3574554810u,3,1},
        {"half-elf","cleric-fighter-magic-user","male",18u,48,{18,14,13,16,16,13},50,83,4,6,3799533841u,2,1},
        {"half-elf","cleric-fighter-magic-user","female",8u,48,{17,15,15,11,14,15},0,83,5,5,4168210838u,3,1},
        {"half-elf","cleric-fighter-magic-user","female",15u,48,{17,15,17,16,17,16},0,53,4,6,1591794725u,3,1},
        {"half-elf","cleric-fighter-magic-user","female",18u,48,{17,14,13,15,15,15},0,90,4,5,1714749904u,2,1},
        {"half-elf","cleric-magic-user","male",8u,48,{18,15,15,11,14,15},0,75,4,4,3256345780u,3,1},
        {"half-elf","cleric-magic-user","male",15u,48,{18,15,17,16,17,16},0,30,4,6,1816173963u,3,1},
        {"half-elf","cleric-magic-user","male",18u,48,{18,14,13,15,15,15},0,65,4,5,1198957470u,2,1},
        {"half-elf","cleric-magic-user","female",8u,48,{17,15,15,11,14,15},0,75,4,4,3256345780u,3,1},
        {"half-elf","cleric-magic-user","female",15u,48,{17,15,17,16,17,16},0,30,4,6,1816173963u,3,1},
        {"half-elf","cleric-magic-user","female",18u,48,{17,14,13,15,15,15},0,65,4,5,1198957470u,2,1},
        {"half-elf","fighter-magic-user","male",8u,46,{18,15,17,10,15,14},39,90,5,6,555304963u,0,1},
        {"half-elf","fighter-magic-user","male",15u,46,{18,15,16,14,16,17},90,80,4,6,277740798u,0,1},
        {"half-elf","fighter-magic-user","male",18u,46,{18,14,13,16,16,13},50,100,6,8,1999484997u,0,1},
        {"half-elf","fighter-magic-user","female",8u,46,{17,15,15,11,14,15},0,90,4,4,52296090u,0,1},
        {"half-elf","fighter-magic-user","female",15u,46,{17,15,17,16,17,16},0,80,4,6,4140605081u,0,1},
        {"half-elf","fighter-magic-user","female",18u,46,{17,14,13,15,15,15},0,100,4,5,4052037236u,0,1},
        {"half-elf","fighter-thief","male",8u,34,{18,15,16,10,15,14},39,90,6,7,555304963u,0,0},
        {"half-elf","fighter-thief","male",15u,34,{18,15,15,14,16,17},90,100,5,7,277740798u,0,0},
        {"half-elf","fighter-thief","male",18u,34,{18,14,12,16,16,13},50,100,8,10,1999484997u,0,0},
        {"half-elf","fighter-thief","female",8u,34,{17,15,14,11,14,15},0,90,6,6,52296090u,0,0},
        {"half-elf","fighter-thief","female",15u,34,{17,15,16,16,17,16},0,100,5,7,4140605081u,0,0},
        {"half-elf","fighter-thief","female",18u,34,{17,14,12,15,15,15},0,90,6,7,4052037236u,0,0},
        {"half-elf","fighter-magic-user-thief","male",8u,46,{18,15,17,10,15,14},39,86,5,6,4168210838u,0,1},
        {"half-elf","fighter-magic-user-thief","male",15u,46,{18,15,16,14,16,17},90,63,4,6,1591794725u,0,1},
        {"half-elf","fighter-magic-user-thief","male",18u,46,{18,14,13,16,16,13},50,106,5,7,1714749904u,0,1},
        {"half-elf","fighter-magic-user-thief","female",8u,46,{17,15,15,11,14,15},0,90,4,4,468714577u,0,1},
        {"half-elf","fighter-magic-user-thief","female",15u,46,{17,15,17,16,17,16},0,66,5,7,4255377620u,0,1},
        {"half-elf","fighter-magic-user-thief","female",18u,46,{17,14,13,15,15,15},0,103,5,6,2810399427u,0,1},
        {"half-elf","magic-user-thief","male",8u,46,{18,15,15,11,14,15},0,40,3,3,1769249623u,0,1},
        {"half-elf","magic-user-thief","male",15u,46,{18,15,17,16,17,16},0,65,3,5,2931210114u,0,1},
        {"half-elf","magic-user-thief","male",18u,46,{18,14,13,15,15,15},0,65,3,4,2815626425u,0,1},
        {"half-elf","magic-user-thief","female",8u,46,{17,15,15,11,14,15},0,40,3,3,1769249623u,0,1},
        {"half-elf","magic-user-thief","female",15u,46,{17,15,17,16,17,16},0,65,3,5,2931210114u,0,1},
        {"half-elf","magic-user-thief","female",18u,46,{17,14,13,15,15,15},0,65,3,4,2815626425u,0,1},
    };
    for (const auto& expected : compound_observations) {
        Character draft{"Multi",expected.race,expected.gender,expected.character_class,"neutral-good",{}};
        HighWordLcg32 rng(expected.seed,profile.random->multiplier(),1);
        const auto rolled = roll_character(complete_rules,draft,rng);
        assert(rolled.age == expected.age && rolled.hit_points);
        for (std::size_t i = 0; i < 6; ++i) {
            if (rolled.abilities[i].value != expected.scores[i]) std::cerr << expected.race << ' ' << expected.character_class
                << ' ' << expected.seed << " ability " << i << ": " << rolled.abilities[i].value << " expected " << expected.scores[i] << '\n';
            assert(rolled.abilities[i].value == expected.scores[i]);
        }
        assert(rolled.abilities[0].percentile == expected.percentile);
        assert(rolled.money.at("gold") == expected.gold && rolled.hit_points->base == expected.base);
        assert(rolled.hit_points->current == expected.hp && rolled.hit_points->maximum == expected.hp);
        assert(rng.state() == expected.after);
        if (expected.divine_slots) assert(rolled.spellbooks.at("divine").first_level_slots == expected.divine_slots &&
            rolled.spellbooks.at("divine").known.size() == 8);
        if (expected.arcane_slots) assert(rolled.spellbooks.at("arcane").first_level_slots == expected.arcane_slots &&
            rolled.spellbooks.at("arcane").known.size() == 4);
        assert(rolled.spellbooks.size() == static_cast<std::size_t>((expected.divine_slots != 0)+(expected.arcane_slots != 0)));
        const auto loaded = read_party(serialize_party({rolled},complete_rules),complete_rules)[0];
        for (const auto& [name,book] : rolled.spellbooks) {
            assert(loaded.spellbooks.at(name).known == book.known);
            assert(loaded.spellbooks.at(name).first_level_slots == book.first_level_slots);
        }
        assert(loaded.money == rolled.money && loaded.hit_points->maximum == expected.hp);
    }
    auto compound_session = session_for(profile);
    compound_session.state.random.emplace(8,profile.random->multiplier(),1);
    Character compound_pc{"Dual","half-elf","male","cleric-fighter-magic-user","neutral-good",{}};
    compound_pc = roll_character(complete_rules,compound_pc,*compound_session.state.random);
    add_character(compound_session.party,complete_rules,compound_pc);
    auto restored_compound = session_for(profile);
    restore_exploration(restored_compound,serialize_exploration(compound_session,"compound-replay"),"compound-replay");
    const auto rerolled_compound = roll_character(complete_rules,compound_pc,*compound_session.state.random);
    const auto replayed_compound = roll_character(complete_rules,restored_compound.party[0],*restored_compound.state.random);
    same(rerolled_compound.abilities,replayed_compound.abilities);
    assert(rerolled_compound.age == compound_pc.age && rerolled_compound.money == replayed_compound.money);
    assert(rerolled_compound.hit_points->current == replayed_compound.hit_points->current);
    for (const auto& [name,book] : rerolled_compound.spellbooks) {
        assert(replayed_compound.spellbooks.at(name).known == book.known);
        assert(replayed_compound.spellbooks.at(name).first_level_slots == book.first_level_slots);
    }
    assert(compound_session.state.random->state() == restored_compound.state.random->state());
    for (const auto& components : std::vector<std::vector<std::string>>{
        {"missing","fighter"},{"cleric-fighter","fighter"},{"fighter","fighter"}}) {
        auto invalid = complete_rules; invalid.classes[5].components = components;
        HighWordLcg32 rng(8,profile.random->multiplier(),1);
        rejects([&] { (void)roll_character(invalid,compound_pc,rng); });
        assert(rng.state() == 8);
    }
    // Original isolated HP routine, CON 3..20. Synthetic ability clamps supply its inputs.
    const int hp_edges[4][18]{
        {3,4,4,4,5,5,5,5,5,5,5,5,6,7,7,7,7,7},
        {7,8,8,8,9,9,9,9,9,9,9,9,10,11,12,13,13,13},
        {1,1,1,1,2,2,2,2,2,2,2,2,3,4,4,4,4,4},
        {2,3,3,3,4,4,4,4,4,4,4,4,5,6,6,6,6,6}};
    for (std::size_t cls = 0; cls < 4; ++cls) for (int con = 3; con <= 20; ++con) {
        auto edge_rules = complete_rules;
        edge_rules.dice_count = 1; edge_rules.dice_sides = 2; edge_rules.drop_lowest = 0; edge_rules.keep_best_of = 1;
        auto& race = edge_rules.races[5]; race.age_adjustments.clear();
        race.minimum.assign(6,15); race.minimum[4] = con;
        race.maximum["male"] = race.minimum;
        auto& choice = edge_rules.classes[cls];
        choice.minimum.assign(6,1); choice.creation_minimum.clear(); choice.starting_money.reset();
        Character draft{"Edge","human","male",choice.id,choice.alignments.front(),{}}; draft.age = 1;
        HighWordLcg32 rng(0x12345678,profile.random->multiplier(),1);
        const auto rolled = roll_character(edge_rules,draft,rng);
        assert(rolled.abilities[4].value == con && rolled.hit_points->current == hp_edges[cls][con-3]);
        auto expected_rng = HighWordLcg32(0xe28f6d7a,profile.random->multiplier(),1);
        (void)expected_rng.draw(choice.hit_points->sides);
        assert(rng.state() == expected_rng.state());
    }
    const int original_spell_slots[]{1,1,1,1,1,1,1,1,1,1,2,3,3,3,3,3,3,3};
    for (int wisdom = 3; wisdom <= 20; ++wisdom) {
        auto spell_rules = complete_rules;
        spell_rules.dice_count = 1; spell_rules.dice_sides = 2; spell_rules.drop_lowest = 0; spell_rules.keep_best_of = 1;
        auto& race = spell_rules.races[5]; race.age_adjustments.clear();
        race.minimum.assign(6,15); race.minimum[2] = wisdom; race.maximum["male"] = race.minimum;
        auto& cleric = spell_rules.classes[0]; cleric.minimum.assign(6,1); cleric.creation_minimum.clear();
        cleric.starting_money.reset(); cleric.hit_points.reset();
        Character draft{"Spells","human","male","cleric","lawful-good",{}}; draft.age = 1;
        HighWordLcg32 rng(0x12345678,profile.random->multiplier(),1);
        const auto rolled = roll_character(spell_rules,draft,rng);
        assert(rolled.abilities[2].value == wisdom);
        assert(rolled.spellbook->first_level_slots == original_spell_slots[wisdom-3]);
        assert(rolled.spellbook->known.size() == 8 && rng.state() == 0xe28f6d7a);
    }
    auto complete_session = session_for(profile);
    complete_session.state.random.emplace(0x12345678,profile.random->multiplier(),1);
    auto original_roll = roll_character(complete_rules,Character{"Full","human","male","fighter","lawful-good",{}},
        *complete_session.state.random);
    add_character(complete_session.party,complete_rules,original_roll);
    const auto full_save = serialize_exploration(complete_session,"vitals-replay");
    auto full_restored = session_for(profile); restore_exploration(full_restored,full_save,"vitals-replay");
    const auto reroll = roll_character(complete_rules,original_roll,*complete_session.state.random);
    const auto replay_roll = roll_character(complete_rules,full_restored.party[0],*full_restored.state.random);
    assert(reroll.age == original_roll.age && replay_roll.age == original_roll.age);
    same(reroll.abilities,replay_roll.abilities);
    assert(reroll.money == replay_roll.money && reroll.hit_points->base == replay_roll.hit_points->base);
    assert(reroll.hit_points->current == replay_roll.hit_points->current);
    assert(reroll.spellbook->known == replay_roll.spellbook->known &&
        reroll.spellbook->first_level_slots == replay_roll.spellbook->first_level_slots);
    assert(complete_session.state.random->state() == full_restored.state.random->state());
    auto invalid_vitals = complete_rules; invalid_vitals.classes[1].hit_points->modifiers.clear();
    HighWordLcg32 vitals_rng(123,profile.random->multiplier(),1);
    rejects([&] { (void)roll_character(invalid_vitals,original_roll,vitals_rng); });
    assert(vitals_rng.state() == 123); // Failures after money rolling also leave the caller RNG untouched.
    invalid_vitals = complete_rules; invalid_vitals.classes[1].starting_money->multiplier = 65535;
    rejects([&] { (void)roll_character(invalid_vitals,original_roll,vitals_rng); });
    assert(vitals_rng.state() == 123);
    invalid_vitals = complete_rules;
    invalid_vitals.classes[1].starting_spells->known = {"duplicate","duplicate"};
    rejects([&] { (void)roll_character(invalid_vitals,original_roll,vitals_rng); });
    assert(vitals_rng.state() == 123);
    invalid_vitals.classes[1].starting_spells->first_level_slots = 2147483647;
    rejects([&] { (void)roll_character(invalid_vitals,original_roll,vitals_rng); });
    assert(vitals_rng.state() == 123);
    invalid_vitals = complete_rules; invalid_vitals.portrait->heads.clear();
    original_roll.portrait.reset();
    rejects([&] { (void)roll_character(invalid_vitals,original_roll,vitals_rng); });
    assert(vitals_rng.state() == 123);
    invalid_vitals = complete_rules; original_roll.portrait = CharacterPortrait{255,255};
    rejects([&] { (void)roll_character(invalid_vitals,original_roll,vitals_rng); });
    assert(vitals_rng.state() == 123);
    struct AgeObservation { const char* race; const char* character_class; int age; std::uint32_t after; };
    auto replay = session_for(profile);
    auto byte_rules = rules;
    byte_rules.dice_count = 1; byte_rules.dice_sides = 3; byte_rules.drop_lowest = 0; byte_rules.keep_best_of = 1;
    Character old_elf{"Byte","elf","male","magic-user","lawful-good",{}}; old_elf.age = 1201;
    HighWordLcg32 byte_rng(0x12345678,profile.random->multiplier(),1);
    const auto wrapped = roll_character(byte_rules,old_elf,byte_rng);
    assert(wrapped.abilities[4].value == 18 && byte_rng.state() == 0xe28f6d7a); // CON 2-3 wraps to 255 before cap.
    byte_rules.races[5].adjustments.assign(6,0); byte_rules.races[5].adjustments[0] = -1;
    Character zero_strength{"Zero","human","male","magic-user","lawful-good",{}}; zero_strength.age = 91;
    byte_rng = HighWordLcg32(0x12345678,profile.random->multiplier(),1);
    assert(roll_character(byte_rules,zero_strength,byte_rng).abilities[0].value == 3); // Zero guard prevents wrapping.
    replay.state.random.emplace(18,profile.random->multiplier(),1);
    Character replay_pc{"Replay","human","male","fighter","lawful-good",{}};
    replay_pc = roll_character(rules,replay_pc,*replay.state.random);
    assert(replay_pc.abilities[0].percentile == 29 && replay.state.random->state() == 1273674349);
    add_character(replay.party,rules,replay_pc);
    const auto replay_save = serialize_exploration(replay,"creation-replay");
    const auto next_roll = roll_character(rules,replay_pc,*replay.state.random);
    auto replay_fresh = session_for(profile);
    restore_exploration(replay_fresh,replay_save,"creation-replay");
    const auto next_fresh = roll_character(rules,replay_fresh.party[0],*replay_fresh.state.random);
    same(next_roll.abilities,next_fresh.abilities);
    assert(next_roll.age == replay_pc.age && next_fresh.age == replay_pc.age);
    assert(replay.state.random->state() == replay_fresh.state.random->state());
    auto invalid_creation = rules;
    invalid_creation.races[5].age_adjustments[0].values.clear();
    HighWordLcg32 preserved_rng(18,profile.random->multiplier(),1);
    rejects([&] { (void)roll_character(invalid_creation,Character{"","human","male","fighter","",{}},
        preserved_rng); });
    assert(preserved_rng.state() == 18);
    const AgeObservation ages[]{
        {"dwarf","fighter",52,3859354085u},{"dwarf","thief",82,3651437751u},
        {"elf","fighter",148,3859354085u},{"elf","magic-user",168,3859354085u},
        {"elf","thief",118,3859354085u},{"gnome","fighter",72,3859354085u},
        {"gnome","thief",92,3859354085u},{"half-elf","cleric",48,2046801342},
        {"half-elf","fighter",31,3651437751u},{"half-elf","magic-user",42,2046801342},
        {"half-elf","thief",39,3651437751u},{"halfling","fighter",29,3651437751u},
        {"halfling","thief",48,2046801342},{"human","cleric",22,3411775577u},
        {"human","fighter",19,3411775577u},{"human","magic-user",32,2046801342},
        {"human","thief",22,3411775577u}
    };
    for (const auto& observed : ages) {
        HighWordLcg32 rng(0x12345678,profile.random->multiplier(),1);
        assert(roll_starting_age(rules,observed.race,observed.character_class,rng) == observed.age);
        assert(rng.state() == observed.after);
    }
    auto no_age = rules;
    for (auto& race : no_age.races) race.starting_ages.clear();
    HighWordLcg32 no_age_rng(1,profile.random->multiplier(),1);
    assert(!roll_starting_age(no_age,"human","fighter",no_age_rng) && no_age_rng.state() == 1);
    rejects([&] { (void)roll_starting_age(rules,"dwarf","magic-user",no_age_rng); });
    assert(no_age_rng.state() == 1);
    auto invalid_age = rules;
    invalid_age.races[0].starting_ages["fighter"].count = -1;
    rejects([&] { (void)roll_starting_age(invalid_age,"dwarf","fighter",no_age_rng); });
    assert(no_age_rng.state() == 1);
    invalid_age.races[0].starting_ages["fighter"] = {40,0,2};
    assert(roll_starting_age(invalid_age,"dwarf","fighter",no_age_rng) == 40 && no_age_rng.state() == 1);
    invalid_age.races[0].starting_ages["fighter"].base = 0;
    rejects([&] { (void)roll_starting_age(invalid_age,"dwarf","fighter",no_age_rng); });
    auto aged = session_for(profile);
    Character aged_pc{"Aged","human","male","fighter","lawful-good",std::vector<AbilityValue>(6,{15,0})};
    aged_pc.age = roll_starting_age(rules,"human","fighter",*aged.state.random);
    add_character(aged.party,rules,finish_character(rules,aged_pc,*aged.state.random));
    const auto age_save = serialize_exploration(aged,"synthetic-age");
    const auto next_age = roll_starting_age(rules,"human","fighter",*aged.state.random);
    auto age_restored = session_for(profile);
    restore_exploration(age_restored,age_save,"synthetic-age");
    assert(age_restored.party[0].age == aged_pc.age);
    assert(roll_starting_age(rules,"human","fighter",*age_restored.state.random) == next_age);
    assert(age_restored.state.random->state() == aged.state.random->state());
    assert(rules.bounds == CreationBounds::clamp && rules.percentile_roll_maximum == 100);
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
    // Original racial-bound blocks: endpoint facts for all six races and both genders.
    struct BoundsObservation {
        const char* race; std::array<int,6> minimum, male_maximum, female_maximum;
    };
    const BoundsObservation bounds[]{
        {"dwarf",{8,3,3,3,12,3},{18,18,18,17,19,16},{17,18,18,17,19,16}},
        {"elf",{3,8,3,7,6,8},{18,18,18,19,18,18},{16,18,18,19,18,18}},
        {"gnome",{6,7,3,3,8,3},{18,18,18,18,18,18},{15,18,18,18,18,18}},
        {"half-elf",{3,4,3,6,6,3},{18,18,18,18,18,18},{17,18,18,18,18,18}},
        {"halfling",{6,6,3,8,10,3},{17,18,17,18,19,18},{14,18,17,18,19,18}},
        {"human",{3,3,3,3,3,3},{18,18,18,18,18,18},{18,18,18,18,18,18}}
    };
    for (const auto& observed : bounds) for (const auto gender : {"male","female"}) {
        for (const auto adjustment : {-100,100}) {
            auto capped = rules;
            capped.dice_count = 1; capped.dice_sides = 2; capped.drop_lowest = 0; capped.keep_best_of = 1;
            for (auto& race : capped.races) race.adjustments.assign(6,adjustment);
            for (auto& choice : capped.classes) choice.minimum.assign(6,1); // Isolate racial bounds.
            HighWordLcg32 rng(0x12345678,profile.random->multiplier(),1);
            const auto rolled = roll_abilities(capped,observed.race,gender,rng);
            const auto& expected = adjustment < 0 ? observed.minimum :
                (std::string_view(gender) == "male" ? observed.male_maximum : observed.female_maximum);
            for (std::size_t i = 0; i < 6; ++i) assert(rolled[i].value == expected[i]);
            assert(rng.state() == 0xe28f6d7a); // Six draws, no cap-triggered rerolls.
        }
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
    struct PercentileObservation { std::uint32_t seed; int rolled; std::uint32_t after; };
    const PercentileObservation percentiles[]{
        {0,1,1},{1,57,134775814},{0xffffffff,80,4160191484u},
        {0x12345678,60,3411775577u},{0x2e4d0932,83,2043606523},{23,100,3099843700u}
    };
    const int male_caps[]{99,75,50,90,0,100};
    for (std::size_t r = 0; r < 6; ++r) for (const auto gender : {"male","female"}) {
        const bool male = std::string_view(gender) == "male";
        const auto strength = std::min(18,(male ? bounds[r].male_maximum : bounds[r].female_maximum)[0]);
        const auto cap = male ? male_caps[r] : (r == 5 ? 50 : 0);
        for (const auto& observed : percentiles) {
            Character draft{"Cap",bounds[r].race,gender,"fighter","lawful-good",
                {{strength,0},{10,0},{10,0},{10,0},{13,0},{10,0}}};
            HighWordLcg32 rng(observed.seed,profile.random->multiplier(),1);
            const auto finished = finish_character(rules,draft,rng);
            assert(finished.abilities[0].percentile == (cap ? std::min(observed.rolled,cap) : 0));
            assert(rng.state() == (cap ? observed.after : observed.seed));
            same(finished.abilities,read_party(serialize_party({finished},rules),rules)[0].abilities);
        }
    }
    // Original class-floor facts. Choice preview and completion agree, with no draws.
    struct FloorObservation { const char* character_class; std::array<int,6> floor; };
    for (const auto& observed : {
        FloorObservation{"cleric",{6,6,9,0,0,0}}, FloorObservation{"fighter",{9,0,6,6,7,0}},
        FloorObservation{"magic-user",{0,9,6,6,0,0}}, FloorObservation{"thief",{6,6,0,9,0,0}}}) {
        for (const auto value : {3,6,8,9,12,17}) {
            Character draft{"Floor","human","male",observed.character_class,
                std::string_view(observed.character_class) == "thief" ? "neutral-good" : "lawful-good",
                std::vector<AbilityValue>(6,{value,0})};
            const auto before = draft.abilities;
            assert(creation_classes(rules,draft).size() == 4);
            if (value == 3) assert(eligible_classes(rules,draft).empty());
            HighWordLcg32 rng(0x12345678,profile.random->multiplier(),1);
            const auto finished = finish_character(rules,draft,rng);
            for (std::size_t i = 0; i < 6; ++i)
                assert(finished.abilities[i].value == std::max(value,observed.floor[i]));
            assert(rng.state() == 0x12345678);
            same(before,draft.abilities); // Preview and completion don't mutate the original draft.
            same(finished.abilities,read_party(serialize_party({finished},rules),rules)[0].abilities);
        }
    }
    auto incompatible = rules;
    incompatible.classes[0].creation_minimum[0] = 19; // Human maximum is 18.
    Character low{"Low","human","male","cleric","lawful-good",std::vector<AbilityValue>(6,{3,0})};
    assert(creation_classes(incompatible,low).size() == 3);
    HighWordLcg32 untouched(1,profile.random->multiplier(),1);
    rejects([&] { (void)finish_character(incompatible,low,untouched); });
    assert(untouched.state() == 1);
    Character old_fighter{"Legacy","human","male","fighter","lawful-good",
        {{9,0},{3,0},{3,0},{3,0},{3,0},{3,0}}};
    const auto old_party = read_party(serialize_party({old_fighter},rules),rules);
    same(old_fighter.abilities,old_party[0].abilities); // Creation floors never rewrite existing saves.
    auto legacy_creation = rules;
    for (auto& choice : legacy_creation.classes) choice.creation_minimum.clear();
    assert(creation_classes(legacy_creation,low).empty());
    rejects([&] { (void)finish_character(legacy_creation,low,untouched); });
    assert(untouched.state() == 1);

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

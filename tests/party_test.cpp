// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/party.hpp"
#include "goldbox/exploration.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>

using namespace goldbox;
namespace {
void rejects(const std::function<void()>& action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
std::string changed(std::string value, const std::string& from, const std::string& to) {
    const auto at = value.find(from);
    if (at == std::string::npos) std::cerr << "Missing fixture fragment: " << from << '\n';
    assert(at != std::string::npos); value.replace(at,from.size(),to); return value;
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto rules = read_creation_rules(argv[1]);
    assert(rules.party_limit == 6 && rules.name_limit == 15 && rules.abilities.size() == 6);
    assert(rules.races.size() == 6 && rules.classes.size() == 11);
    std::mt19937 random(1234), same(1234);
    const auto a = roll_abilities(rules,"human","male",random);
    const auto b = roll_abilities(rules,"human","male",same);
    for (std::size_t i = 0; i < a.size(); ++i) assert(a[i].value == b[i].value);
    for (const auto& race : rules.races) for (const auto& gender : rules.genders) {
        Character c; c.name = "Hero"; c.race = race.id; c.gender = gender.id;
        for (int attempt = 0; attempt < 20; ++attempt) {
            c.abilities = roll_abilities(rules,race.id,gender.id,random);
            const auto choices = creation_classes(rules,c); assert(!choices.empty());
            const auto& clazz = rules.classes[choices.front()];
            c.character_class = clazz.id; c.alignment = clazz.alignments.front();
            validate_character(rules,finish_character(rules,c,random));
        }
    }
    Character hero{"Ada","human","female","cleric","lawful-good",std::vector<AbilityValue>(6,{15,0})};
    validate_character(rules,hero);
    Character fighter = hero; fighter.character_class = "fighter"; fighter.abilities[0].value = 18;
    const auto exceptional = finish_character(rules,fighter,random);
    assert(exceptional.status == "okay");
    assert(rules.starting_level == 1 && exceptional.levels ==
        (std::map<std::string,std::uint16_t>{{"fighter",1}}));
    for (const auto& choice : rules.classes) {
        const auto race = std::find_if(rules.races.begin(),rules.races.end(),[&](const auto& r) {
            return std::find(r.classes.begin(),r.classes.end(),choice.id) != r.classes.end();
        });
        assert(race != rules.races.end());
        auto draft = hero; draft.race = race->id; draft.gender = "male";
        draft.character_class = choice.id; draft.alignment = choice.alignments.front();
        const auto rolled = roll_character(rules,draft,random);
        std::map<std::string,std::uint16_t> expected;
        if (choice.components.empty()) expected.emplace(choice.id,1);
        else for (const auto& name : choice.components) expected.emplace(name,1);
        assert(rolled.levels == expected);
        auto completion_random = random;
        assert(finish_character(rules,rolled,completion_random).levels == expected);
        validate_character(rules,rolled);
        const auto saved = serialize_party({rolled},rules);
        assert(read_party(saved,rules)[0].levels == expected);
        auto bad = rolled; bad.levels.begin()->second = 0;
        rejects([&] { validate_character(rules,bad); });
        bad = rolled; bad.levels.emplace("unknown",1);
        rejects([&] { validate_character(rules,bad); });
        if (expected.size() > 1) {
            bad = rolled; bad.levels.erase(bad.levels.begin());
            rejects([&] { validate_character(rules,bad); });
        }
        rejects([&] { (void)read_party(changed(saved,"\"levels\":{","\"levels\":{\"unknown\":1,"),rules); });
    }
    auto custom_rules = rules; custom_rules.starting_level = 3;
    assert(finish_character(custom_rules,hero,random).levels.at("cleric") == 3);
    custom_rules.starting_level.reset();
    assert(finish_character(custom_rules,hero,random).levels.empty());
    assert(exceptional.abilities[0].percentile >= 1 && exceptional.abilities[0].percentile <= 50);
    rejects([&] { validate_character(rules,fighter); }); // Missing exceptional score is not silently accepted.
    for (int failure = 0; failure < 6; ++failure) {
        auto invalid = hero;
        if (failure == 0) invalid.race = "unknown";
        if (failure == 1) invalid.gender = "unknown";
        if (failure == 2) invalid.name = std::string(16,'x');
        if (failure == 3) invalid.abilities[0].value = 19;
        if (failure == 4) invalid.character_class = "thief"; // Disallowed alignment.
        if (failure == 5) invalid.race = "dwarf"; // No dwarf cleric in this profile.
        rejects([&] { validate_character(rules,invalid); });
    }
    Party party; add_character(party,rules,hero);
    auto duplicate = hero; duplicate.name = "ada";
    rejects([&] { add_character(party,rules,duplicate); });
    for (int i = 1; i < 6; ++i) { auto c = hero; c.name = "Hero"+std::to_string(i); add_character(party,rules,c); }
    const auto json = serialize_party(party,rules);
    assert(!read_party(json,rules)[0].age); // Legacy ages remain unknown.
    assert(!read_party(json,rules)[0].portrait);
    assert(read_party(json,rules)[0].levels.empty());
    auto painted = hero; painted.portrait = CharacterPortrait{0,1};
    const auto portrait_json = serialize_party({painted},rules);
    assert(read_party(portrait_json,rules)[0].portrait->head == 0 && read_party(portrait_json,rules)[0].portrait->body == 1);
    for (const auto& value : {"-1","256","true","2"})
        rejects([&] { (void)read_party(changed(portrait_json,"\"head\":0",std::string("\"head\":")+value),rules); });
    rejects([&] { (void)read_party(changed(portrait_json,"\"body\":1","\"body\":0"),rules); });
    auto no_portrait_rules = rules; no_portrait_rules.portrait.reset();
    rejects([&] { (void)read_party(portrait_json,no_portrait_rules); });
    assert(!read_party(json,rules)[0].hit_points && read_party(json,rules)[0].money.empty());
    assert(!read_party(json,rules)[0].spellbook);
    auto caster = hero; caster.spellbook = Spellbook{{"mod.custom-spell","divine.bless"},3};
    const auto spells_json = serialize_party({caster},rules);
    const auto saved_caster = read_party(spells_json,rules)[0];
    assert(saved_caster.spellbook->known == caster.spellbook->known && saved_caster.spellbook->first_level_slots == 3);
    for (const auto& value : {"-1","65","true","null"})
        rejects([&] { (void)read_party(changed(spells_json,"\"firstLevelSlots\":3",
            std::string("\"firstLevelSlots\":")+value),rules); });
    rejects([&] { (void)read_party(changed(spells_json,"mod.custom-spell","divine.bless"),rules); });
    caster.spellbook->known.push_back("divine.bless");
    rejects([&] { (void)serialize_party({caster},rules); });
    caster.spellbook.reset();
    caster.spellbooks = {{"divine",{{"divine.bless"},3}},{"arcane",{{"arcane.sleep"},1}}};
    const auto grouped_json = serialize_party({caster},rules);
    auto grouped = read_party(grouped_json,rules)[0];
    assert(grouped.spellbooks.at("divine").first_level_slots == 3 && grouped.spellbooks.at("arcane").first_level_slots == 1);
    rejects([&] { (void)read_party(changed(grouped_json,"\"arcane\":{","\"Invalid ID\":{"),rules); });
    caster.spellbook = Spellbook{{},0};
    rejects([&] { (void)serialize_party({caster},rules); });
    caster.spellbook.reset(); caster.spellbooks.clear();
    Spellbook large_book;
    for (int n = 0; n < 64; ++n) large_book.known.push_back(std::string(60,'a')+std::to_string(n));
    for (int n = 0; n < 8; ++n) caster.spellbooks.emplace("book"+std::to_string(n),large_book);
    auto another_caster = caster; another_caster.name = "Another";
    rejects([&] { (void)serialize_party({caster,another_caster},rules); }); // Never write a party that its reader rejects by size.
    auto wounded = hero; wounded.hit_points = HitPoints{5,0,6}; wounded.money["gold"] = 0;
    const auto vitals_json = serialize_party({wounded},rules);
    auto restored = read_party(vitals_json,rules)[0];
    assert(restored.hit_points->base == 5 && restored.hit_points->current == 0 &&
        restored.hit_points->maximum == 6 && restored.money.at("gold") == 0);
    for (const auto& value : {"-1","7","true","null"})
        rejects([&] { (void)read_party(changed(vitals_json,"\"current\":0",std::string("\"current\":")+value),rules); });
    for (const auto& value : {"-1","65536","1.5","true"})
        rejects([&] { (void)read_party(changed(vitals_json,"\"gold\":0",std::string("\"gold\":")+value),rules); });
    wounded.hit_points->current = 7;
    rejects([&] { (void)serialize_party({wounded},rules); });
    auto with_age = party; with_age[0].age = 22;
    const auto age_json = serialize_party(with_age,rules);
    assert(read_party(age_json,rules)[0].age == 22);
    for (const auto& value : {"0","65536","true","null"})
        rejects([&] { (void)read_party(changed(age_json,"\"age\":22",std::string("\"age\":")+value),rules); });
    assert(!read_party(json,rules)[0].status); // Legacy party records remain explicitly unknown.
    auto conditioned = party; conditioned[0].status = "unconscious";
    assert(read_party(serialize_party(conditioned,rules),rules)[0].status == "unconscious");
    conditioned[0].status = "Invalid Status";
    rejects([&] { (void)serialize_party(conditioned,rules); });
    rejects([&] { (void)read_party(changed(serialize_party(party,rules),"\"name\":\"Ada\"",
        "\"name\":\"Ada\",\"status\":null"),rules); });
    assert(serialize_party(read_party(json,rules),rules) == json);
    rejects([&] { add_character(party,rules,hero); }); assert(serialize_party(party,rules) == json);
    rejects([&] { (void)read_party(changed(json,rules.id,"other-rules"),rules); });
    rejects([&] { (void)read_party(changed(json,"\"value\":15","\"value\":99"),rules); });
    rejects([&] { (void)read_party(changed(json,"\"class\":\"cleric\"","\"class\":\"missing\""),rules); });
    rejects([&] { (void)read_party(changed(json,"Hero1","Ada"),rules); });
    ExplorationSession session; session.character_rules = rules; session.party = party;
    session.state.memory[123] = 42;
    const auto save = serialize_exploration(session,"party-test");
    ExplorationSession loaded; loaded.character_rules = rules;
    restore_exploration(loaded,save,"party-test");
    assert(serialize_exploration(loaded,"party-test") == save);
    rejects([&] { restore_exploration(loaded,changed(save,rules.id,"bad-rules"),"party-test"); });
    assert(serialize_exploration(loaded,"party-test") == save);
    ExplorationSession no_rules;
    rejects([&] { restore_exploration(no_rules,save,"party-test"); });
    const auto legacy = serialize_exploration(no_rules,"party-test");
    restore_exploration(loaded,legacy,"party-test"); assert(loaded.party.empty() && loaded.character_rules);
    auto custom = rules; custom.id = "other-family"; custom.abilities.resize(2); custom.races.resize(1);
    custom.genders.resize(1); custom.classes.resize(1); custom.percentile_ability.reset();
    custom.classes[0].minimum = {1,1}; custom.classes[0].percentile = false;
    custom.classes[0].creation_minimum.clear();
    custom.races[0].minimum = {1,1}; custom.races[0].maximum = {{"male",{18,18}}};
    custom.races[0].adjustments.clear();
    custom.races[0].percentile_maximum = {{"male",0}}; custom.races[0].classes = {custom.classes[0].id};
    Character other{"Other",custom.races[0].id,"male",custom.classes[0].id,custom.classes[0].alignments[0],{}};
    other.abilities = roll_abilities(custom,other.race,other.gender,random);
    validate_character(custom,finish_character(custom,other,random)); assert(other.abilities.size() == 2);
    auto impossible = rules; impossible.dice_count = 1; impossible.dice_sides = 2; impossible.drop_lowest = 0;
    for (auto& choice : impossible.classes) choice.creation_minimum.clear();
    rejects([&] { (void)roll_abilities(impossible,"human","male",random); });
    // Alternate ability layout: signed adjustments apply once and do not consume extra draws.
    custom.races[0].minimum = {1,1}; custom.races[0].maximum = {{"male",{100,100}}};
    auto adjusted = custom;
    adjusted.races[0].adjustments = {-1,2};
    std::mt19937 plain_rng(99), adjusted_rng(99);
    const auto plain = roll_abilities(custom,other.race,other.gender,plain_rng);
    const auto modified = roll_abilities(adjusted,other.race,other.gender,adjusted_rng);
    assert(modified[0].value == plain[0].value-1 && modified[1].value == plain[1].value+2);
    assert(plain_rng == adjusted_rng);
    adjusted.races[0].adjustments = {1};
    rejects([&] { (void)roll_abilities(adjusted,other.race,other.gender,adjusted_rng); });
    assert(plain_rng == adjusted_rng);
    std::ifstream source(argv[1]); std::string original{std::istreambuf_iterator<char>(source),{}};
    std::erase(original,'\r');
    const auto invalid_file = std::filesystem::temp_directory_path()/"goldbox-invalid-creation-rules.json";
    for (const auto& value : {changed(original,"\"partyLimit\": 6","\"partyLimit\": 0"),
        changed(original,"\"dropLowest\": 1","\"dropLowest\": 4"),
        changed(original,"\"id\": \"strength\"","\"id\": \"intelligence\""),
        changed(original,"\"id\": \"fighter\"","\"id\": \"missing\""),
        changed(original,"\"adjustments\": [0, 0, 0, 0, 1, -1]","\"adjustments\": [1]"),
        changed(original,"\"adjustments\": [0, 0, 0, 0, 1, -1]","\"adjustments\": [0, 0, 0, 0, true, -1]"),
        changed(original,"\"adjustments\": [0, 0, 0, 0, 1, -1]","\"adjustments\": [0, 0, 0, 0, 101, -1]"),
        changed(original,"\"bounds\": \"clamp\"","\"bounds\": \"unknown\""),
        changed(original,"\"bounds\": \"clamp\"","\"bounds\": true"),
        changed(original,"\"rollMaximum\": 100","\"rollMaximum\": 0"),
        changed(original,"\"rollMaximum\": 100","\"rollMaximum\": 101"),
        changed(original,"\"rollMaximum\": 100","\"rollMaximum\": false"),
        changed(original,"\"creationMinimum\": [6, 6, 9, 0, 0, 0]","\"creationMinimum\": [6]"),
        changed(original,"\"creationMinimum\": [6, 6, 9, 0, 0, 0]","\"creationMinimum\": [6, 6, -1, 0, 0, 0]"),
        changed(original,"\"creationMinimum\": [6, 6, 9, 0, 0, 0]","\"creationMinimum\": [6, 6, true, 0, 0, 0]"),
        changed(original,"\"creationMinimum\": [6, 6, 9, 0, 0, 0]","\"creationMinimum\": [6, 6, 1001, 0, 0, 0]"),
        changed(original,"\"count\": 5","\"count\": 0"),
        changed(original,"\"base\": 40","\"base\": 65535"),
        changed(original,"\"sides\": 4","\"sides\": true"),
        changed(original,"\"fighter\": {\"base\": 40","\"unknown\": {\"base\": 40"),
        changed(original,"\"initialRandomBounds\": [256]","\"initialRandomBounds\": [65536]"),
        changed(original,"\"values\": [0, 0, -1, 0, 0, 0]","\"values\": [0]"),
        changed(original,"\"values\": [0, 0, -1, 0, 0, 0]","\"values\": [0, 0, -101, 0, 0, 0]"),
        changed(original,"\"positiveOnly\": [\"strength\"]","\"positiveOnly\": [\"unknown\"]"),
        changed(original,"\"wrapByte\": true","\"wrapByte\": 1"),
        changed(original,"\"abilityMinimum\": 3","\"abilityMinimum\": true"),
        changed(original,"\"minimum\": 5, \"ability\"","\"minimum\": 9, \"ability\""),
        changed(original,"\"currency\": \"gold\"","\"currency\": \"Invalid ID\""),
        changed(original,"\"multiplier\": 10","\"multiplier\": 65535"),
        changed(original,"\"firstLevelSlots\": 1","\"firstLevelSlots\": 65"),
        changed(original,"\"divine.curse\"","\"divine.bless\""),
        changed(original,"\"ability\": \"wisdom\"","\"ability\": \"missing\""),
        changed(original,"\"firstLevelSlots\": 0","\"firstLevelSlots\": 0, \"modifiers\": [0]"),
        changed(original,"\"modifiers\": [0, 0","\"modifiers\": [65, 0"),
        changed(original,"\"components\": [\n        \"cleric\",\n        \"fighter\"",
            "\"components\": [\n        \"cleric-fighter\",\n        \"fighter\""),
        changed(original,"\"components\": [\n        \"cleric\",\n        \"fighter\"",
            "\"components\": [\n        \"missing\",\n        \"fighter\""),
        changed(original,"\"components\": [\n        \"cleric\",\n        \"fighter\"",
            "\"components\": [\n        \"fighter\",\n        \"fighter\""),
        changed(original,"\"startingSpellbooks\": {",
            "\"startingSpells\": {\"known\": [], \"firstLevelSlots\": 0}, \"startingSpellbooks\": {"),
        changed(original,"\"heads\": [0, 8","\"heads\": [0, 0"),
        changed(original,"\"heads\": [0, 8","\"heads\": [0, 256"),
        changed(original,"\"heads\": [","\"heads\": [false,"),
        changed(original,"\"bodies\": [1, 2","\"bodies\": [1, 1")}) {
        { std::ofstream output(invalid_file); output << value; }
        rejects([&] { (void)read_creation_rules(invalid_file); });
    }
    auto legacy_source = original;
    std::erase(legacy_source,'\r');
    const auto legacy_rules_json = changed(changed(legacy_source,",\n    \"bounds\": \"clamp\"",""),
        ",\n    \"rollMaximum\": 100","");
    { std::ofstream output(invalid_file); output << legacy_rules_json; }
    const auto legacy_rules = read_creation_rules(invalid_file);
    assert(legacy_rules.bounds == CreationBounds::reject && !legacy_rules.percentile_roll_maximum);
    HighWordLcg32 legacy_rng(0x12345678,134775813,1);
    assert(finish_character(legacy_rules,fighter,legacy_rng).abilities[0].percentile == 10);
    std::filesystem::remove(invalid_file);
    std::cout << "Party creation, eligibility, bounded dice, save validation and alternate rules passed\n";
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "goldbox/exploration.hpp"
#include "goldbox/formats/monster.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <functional>

using namespace goldbox;
using namespace goldbox::formats;
namespace {
struct Host : EclHost {
    std::vector<std::uint8_t> programs;
    bool reject_program{};
    void program_changed(std::uint8_t id) override {
        if (reject_program) throw std::runtime_error("Missing graphics binding");
        programs.push_back(id);
    }
    std::string output;
    std::vector<std::uint16_t> sounds;
    unsigned clears{};
    void clear_images() override { ++clears; }
    void sound(std::uint16_t id) override { sounds.push_back(id); }
    void text(std::string_view value, bool) override { output += value; }
};
void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
std::vector<std::uint8_t> lifecycle(std::vector<std::uint8_t> move, std::vector<std::uint8_t> search,
    std::vector<std::uint8_t> init = {0}) {
    std::vector<std::uint8_t> code(20);
    const std::vector<std::uint8_t> bodies[]{move,search,{0},{0},init};
    for (std::size_t i = 0; i < 5; ++i) {
        const auto address = 0x9900+code.size();
        code[i*4] = 1; code[i*4+1] = 1;
        code[i*4+2] = address & 255; code[i*4+3] = address >> 8;
        code.insert(code.end(),bodies[i].begin(),bodies[i].end());
    }
    return code;
}
ExplorationSession native_session(std::vector<std::uint8_t> code) {
    ExplorationSession session;
    session.dialect = {"ssi-five-entry-v1",0x9900,0x7103,true,
        std::array<std::uint16_t,3>{0x7100,0x7101,0x7102}};
    session.movement_addresses = std::array<std::uint16_t,4>{0x7200,0x7201,0x7202,0x7203};
    session.opcodes = minimal_vm_opcodes(); session.ecl_record = std::move(code);
    return session;
}
void monster_setup() {
    const auto load = [](std::uint8_t count) { return std::vector<std::uint8_t>{0x0b,0,7,0,count,0,3}; };
    const auto fixture = [&](std::vector<std::uint8_t> body) {
        auto session = native_session(lifecycle(std::move(body),{0}));
        session.monster_dialect = EclMonsterDialect::setup_v1;
        session.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,
            EclPartyDialect::disabled,EclPartyCheckDialect::disabled,EclSurpriseDialect::disabled,
            EclEncounterDialect::disabled,EclMonsterDialect::setup_v1);
        session.current_program_record = 0;
        session.programs = ProgramResources{[code=session.ecl_record](std::uint8_t id) {
            return ProgramResource{id,code}; },"synthetic-monster-program"};
        session.monster_resources = MonsterResources{[](std::uint8_t program,std::uint8_t id) {
            if (program != 0 || id != 7) throw std::runtime_error("Missing synthetic monster");
            const std::vector<std::uint8_t> character(285),items(126),effects(18);
            validate_ssi_monster_resources(character,items,effects);
        },"synthetic-monster-resources"};
        session.initial_random = HighWordLcg32(0,0x08088405,1);
        session.state.random = session.initial_random;
        return session;
    };
    Host host;
    for (const auto requested : {0,1,2,63,255}) {
        auto body = std::vector<std::uint8_t>{3,0,1,0,2,0x1c};
        const auto request = load(requested); body.insert(body.end(),request.begin(),request.end()); body.push_back(0);
        auto session = fixture(body);
        const auto run = session.trigger(EclEventKind::move,host);
        assert(run.exited && session.monster_setup && session.monster_setup->groups.size() == 1);
        const auto& group = session.monster_setup->groups.front();
        assert(group.count == std::min(std::max(requested,1),63) && group.group == 8 && group.selector == 3);
        assert(group.program == 0 && group.resource == 7 && group.template_seed == 0);
        assert(session.monster_setup->next_group == 9 && session.state.random->state() == 1172187917);
        assert(session.state.comparison == -1 && host.clears == 0); // Native CLEAR_MON does not clear the sprite.
        const auto saved = serialize_exploration(session,"monster-fixture");
        auto restored = fixture(body);
        restore_exploration(restored,saved,"monster-fixture");
        assert(serialize_exploration(restored,"monster-fixture") == saved);
        for (const auto& [before,after] : std::vector<std::pair<std::string,std::string>>{
            {"\"nextGroup\":9","\"nextGroup\":255"},
            {"\"sourceIdentity\":\"synthetic-monster-resources\"","\"sourceIdentity\":\"wrong\""},
            {"\"count\":"+std::to_string(group.count),"\"count\":0"},
            {"\"templateSeed\":0","\"templateSeed\":4294967296"}}) {
            auto invalid = saved;
            const auto at = invalid.find(before); assert(at != std::string::npos);
            invalid.replace(at,before.size(),after);
            rejects([&] { restore_exploration(restored,invalid,"monster-fixture"); });
            assert(serialize_exploration(restored,"monster-fixture") == saved);
        }
        auto legacy = session; legacy.monster_setup.reset();
        restore_exploration(restored,serialize_exploration(legacy,"monster-fixture"),"monster-fixture");
        assert(!restored.monster_setup);
    }
    auto body = load(1); body.push_back(0);
    auto multiple_body = std::vector<std::uint8_t>{0x1c};
    for (const auto count : {62,2,1}) {
        const auto request = load(count); multiple_body.insert(multiple_body.end(),request.begin(),request.end());
    }
    multiple_body.push_back(0);
    auto multiple = fixture(multiple_body);
    assert(multiple.trigger(EclEventKind::move,host).exited);
    assert(multiple.monster_setup->groups.size() == 2 && multiple.monster_setup->next_group == 10);
    assert(multiple.monster_setup->groups[0].count == 62 && multiple.monster_setup->groups[1].count == 1 &&
        multiple.monster_setup->groups[1].group == 9 && multiple.monster_setup->groups[1].template_seed == 1172187917);
    assert(multiple.state.random->state() == 1828254910); // Ten draws; the third request hits the cap.
    auto invalid_resource = fixture(body); invalid_resource.monster_setup.emplace();
    invalid_resource.monster_resources->validate = [](std::uint8_t,std::uint8_t) {
        throw std::runtime_error("Malformed synthetic template");
    };
    assert(!invalid_resource.trigger(EclEventKind::move,host).exited &&
        invalid_resource.monster_setup->groups.empty() && invalid_resource.state.random->state() == 0);
    auto unknown = fixture(body);
    assert(unknown.trigger(EclEventKind::move,host).diagnostic == "Monster setup state unavailable; CLEAR_MON required");
    auto capped = fixture(body);
    capped.monster_setup = MonsterSetup{{PreparedMonsterGroup{0,7,63,8,3,0}},9};
    capped.monster_resources.reset(); capped.state.random.reset();
    assert(capped.trigger(EclEventKind::move,host).exited && capped.monster_setup->groups.size() == 1);
    auto missing = fixture(body); missing.monster_setup.emplace(); missing.monster_resources.reset();
    assert(missing.trigger(EclEventKind::move,host).diagnostic == "Monster resource binding unavailable");
    assert(missing.state.random->state() == 0 && missing.monster_setup->groups.empty());
    auto clear = fixture({0x1c,0}); clear.monster_setup = capped.monster_setup;
    assert(clear.trigger(EclEventKind::move,host).exited && clear.monster_setup->groups.empty() &&
        clear.monster_setup->next_group == 8 && clear.state.random->state() == 0);
    for (const auto sizes : {std::array<std::size_t,3>{284,0,0},{285,1,0},{285,0,1}})
        rejects([&] { validate_ssi_monster_resources(std::vector<std::uint8_t>(sizes[0]),
            std::vector<std::uint8_t>(sizes[1]),std::vector<std::uint8_t>(sizes[2])); });
}
void attack_primitives() {
    const auto append = [](std::uint64_t& hash, int byte) {
        hash ^= static_cast<std::uint8_t>(byte); hash *= 1099511628211ull;
    };
    HighWordLcg32 fixture(104,0x08088405,1);
    std::uint64_t large_hash = 14695981039346656037ull;
    for (unsigned i = 0; i < 384; ++i) {
        const auto size = i < 256 ? i : fixture.draw(256);
        const auto present = fixture.draw(2); (void)fixture.draw(256); // Native fixture's item type.
        SsiWeaponStats prepared{41,{static_cast<std::uint8_t>(fixture.draw(4)),
            static_cast<std::uint8_t>(fixture.draw(12)+1),ssi_signed_byte(fixture.draw(256))}};
        SsiBaseItem definition; definition.small_medium.bonus = ssi_signed_byte(fixture.draw(256));
        definition.large = {static_cast<std::uint8_t>(fixture.draw(4)),
            static_cast<std::uint8_t>(fixture.draw(12)+1),ssi_signed_byte(fixture.draw(256))};
        const auto result = present ? ssi_target_weapon_stats(prepared,definition,size) :
            SsiTargetWeaponStats{prepared.thac0_code,prepared.small_medium};
        assert(result.thac0_code == prepared.thac0_code);
        for (const auto value : {int(result.damage.count),int(result.damage.sides),result.damage.bonus}) append(large_hash,value);
    }
    assert(large_hash == 1067471470663856932ull);
    std::uint64_t dice_hash = 14695981039346656037ull;
    for (const auto seed : std::array<std::uint32_t,6>{0,1,10,19,0xffffffff,0x12345678})
        for (const auto count : {0,1,2,3,127,128,255}) for (const auto sides : {0,1,2,6,20,127,128,255}) {
            HighWordLcg32 random(seed,0x08088405,1);
            append(dice_hash,roll_byte_dice(random,count,sides));
            for (unsigned byte = 0; byte < 4; ++byte) append(dice_hash,random.state() >> (byte*8));
        }
    assert(dice_hash == 7182648793210212076ull);
    const std::array<std::uint32_t,20> seeds{0,11,5,22,16,10,4,15,9,3,20,14,8,2,13,7,1,18,12,6};
    const std::array<std::uint8_t,8> scores{0,1,39,60,100,127,128,255};
    fixture = HighWordLcg32(6816,0x08088405,1);
    std::uint64_t hit_hash = 14695981039346656037ull;
    for (const auto mode : {0,1}) for (unsigned i = 0; i < 320; ++i) {
        const auto natural = i%20+1;
        const auto accuracy = scores[fixture.draw(scores.size())], defense = scores[fixture.draw(scores.size())];
        const auto modifier = ssi_signed_byte(fixture.draw(256));
        const auto effective = i%2 ? fixture.draw(256) : (natural == 20 ? 100 : natural);
        HighWordLcg32 random(seeds[natural-1],0x08088405,1);
        auto roll = make_ssi_attack_roll(roll_byte_dice(random,1,20));
        assert(roll.natural == natural);
        if (roll.natural != 1) roll.effective = effective; // Explicit synthetic effect boundary.
        append(hit_hash,mode == 0 ? ssi_defender_only_attack_hits(roll,accuracy,defense) :
            ssi_attack_hits(roll,accuracy,defense,modifier));
        append(hit_hash,roll.effective);
    }
    assert(hit_hash == 15120811759041066323ull);
    assert(ssi_attack_hits(make_ssi_attack_roll(10),50,60,0));
    assert(!ssi_defender_only_attack_hits(make_ssi_attack_roll(10),50,60));
    assert(!ssi_attack_hits(make_ssi_attack_roll(20),0,255,0));
    assert(!ssi_attack_hits({1,100},255,0,127) && !ssi_attack_hits({10,128},255,0,127));
    for (const auto natural : {0,21,255}) rejects([&] { make_ssi_attack_roll(natural); });
    for (const auto modifier : {-129,128}) rejects([&] { ssi_attack_hits({10,10},50,60,modifier); });
    rejects([&] { ssi_defender_only_attack_hits({0,0},0,0); });
    SsiBaseItem invalid; invalid.large.bonus = 128;
    rejects([&] { ssi_target_weapon_stats({0,{1,6,0}},invalid,2); });
    assert(!prepare_ssi_weapon_stats(SsiMonsterTemplate{},std::span<const SsiBaseItem>{},2));
}
void weapon_inputs() {
    const auto append = [](std::uint64_t& hash, int byte) {
        hash ^= static_cast<std::uint8_t>(byte); hash *= 1099511628211ull;
    };
    std::uint64_t ability_hash = 14695981039346656037ull;
    for (unsigned dexterity = 0; dexterity <= 255; ++dexterity)
        append(ability_hash,ssi_dexterity_accuracy(dexterity));
    for (unsigned strength = 0; strength <= 25; ++strength) {
        std::vector<unsigned> percentiles{0,100,255};
        if (strength == 18) { percentiles.clear(); for (unsigned i = 0; i <= 100; ++i) percentiles.push_back(i); }
        for (const auto percentile : percentiles) for (const auto flag : {0,1,255})
            for (const auto bonus : ssi_strength_weapon_bonuses(strength,percentile,flag != 0)) append(ability_hash,bonus);
    }
    assert(ability_hash == 11913737308611779697ull); // 1,312 original helper returns.
    for (unsigned strength = 26; strength <= 255; ++strength) {
        rejects([&] { ssi_strength_weapon_bonuses(strength,0,true); });
        assert((ssi_strength_weapon_bonuses(strength,255,false) == std::array<int,2>{0,0}));
    }
    for (unsigned percentile = 101; percentile <= 255; ++percentile)
        rejects([&] { ssi_strength_weapon_bonuses(18,percentile,true); });
    HighWordLcg32 fixture_random(6812,0x08088405,1);
    const std::array<std::uint8_t,8> flags{0,1,2,4,128,129,135,255};
    const std::array<std::uint8_t,5> types{0,1,28,73,255};
    const std::array<std::uint8_t,4> ready{0,1,2,255};
    std::uint64_t equipment_hash = 14695981039346656037ull, stats_hash = equipment_hash;
    for (unsigned i = 0; i < 128; ++i) {
        std::vector<SsiBaseItem> definitions(256);
        for (auto& definition : definitions) {
            definition.slot = fixture_random.draw(13); definition.hands = fixture_random.draw(3);
            definition.small_medium.count = fixture_random.draw(4);
            definition.small_medium.sides = fixture_random.draw(12)+1;
            definition.small_medium.bonus = ssi_signed_byte(fixture_random.draw(256));
            definition.weapon_flags = flags[fixture_random.draw(flags.size())];
        }
        SsiMonsterTemplate source;
        source.character[0x10] = 18; source.character[0x16] = 100; source.character[0x13] = 17;
        source.character[0xaa] = 1; source.character[0x2d] = 41; source.character[0x2e] = 2;
        for (unsigned j = 0; j < i%15; ++j) {
            auto& item = source.items.emplace_back();
            item[0x2e] = types[fixture_random.draw(types.size())];
            item[0x34] = ready[fixture_random.draw(ready.size())]; item[0x32] = fixture_random.draw(256);
        }
        const auto before = source;
        for (const auto& slot : resolve_ssi_readied_equipment(source,definitions)) append(equipment_hash,slot ? *slot : 255);
        const auto stats = prepare_ssi_weapon_stats(source,definitions);
        append(stats_hash,stats ? 1 : 0);
        if (stats) for (const auto value : {int(stats->thac0_code),int(stats->small_medium.count),
            int(stats->small_medium.sides),stats->small_medium.bonus}) append(stats_hash,value);
        assert(source == before);
    }
    assert(equipment_hash == 17918659399157245257ull);
    assert(stats_hash == 5740228811019828493ull);
    SsiMonsterTemplate source; source.items.emplace_back()[0x2e] = 255;
    assert(!prepare_ssi_weapon_stats(source,{})); // Unreadied items do not dereference the table.
    source.items[0][0x34] = 1;
    rejects([&] { prepare_ssi_weapon_stats(source,{}); });
}
void base_items_and_weapons() {
    std::array<std::uint8_t,16> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = i;
    bytes[4] = 128; bytes[11] = 255;
    const auto definition = decode_ssi_base_item(bytes);
    assert(definition.slot == 0 && definition.hands == 1 &&
        (definition.large == SsiDamageDice{2,3,-128}) && (definition.small_medium == SsiDamageDice{9,10,-1}));
    assert(definition.rate == 5 && definition.armor_descriptor == 6 && definition.weapon_type == 7 &&
        definition.unknown8 == 8 && definition.range == 12 && definition.class_mask == 13 &&
        definition.weapon_flags == 14 && definition.unknown15 == 15);
    for (const auto& [encoded,expected] : std::array<std::pair<std::uint8_t,int>,4>{{{0,0},{127,127},{128,-128},{255,-1}}}) {
        std::array<std::uint8_t,63> item{}; item[0x32] = encoded;
        assert(decode_ssi_monster_item(item).magical_bonus == expected);
        auto record = bytes; record[4] = encoded; record[11] = encoded;
        const auto decoded = decode_ssi_base_item(record);
        assert(decoded.large.bonus == expected && decoded.small_medium.bonus == expected);
    }
    std::vector<std::uint8_t> table{255,255}; table.insert(table.end(),bytes.begin(),bytes.end());
    assert(decode_ssi_base_items(table) == std::vector<SsiBaseItem>{definition});
    table[0] = 0; table[1] = 0;
    assert(decode_ssi_base_items(table) == std::vector<SsiBaseItem>{definition}); // Prefix is not a count.
    assert(decode_ssi_base_items(std::vector<std::uint8_t>(4098)).size() == 256);
    for (const auto size : {0,1,2,17,19,4114})
        rejects([&] { decode_ssi_base_items(std::vector<std::uint8_t>(size)); });
    for (const auto size : {0,15,17})
        rejects([&] { decode_ssi_base_item(std::vector<std::uint8_t>(size)); });
    // Fingerprint of 960 unmodified native weapon-stat cases. Only the three
    // ability helper boundaries were supplied synthetic signed-byte results.
    std::vector<SsiBaseItem> definitions(256);
    const std::array<int,5> bonuses{-128,-1,0,1,127};
    const std::array<int,4> baselines{0,39,60,255};
    std::uint64_t hash = 14695981039346656037ull;
    unsigned i = 0;
    for (const auto flags : {0,1,2,4,128,129,135,255}) for (const auto kind : {0,36,37,40,41,44,45,255})
        for (const auto race : {0,2,255}) for (const auto magic : bonuses) {
            auto& base = definitions[kind];
            base.weapon_flags = flags;
            base.small_medium = {static_cast<std::uint8_t>(i%256),static_cast<std::uint8_t>((i*7)%256),bonuses[(i/4)%5]};
            SsiMonsterItem item; item.type = kind; item.magical_bonus = magic;
            SsiWeaponContext context;
            context.base_thac0_code = baselines[i%4]; context.race_code = race;
            context.dexterity_to_hit = bonuses[(i/7)%5]; context.strength_to_hit = bonuses[(i/11)%5];
            context.strength_to_damage = bonuses[(i/13)%5];
            if (i%3) context.arrow_bonus = bonuses[i%5];
            if (i%4) context.bolt_bonus = bonuses[(i/5)%5];
            const auto result = ssi_weapon_stats(definitions,item,context);
            for (const auto byte : {result.thac0_code,result.small_medium.count,result.small_medium.sides,
                static_cast<std::uint8_t>(result.small_medium.bonus)}) {
                hash ^= byte; hash *= 1099511628211ull;
            }
            ++i;
        }
    assert(i == 960 && hash == 5567199630209685347ull);
    SsiMonsterItem weapon; SsiWeaponContext context;
    definitions[0].weapon_flags = 2;
    rejects([&] { ssi_weapon_stats(definitions,weapon,context); });
    context.dexterity_to_hit = 128;
    rejects([&] { ssi_weapon_stats(definitions,weapon,context); });
    definitions[0].weapon_flags = 4;
    context.strength_to_hit = 0;
    rejects([&] { ssi_weapon_stats(definitions,weapon,context); });
    definitions[0].weapon_flags = 1; context.arrow_bonus = -129;
    rejects([&] { ssi_weapon_stats(definitions,weapon,context); });
    definitions[0].weapon_flags = 0; weapon.magical_bonus = 128;
    rejects([&] { ssi_weapon_stats(definitions,weapon,context); });
    rejects([&] { ssi_weapon_stats(std::span<const SsiBaseItem>{},weapon,context); });
}
void monster_attachments() {
    // Matches the synthetic parameter grid exercised in native BA:0BB8..0D84.
    for (const auto weight : {0,1,255,256,65535}) for (const auto quantity : {0,1,2,255})
        for (const auto equipped : {0,1,2,255}) for (const auto type : {0,7,255}) {
            std::array<std::uint8_t,63> record{};
            record[0x2e] = type; record[0x34] = equipped; record[0x39] = quantity;
            record[0x37] = weight & 255; record[0x38] = weight >> 8;
            std::fill_n(record.begin()+0x2a,4,0xff); // Never follow serialized pointers.
            const auto before = record;
            const auto item = decode_ssi_monster_item(record);
            assert((item == SsiMonsterItem{static_cast<std::uint8_t>(type),static_cast<std::uint8_t>(quantity),
                equipped != 0,static_cast<std::uint16_t>(weight)}));
            assert(item.stack_weight() == (unsigned(weight)*unsigned(std::max(quantity,1)) & 65535));
            assert(record == before);
        }
    for (const auto timer : {0,1,2,10,11,255,256,65535}) for (const auto flag : {0,1,2,255}) {
        const std::array<std::uint8_t,9> record{39,static_cast<std::uint8_t>(timer & 255),
            static_cast<std::uint8_t>(timer >> 8),255,static_cast<std::uint8_t>(flag),255,255,255,255};
        const auto effect = decode_ssi_monster_effect(record);
        assert((effect == SsiMonsterEffect{39,static_cast<std::uint16_t>(timer),255,flag != 0}));
    }
    // Presence alone controls encounter movement. Unknown IDs are retained and
    // ignored here; zero timers, list order and duplicates do not alter priority.
    const std::vector<std::vector<std::uint8_t>> effect_lists{
        {},{1},{0x27},{0x2a},{0x27,0x2a},{0x2a,0x27},{0x27,0x27}};
    for (unsigned speed = 0; speed <= 255; ++speed) for (const auto& ids : effect_lists) {
        SsiMonsterTemplate source; source.character[0x11c] = speed;
        for (const auto id : ids) source.effects.push_back({id,0,0,0,0,0,0,0,0});
        const bool twice = std::find(ids.begin(),ids.end(),0x27) != ids.end();
        const bool half = std::find(ids.begin(),ids.end(),0x2a) != ids.end();
        const auto before = source;
        assert(ssi_encounter_movement(source) == (twice ? (speed*2)%256 : (half ? speed/2 : speed)));
        assert(source == before);
    }
    for (const auto size : {0,62,64})
        rejects([&] { decode_ssi_monster_item(std::vector<std::uint8_t>(size)); });
    for (const auto size : {0,8,10})
        rejects([&] { decode_ssi_monster_effect(std::vector<std::uint8_t>(size)); });
}
void monster_templates() {
    std::vector<std::uint8_t> character(285),items(126),effects(18);
    for (std::size_t i = 0; i < character.size(); ++i) character[i] = (i*17+3)%256;
    for (std::size_t i = 0; i < items.size(); ++i) items[i] = (i*13+7)%256;
    for (std::size_t i = 0; i < effects.size(); ++i) effects[i] = (i*19+11)%256;
    character[0x11b] = 42; character[0x11c] = 12; character[0x111] = 67; character[0x110] = 55;
    const auto source = character;
    const auto decoded = decode_ssi_monster_resources(character,items,effects);
    assert((decoded.stats() == SsiMonsterStats{42,12,-7,5}));
    for (std::size_t i = 0; i < character.size(); ++i) {
        const bool pointer = (i >= 0x7f && i < 0x83) || (i >= 0xc8 && i < 0xcc) ||
            (i >= 0x104 && i < 0x10c);
        assert(decoded.character[i] == (pointer ? 0 : character[i]));
    }
    assert(decoded.items.size() == 2 && decoded.effects.size() == 2);
    for (std::size_t i = 0; i < items.size(); ++i)
        assert(decoded.items[i/63][i%63] == (i%63 >= 42 && i%63 < 46 ? 0 : items[i]));
    for (std::size_t i = 0; i < effects.size(); ++i)
        assert(decoded.effects[i/9][i%9] == (i%9 >= 5 ? 0 : effects[i]));
    const auto actors = materialize_ssi_monster_group(decoded,63,8,{1,2,3,4,6});
    assert(actors.size() == 63 && actors[0] == actors[62] && character == source);
    assert(actors[0].character[0xbf] == 8 && actors[0].character[0xc1] == 145 &&
        actors[0].character[0xc2] == 162 && actors[0].character[0xc3] == 179 &&
        actors[0].character[0xc4] == 196 && actors[0].character[0xc5] == 196 &&
        actors[0].character[0xc6] == 230 && actors[0].stats() == decoded.stats());
    auto independent = actors;
    independent[0].items[0][0] ^= 1; independent[0].effects[0][0] ^= 1; independent[0].character[0x11b] = 0;
    assert(independent[1] == actors[1] && independent[0] != actors[0]);
    for (const auto count : {0,64,255})
        rejects([&] { materialize_ssi_monster_group(decoded,count,8,{1,2,3,4,6}); });
    for (const auto roll : {0,7,255})
        rejects([&] { materialize_ssi_monster_group(decoded,1,8,{1,2,3,4,static_cast<std::uint8_t>(roll)}); });
    const auto empty = decode_ssi_monster_resources(character,{},{});
    assert(empty.items.empty() && empty.effects.empty());
    // Whole-character fingerprints from the unmodified native initializer with
    // this synthetic record, after pointer reset and before group assignment.
    const std::array<std::uint32_t,6> seeds{0,1,10,19,0xffffffff,0x12345678};
    const std::array<std::uint64_t,6> hashes{16231138795531230182ull,7236336294179404197ull,
        10598656561769504289ull,11904246922652444164ull,6601990890219560637ull,3773905603647061516ull};
    auto native_record = character; native_record[0x2e] = 0;
    const auto native_template = decode_ssi_monster_resources(native_record,{},{});
    for (std::size_t i = 0; i < seeds.size(); ++i) {
        HighWordLcg32 random(seeds[i],0x08088405,1);
        std::array<std::uint8_t,5> rolls;
        for (auto& roll : rolls) roll = random.draw(6)+1;
        const auto result = materialize_ssi_monster_group(native_template,1,native_record[0xbf],rolls);
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto byte : result[0].character) { hash ^= byte; hash *= 1099511628211ull; }
        assert(hash == hashes[i]);
    }
    for (unsigned race = 0; race <= 255; ++race) {
        auto record = character; record[0x2e] = race;
        const auto result = materialize_ssi_monster_group(decode_ssi_monster_resources(record,{},{}),1,8,{1,2,3,4,5});
        const auto expected = race == 1 || race == 3 || race == 5 ? 1 :
            (race == 2 || race == 4 || race == 6 || race == 7 ? 2 : record[0xc0]);
        assert(result[0].character[0xc0] == expected);
    }
    for (const auto byte : {0,39,60,255}) {
        auto record = character; record[0x111] = byte; record[0x110] = byte;
        const auto stats = decode_ssi_monster_resources(record,{},{}).stats();
        assert(stats.armor_class == 60-byte && stats.thac0 == 60-byte);
    }
}
void encounter_visibility() {
    struct EncounterHost : Host {
        unsigned sprites{};
        std::uint8_t distance{};
        void sprite_start(EclSpriteRequest request) override { ++sprites; distance = request.distance; }
        std::size_t menu(std::span<const std::string>, std::string_view, bool) override { return 0; }
    };
    auto code = std::vector<std::uint8_t>{0x29,0,7,0,2,0,8,1,0x20,0x4a};
    for (unsigned i = 0; i < 5; ++i) code.insert(code.end(),{0,0});
    code.insert(code.end(),{0x80,0,0x80,0,0x80,0,0,6,0,12,0});
    const auto fixture = [&] {
        auto session = native_session(lifecycle(code,{0}));
        session.encounter = EclEncounterBinding{EclEncounterDialect::menu14,0x4a32,{0x4a30,0x4a31}};
        session.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::disabled,
            EclPartyDialect::disabled,EclPartyCheckDialect::disabled,EclSurpriseDialect::disabled,EclEncounterDialect::menu14);
        session.row = session.column = 8;
        return session;
    };
    // Native helper observations: first-edge wall graphics, independent of door flags.
    const int dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
    for (unsigned direction = 0; direction < 4; ++direction) for (unsigned blocked = 0; blocked < 3; ++blocked)
        for (const auto wall : {1,10,15}) for (const auto mode : {0,1}) for (const auto door : {0,1}) {
            auto session = fixture(); session.facing = static_cast<Direction>(direction);
            session.state.memory[0x4a32] = mode;
            if (blocked < 2) {
                auto& cell = session.map.cells[(8+blocked*dy[direction])*16+8+blocked*dx[direction]];
                std::uint8_t* walls[]{&cell.north,&cell.east,&cell.south,&cell.west};
                *walls[direction] = wall; cell.door_flags = door << (direction*2);
            }
            EncounterHost host;
            const auto run = session.trigger(EclEventKind::move,host);
            assert(run.exited && host.sprites == 1 && host.distance == (mode ? blocked : 2));
            assert(session.row == 8 && session.column == 8 && session.state.memory[0x4a31] == host.distance);
        }
    auto boundary = fixture(); boundary.column = 0; boundary.facing = Direction::west;
    boundary.state.memory[0x4a32] = 1;
    EncounterHost host;
    assert(boundary.trigger(EclEventKind::move,host).diagnostic ==
        "Encounter visibility across map boundaries is unverified");
    assert(host.sprites == 0);
}
void movement_lifecycle() {
    Host host;
    const auto source = std::vector<std::uint8_t>{9,1,0,0x71,1,0,0x4a,
        9,1,3,0x71,1,2,0x4a,9,1,1,0x72,1,4,0x4a,0};
    const auto destination = std::vector<std::uint8_t>{9,1,0,0x71,1,1,0x4a,9,1,3,0x71,1,3,0x4a,0};
    auto session = native_session(lifecycle(source,destination));
    session.map.cells[0].raw_event = 0x81; session.map.cells[1].raw_event = 0x82;
    session.map.cells[1].west = 7; // The native lifecycle checks the source edge only.
    auto move = session.move(Direction::east,host);
    assert(move.moved && move.execution->exited && session.column == 1);
    assert(session.state.memory[0x4a00] == 0 && session.state.memory[0x4a01] == 1);
    assert(session.state.memory[0x4a02] == 0x81 && session.state.memory[0x4a03] == 0x82);
    assert(session.state.memory[0x7202] == 0 && session.state.memory[0x7203] == 0);
    session.map.cells[1].east = 1;
    session.state.memory[0x4a03] = 0;
    move = session.move(Direction::east,host);
    assert(!move.moved && move.execution->exited && session.column == 1);
    assert(session.state.memory[0x4a00] == 1 && session.state.memory[0x4a03] == 0x82);
    const auto saved = serialize_exploration(session,"movement-fixture");
    auto restored = native_session(session.ecl_record); restored.map = session.map;
    restore_exploration(restored,saved,"movement-fixture");
    assert(serialize_exploration(restored,"movement-fixture") == saved);
    for (unsigned direction = 0; direction < 4; ++direction) for (unsigned door : {1,2,3}) {
        auto edge = native_session(lifecycle(source,destination));
        edge.row = direction == 0 ? 0 : direction == 2 ? 15 : 8;
        edge.column = direction == 1 ? 15 : direction == 3 ? 0 : 8;
        auto& cell = edge.map.cells[edge.row*16+edge.column];
        std::uint8_t* walls[]{&cell.north,&cell.east,&cell.south,&cell.west};
        *walls[direction] = 1; cell.door_flags = door << (direction*2);
        const auto old_row = edge.row, old_column = edge.column;
        const auto step = edge.move(static_cast<Direction>(direction),host);
        assert(step.execution->exited && step.moved == (door == 1));
        assert(edge.state.memory[0x4a04] == 1);
        if (door == 1) {
            assert(edge.row == (direction == 0 ? 15 : direction == 2 ? 0 : old_row));
            assert(edge.column == (direction == 1 ? 0 : direction == 3 ? 15 : old_column));
        } else assert(edge.row == old_row && edge.column == old_column);
    }
    auto blocked = native_session(lifecycle({9,0,255,1,0,0x72,0},{9,1,0,0x72,1,0,0x4a,0}));
    auto open_edge = native_session(lifecycle(source,destination));
    move = open_edge.move(Direction::west,host);
    assert(move.moved && move.execution->exited && open_edge.column == 15 && open_edge.state.memory[0x4a04] == 1);
    move = blocked.move(Direction::east,host);
    assert(!move.moved && move.execution->exited && blocked.state.memory[0x4a00] == 0);
    auto reposition = native_session(lifecycle({9,0,7,1,0,0x71,9,0,8,1,1,0x71,0},destination));
    move = reposition.move(Direction::east,host);
    assert(move.moved && move.execution->exited && reposition.column == 8 && reposition.row == 8);
    assert(reposition.state.memory[0x7202] == 7 && reposition.state.memory[0x7203] == 8);
    auto invalid = native_session(lifecycle({9,0,16,1,0,0x71,0},destination));
    move = invalid.move(Direction::east,host);
    assert(!move.moved && !move.execution->exited && invalid.column == 0);
    for (const auto bad_address : {0x7100,0x7201,0x9900}) {
        auto conflict = native_session(lifecycle(source,destination));
        (*conflict.movement_addresses)[0] = bad_address;
        conflict.state.memory[0x7201] = 17;
        move = conflict.move(Direction::east,host);
        assert(!move.moved && !move.execution->exited && conflict.state.memory[0x7201] == 17);
    }
    auto failure = native_session(lifecycle({0xff},destination));
    move = failure.move(Direction::east,host);
    assert(!move.moved && !move.execution->exited);
    auto limited = native_session(lifecycle(source,destination));
    const auto exact = limited.trigger(EclEventKind::move,host).steps;
    move = limited.move(Direction::east,host,{exact,64});
    assert(move.moved && !move.execution->exited && move.execution->steps == exact);
    auto transfer = native_session(lifecycle({0x20,0,42},destination));
    const auto increment = std::vector<std::uint8_t>{4,1,0x10,0x4a,0,1,1,0x10,0x4a,0};
    const auto next = lifecycle(increment,{4,1,0x11,0x4a,0,1,1,0x11,0x4a,0},
        {9,0,5,1,0,0x71,9,0,6,1,1,0x71,0});
    transfer.programs = ProgramResources{[&](std::uint8_t id) { assert(id == 42); return ProgramResource{id,next}; },"fixture"};
    transfer.current_program_record = 7;
    auto refused = transfer;
    host.reject_program = true;
    const auto rejected = refused.move(Direction::east,host);
    assert(!rejected.execution->exited && refused.current_program_record == 7);
    host.reject_program = false;
    auto clearing_transfer = transfer;
    clearing_transfer.transfer_clear_ranges = {{0x4a00,32},{0x6e79,10}};
    for (const auto address : {0x49ff,0x4a00,0x4a10,0x4a17,0x4a1f,0x4a20,
        0x6e78,0x6e79,0x6e82,0x6e83}) clearing_transfer.state.memory[address] = 7;
    auto unresolved = clearing_transfer;
    unresolved.programs->resolve = [](std::uint8_t) -> ProgramResource { throw std::runtime_error("Missing program"); };
    auto reset_move = unresolved.move(Direction::east,host);
    assert(!reset_move.execution->exited && unresolved.state.memory[0x4a17] == 7);
    reset_move = clearing_transfer.move(Direction::east,host);
    assert(reset_move.execution->exited && reset_move.execution->program_transfers == 1);
    assert(clearing_transfer.state.memory[0x4a10] == 1 && clearing_transfer.state.memory[0x4a11] == 1);
    for (const auto address : {0x4a00,0x4a17,0x4a1f,0x6e79,0x6e82})
        assert(clearing_transfer.state.memory[address] == 0);
    for (const auto address : {0x49ff,0x4a20,0x6e78,0x6e83}) assert(clearing_transfer.state.memory[address] == 7);
    clearing_transfer.state.memory[0x4a17] = 9;
    const auto transfer_save = serialize_exploration(clearing_transfer,"transfer-clear-fixture");
    auto transfer_restore = clearing_transfer;
    restore_exploration(transfer_restore,transfer_save,"transfer-clear-fixture");
    assert(transfer_restore.state.memory[0x4a17] == 9); // Restore preserves saved temporaries.
    assert(transfer_restore.trigger(EclEventKind::search,host).exited && transfer_restore.state.memory[0x4a17] == 9);
    for (const auto& ranges : std::vector<std::vector<std::array<std::uint16_t,2>>>{
        {{0x4a00,0}},{{65535,2}},{{0x4a00,2},{0x4a01,1}},{{0x7100,1}},{{0x7200,1}},{{0x9900,1}}}) {
        auto invalid_reset = clearing_transfer;
        invalid_reset.transfer_clear_ranges = ranges;
        const auto attempt = invalid_reset.move(Direction::east,host);
        assert(!attempt.execution->exited && invalid_reset.state.memory[0x4a17] == 9);
    }
    move = transfer.move(Direction::east,host);
    assert(move.execution->exited && move.execution->program_transfers == 1 && move.moved);
    assert(!host.programs.empty() && host.programs.back() == 42);
    assert(transfer.row == 6 && transfer.column == 5); // No duplicate physical step/search.
    assert(transfer.state.memory[0x4a10] == 1 && transfer.state.memory[0x4a11] == 1);
    assert(transfer.state.memory[0x7201] == 0);
    const auto self_code = lifecycle({3,1,0,0x4a,0,0,0x16,0x20,0,42,0},
        {4,1,0x11,0x4a,0,1,1,0x11,0x4a,0},{9,0,1,1,0,0x4a,0});
    auto self = native_session(self_code); self.current_program_record = 42;
    self.programs = ProgramResources{[&](std::uint8_t id) { return ProgramResource{id,self_code}; },"self"};
    move = self.move(Direction::east,host);
    assert(move.execution->exited && move.execution->program_transfers == 1 && self.column == 0 &&
        self.state.memory[0x4a11] == 1);
    auto load = native_session(lifecycle({9,0,7,1,0,0x71,9,0,8,1,1,0x71,0x21,0,9,0,2,0,0,0},destination));
    load.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::map3);
    load.maps = MapResources{[](std::uint8_t id) { GeoMap map; map.dax_record_id = id; return map; },"maps"};
    move = load.move(Direction::east,host);
    assert(move.execution->exited && load.map.dax_record_id == 9 && load.column == 8 && load.row == 8);
    load.load_area({255,255,127});
    assert(!load.has_dungeon_map() && !load.move(Direction::east));
    const auto inactive = serialize_exploration(load,"inactive-fixture");
    auto resumed = load;
    restore_exploration(resumed,inactive,"inactive-fixture");
    assert(!resumed.has_dungeon_map() && serialize_exploration(resumed,"inactive-fixture") == inactive);
    move = resumed.move(Direction::east,host);
    assert(!move.moved && !move.execution->exited && resumed.column == 8);
    resumed.load_area({9,2,0});
    assert(resumed.has_dungeon_map() && resumed.move(Direction::east,host).execution->exited);
    auto clearing = native_session(lifecycle({0x21,0,255,0,255,0,127,0},{0}));
    clearing.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::map3);
    clearing.maps = load.maps;
    const auto old_clears = host.clears;
    move = clearing.move(Direction::east,host);
    assert(!move.moved && !move.execution->exited && !clearing.has_dungeon_map() && host.clears == old_clears+1);
    auto cycle = native_session(lifecycle({1,1,20,0x99},destination));
    move = cycle.move(Direction::east,host,{16,64});
    assert(!move.moved && !move.execution->exited && move.execution->steps == 16);
    for (const auto [x,y] : {std::pair{0,0},std::pair{15,0},std::pair{0,15},std::pair{15,15},std::pair{7,8}}) {
        for (unsigned dir = 0; dir < 4; ++dir) {
            auto forced = native_session(lifecycle({0x2d,1,0x56,0x34,0},{0}));
            forced.position_advance_call = EclPositionAdvanceCall{0x3456,0x7104};
            forced.column = x; forced.row = y; forced.facing = static_cast<Direction>(dir);
            for (auto& cell : forced.map.cells) {
                cell.north = cell.east = cell.south = cell.west = 7; cell.raw_event = 37;
            }
            const auto run = forced.trigger(EclEventKind::move,host);
            const int dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
            assert(run.exited && forced.column == (x+dx[dir]+16)%16 && forced.row == (y+dy[dir]+16)%16);
            assert(forced.state.memory[0x7103] == 37 && forced.state.memory[0x7104] == 7);
            forced.position_refresh_call = 0x3456;
            assert(!forced.trigger(EclEventKind::move,host).exited);
            forced.position_refresh_call.reset(); forced.position_advance_call->wall_type_address = 0x7200;
            assert(!forced.trigger(EclEventKind::move,host).exited);
        }
    }
}
} // namespace

int main() {
    attack_primitives();
    weapon_inputs();
    base_items_and_weapons();
    monster_attachments();
    monster_templates();
    monster_setup();
    encounter_visibility();
    movement_lifecycle();
    // Every GEO16 direction permits an ordinary door, in both directions,
    // while closed states, inconsistent opposing walls and boundaries stay solid.
    for (unsigned direction = 0; direction < 4; ++direction) {
        ExplorationSession doors;
        doors.row = doors.column = 8;
        const int dy[]{-1,0,1,0}, dx[]{0,1,0,-1};
        const auto target = std::size_t((8+dy[direction])*16+8+dx[direction]);
        const auto reverse = (direction+2)%4;
        auto set_wall = [](GeoCell& cell, unsigned d) {
            std::uint8_t* walls[]{&cell.north,&cell.east,&cell.south,&cell.west};
            *walls[d] = 7;
        };
        auto& source = doors.map.cells[8*16+8];
        auto& destination = doors.map.cells[target];
        set_wall(source,direction); set_wall(destination,reverse);
        for (unsigned state = 0; state < 4; ++state) {
            source.door_flags = state << (direction*2);
            destination.door_flags = state << (reverse*2);
            doors.row = doors.column = 8;
            assert(doors.move(static_cast<Direction>(direction)) == (state == 1));
            if (state == 1) {
                assert(doors.row == 8+dy[direction] && doors.column == 8+dx[direction]);
                assert(doors.move(static_cast<Direction>(reverse)));
            }
            assert(doors.row == 8 && doors.column == 8);
        }
        source.door_flags = 1u << (direction*2); destination.door_flags = 0;
        assert(!doors.move(static_cast<Direction>(direction)));
    }
    ExplorationSession boundary;
    boundary.map.cells[0].north = 1; boundary.map.cells[0].door_flags = 1;
    assert(!boundary.move(Direction::north));
    {
        const auto script = [](std::initializer_list<std::uint8_t> body) {
            std::vector<std::uint8_t> bytes;
            for (int i = 0; i < 5; ++i) bytes.insert(bytes.end(),{1,1,20,0x99});
            bytes.insert(bytes.end(),body); return bytes;
        };
        const auto boot = script({0x20,0,42});
        const auto destination = script({9,0,3,1,0,0x4a,0x11,0x80,1,4,0});
        ExplorationSession campaign;
        campaign.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true,
            std::array<std::uint16_t,3>{0xc04b,0xc04c,0xc04d}};
        campaign.opcodes = minimal_vm_opcodes(); campaign.ecl_record = boot;
        campaign.current_program_record = 0;
        campaign.programs = ProgramResources{[&](std::uint8_t id) {
            if (id != 0 && id != 42) throw std::runtime_error("Missing synthetic program");
            return ProgramResource{id,id == 0 ? boot : destination};
        },"synthetic-programs-v1"};
        campaign.campaign_start = std::array<std::uint8_t,2>{0,0};
        campaign.maps = MapResources{[](std::uint8_t id) { GeoMap map; map.dax_record_id = id; return map; },
            "synthetic-maps-v1"};
        Host begin_output;
        rejects([&] { campaign.begin_campaign(begin_output); });
        campaign.party.emplace_back(); campaign.party[0].name = "Synthetic";
        campaign.state.random = HighWordLcg32(123,134775813,1);
        campaign.state.memory[0x4a01] = 99;
        auto begun = campaign;
        assert(begun.begin_campaign(begin_output).exited && begin_output.output == "AAA");
        assert(begun.state.memory[0x4a01] == 0 && begun.party[0].name == "Synthetic");
        assert(begun.state.random->state() == 123);
        campaign.party.clear(); campaign.state.random.reset();
        auto initial = campaign; Host output;
        const auto run = campaign.trigger(EclEventKind::initialize,output);
        assert(run.exited && run.steps == 14 && !run.next_program && output.output == "AAA");
        assert(campaign.current_program_record == 42 && campaign.state.memory[0x4a00] == 3);
        const auto saved = serialize_exploration(campaign,"synthetic-campaign");
        restore_exploration(initial,saved,"synthetic-campaign");
        assert(initial.current_program_record == 42 && initial.ecl_record == destination);
        assert(initial.state.memory == campaign.state.memory);
        auto invalid = saved;
        const auto location = invalid.find("synthetic-programs-v1");
        assert(location != std::string::npos);
        invalid.replace(location,21,"synthetic-programs-v2");
        rejects([&] { restore_exploration(initial,invalid,"synthetic-campaign"); });
        assert(initial.current_program_record == 42 && initial.ecl_record == destination);
        campaign.ecl_record = boot; campaign.current_program_record = 0;
        auto missing = campaign; missing.programs.reset();
        assert(!missing.trigger(EclEventKind::initialize,output).exited && missing.current_program_record == 0);
        auto wrong = campaign;
        wrong.programs->resolve = [&](std::uint8_t) { return ProgramResource{43,destination}; };
        assert(!wrong.trigger(EclEventKind::initialize,output).exited && wrong.ecl_record == boot);
        auto malformed = campaign;
        malformed.programs->resolve = [](std::uint8_t id) { return ProgramResource{id,{0xff}}; };
        assert(!malformed.trigger(EclEventKind::initialize,output).exited && malformed.ecl_record == boot);
        auto endless = campaign;
        endless.programs->resolve = [&](std::uint8_t id) { return ProgramResource{id,boot}; };
        assert(!endless.trigger(EclEventKind::initialize,output).exited);
        assert(!campaign.trigger(EclEventKind::initialize,output,{5,64}).exited);
        campaign.programs.reset(); campaign.current_program_record.reset();
        campaign.position_refresh_call = 0x2c90;
        campaign.map.cells[31].raw_event = 0x82;
        campaign.ecl_record = script({9,0,15,1,0x4b,0xc0,9,0,1,1,0x4c,0xc0,
            9,0,3,1,0x4d,0xc0,0x2d,1,0x90,0x2c,0});
        assert(campaign.trigger(EclEventKind::initialize,output).exited);
        assert(campaign.row == 1 && campaign.column == 15 && campaign.facing == Direction::west);
        assert(campaign.state.memory[0xc04f] == 0x82);
        campaign.sound_call = EclSoundCall{0x2c90,0x03de,{}};
        assert(!campaign.trigger(EclEventKind::initialize,output).exited);
    }
    {
        ExplorationSession audio;
        audio.dialect = {"ssi-five-entry-v1",0x9900,0xc04f};
        audio.opcodes = minimal_vm_opcodes();
        for (int i = 0; i < 5; ++i) audio.ecl_record.insert(audio.ecl_record.end(),{1,1,20,0x99});
        audio.ecl_record.insert(audio.ecl_record.end(),{0x2d,1,3,0xba,0});
        audio.sound_call = EclSoundCall{0xba03,0x03de,{{8,10},{10,11}}};
        Host sink;
        for (const std::uint16_t selector : {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,264,266}) {
            auto script = std::vector<std::uint8_t>(audio.ecl_record.begin(),audio.ecl_record.begin()+20);
            script.insert(script.end(),{9,2,static_cast<std::uint8_t>(selector),
                static_cast<std::uint8_t>(selector >> 8),1,0xde,3,0x2d,1,3,0xba,0});
            audio.ecl_record = script;
            sink.sounds.clear();
            assert(audio.trigger(EclEventKind::move,sink).exited);
            assert(audio.state.memory[0x03de] == (selector & 255));
            assert(audio.state.memory[0x03df] == (selector >> 8));
            assert(sink.sounds == (selector == 8 ? std::vector<std::uint16_t>{10} :
                selector == 10 ? std::vector<std::uint16_t>{11} : std::vector<std::uint16_t>{}));
        }
        // A byte WRITE clears the previous high byte of this native word register.
        audio.ecl_record.resize(20);
        audio.ecl_record.insert(audio.ecl_record.end(),{9,0,8,1,0xde,3,0x2d,1,3,0xba,0});
        assert(audio.trigger(EclEventKind::move,sink).exited && sink.sounds.back() == 10);
        assert(audio.state.memory[0x03df] == 0);
        const auto saved = serialize_exploration(audio,"synthetic-audio");
        audio.state.memory[0x03de] = 0;
        restore_exploration(audio,saved,"synthetic-audio");
        assert(audio.state.memory[0x03de] == 8);
        audio.ecl_record.resize(20);
        audio.ecl_record.insert(audio.ecl_record.end(),{0x2d,1,3,0xba,0});
        audio.sound_call->function = 0xba04;
        assert(!audio.trigger(EclEventKind::move,sink).exited);
        audio.sound_call->function = 0xba03;
        audio.sound_call->selector_address = 65535;
        assert(!audio.trigger(EclEventKind::move,sink).exited);
        audio.sound_call->selector_address = 0x03de;
        audio.ecl_record.resize(20);
        audio.ecl_record.insert(audio.ecl_record.end(),{9,0,0,1,0xdf,3,0});
        assert(!audio.trigger(EclEventKind::move,sink).exited);

    }
    ExplorationSession session;
    session.map = decode_geo16_record(std::vector<std::uint8_t>(1026), 7);
    session.map.cells[1].raw_event = 0x81;
    session.dialect = {"ssi-five-entry-v1",0x9900,0xc04f,true, std::array<std::uint16_t,3>{0xc04b,0xc04c,0xc04d}};
    session.opcodes = minimal_vm_opcodes();
    for (int i = 0; i < 5; ++i) session.ecl_record.insert(session.ecl_record.end(), {1,1,20,0x99});
    session.ecl_record.insert(session.ecl_record.end(), {9,1,0x4f,0xc0,1,0,0x4a,0x11,0x80,1,4,0});
    Host host;
    assert(!session.move(Direction::north));
    assert(session.move(Direction::east));
    assert(session.trigger(EclEventKind::move, host).exited);
    assert(host.output == "A" && session.state.memory[0x4a00] == 0x81);
    assert(session.state.memory[0xc04b] == 1 && session.state.memory[0xc04c] == 0 && session.state.memory[0xc04d] == 1);
    session.map.cells[1].south = 2;
    assert(!session.move(Direction::south));
    session.map.cells[2].west = 1;
    assert(!session.move(Direction::east)); // opposing wall blocks inconsistent map cells
    const auto identity = "synthetic.game:GEO1:7:ECL1:3:synthetic-fingerprint";
    session.state.strings[0x4b00] = "Synthetic flag description";
    const auto save = serialize_exploration(session, identity);
    auto restored = session;
    restored.state = {};
    restored.row = 4;
    restored.column = 5;
    restore_exploration(restored, save, identity);
    assert(restored.row == session.row && restored.column == session.column && restored.facing == session.facing);
    assert(restored.state.memory == session.state.memory && restored.state.strings == session.state.strings);
    session.wall_banks = std::array<std::uint8_t,3>{7,127,255};
    const auto banked = serialize_exploration(session,identity);
    restore_exploration(restored,banked,identity);
    assert(restored.wall_banks == session.wall_banks);
    auto bad_banks = save;
    bad_banks.insert(bad_banks.size()-1,",\"wallBanks\":[7,256,255]");
    rejects([&] { restore_exploration(restored,bad_banks,identity); });
    assert(restored.wall_banks == session.wall_banks);
    restore_exploration(restored,save,identity);
    assert(!restored.wall_banks); // Existing saves remain readable by the engine.
    rejects([&] { restore_exploration(restored, save, "different-source"); });
    assert(restored.state.memory == session.state.memory); // invalid loads publish no partial state
    auto invalid = save;
    const auto coordinate = invalid.find("\"row\":0");
    assert(coordinate != std::string::npos);
    invalid.replace(coordinate, 7, "\"row\":99");
    rejects([&] { restore_exploration(restored, invalid, identity); });
    assert(restored.row == session.row);
    rejects([&] { restore_exploration(restored, "{}", identity); });
    session.ecl_record[20] = 0xff;
    assert(!session.trigger(EclEventKind::move, host).exited);
    assert(session.row == 0 && session.column == 1); // errors retain explicit partial progress
    ExplorationSession loading;
    loading.map.dax_record_id = 7;
    loading.row = 3; loading.column = 4;
    loading.dialect = session.dialect;
    loading.opcodes = minimal_vm_opcodes(EclImageDialect::disabled,EclAreaDialect::map3);
    unsigned lookups = 0;
    loading.maps = MapResources{[&](std::uint8_t id) {
        ++lookups;
        if (id != 0 && id != 7 && id != 8) throw std::runtime_error("Missing synthetic map");
        GeoMap map; map.dax_record_id = id;
        if (id == 8) { map.cells[3*16+4].north = 5; map.cells[3*16+4].raw_event = 0x82; }
        return map;
    },"synthetic-map-archive-v1"};
    const auto legacy = serialize_exploration(loading,identity);
    for (int i = 0; i < 5; ++i) loading.ecl_record.insert(loading.ecl_record.end(),{1,1,20,0x99});
    loading.ecl_record.insert(loading.ecl_record.end(),{0x21,0,8,0,2,0,99,9,0,42,1,0,0x4a,0});
    const auto script = loading.ecl_record;
    assert(loading.trigger(EclEventKind::initialize,host).exited);
    assert(loading.map.dax_record_id == 8 && loading.map.cells[3*16+4].north == 5);
    assert(loading.row == 3 && loading.column == 4 && loading.initial_map_record == 7);
    assert(loading.ecl_record == script && loading.state.memory[0x4a00] == 42);
    assert((loading.area == std::array<std::uint8_t,3>{8,2,99}));
    const auto before_reuse = lookups;
    loading.load_area({255,2,44});
    assert(lookups == before_reuse && loading.map.dax_record_id == 8);
    const auto bound = serialize_exploration(loading,identity);
    restore_exploration(loading,legacy,identity);
    assert(loading.map.dax_record_id == 7 && !loading.area);
    restore_exploration(loading,bound,identity);
    assert(loading.map.dax_record_id == 8 && loading.area);
    const auto good_area = loading.area;
    for (const auto mode : {2,127,255}) loading.load_area({8,static_cast<std::uint8_t>(mode),1});
    rejects([&] { loading.load_area({9,2,0}); });
    rejects([&] { loading.load_area({127,2,0}); });
    rejects([&] { loading.load_area({255,16,0}); });
    assert(loading.map.dax_record_id == 8 && loading.row == 3);
    loading.state.memory[0xc04b] = 255;
    rejects([&] { loading.load_area({7,2,0}); });
    assert(loading.map.dax_record_id == 8 && loading.column == 4);
    loading.state.memory[0xc04b] = 4;
    loading.maps->source_identity = "changed-archive";
    rejects([&] { restore_exploration(loading,bound,identity); });
    assert(loading.map.dax_record_id == 8);
    loading.maps->source_identity = "synthetic-map-archive-v1";
    const auto resolver = *loading.maps;
    loading.maps->resolve = [](std::uint8_t) { GeoMap map; map.dax_record_id = 3; return map; };
    rejects([&] { loading.load_area({8,2,0}); });
    rejects([&] { restore_exploration(loading,bound,identity); });
    loading.maps = resolver;
    loading.load_area({0,0,0});
    assert(loading.map.dax_record_id == 0);
    restore_exploration(loading,bound,identity);
    loading.maps.reset();
    rejects([&] { loading.load_area({8,2,0}); });
    rejects([&] { restore_exploration(loading,bound,identity); });
    assert(loading.map.dax_record_id == 8 && loading.area == good_area);
    auto conflicting = loading.opcodes;
    for (auto& op : conflicting) if (op.opcode == 0x21) op.operand_count = 2;
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,conflicting,loading.state,host).exited);
    conflicting = loading.opcodes; conflicting.push_back({0x21,"OTHER_MAP",3});
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,conflicting,loading.state,host).exited);
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,loading.opcodes,loading.state,host).exited);
    assert(!execute_ecl(script,EclEventKind::initialize,loading.dialect,minimal_vm_opcodes(),loading.state,host).exited);
    auto bad_operand = script; bad_operand[21] = 2; bad_operand.insert(bad_operand.begin()+23,2); // 520, not a byte.
    assert(!execute_ecl(bad_operand,EclEventKind::initialize,loading.dialect,loading.opcodes,loading.state,host).exited);
    loading.maps = resolver;
    for (const auto& change : {std::pair{std::string("\"mapRecord\":8"),std::string("\"mapRecord\":256")},
        std::pair{std::string("\"area\":[255,2,44]"),std::string("\"area\":[255,16,44]")},
        std::pair{std::string("\"area\":[255,2,44]"),std::string("\"area\":[255,2]")}}) {
        auto invalid_binding = bound;
        const auto at = invalid_binding.find(change.first);
        assert(at != std::string::npos);
        invalid_binding.replace(at,change.first.size(),change.second);
        rejects([&] { restore_exploration(loading,invalid_binding,identity); });
        assert(loading.map.dax_record_id == 8 && loading.area == good_area);
    }
    return 0;
}

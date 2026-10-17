// SPDX-License-Identifier: GPL-3.0-or-later
// Independent normalized creation model; research references in attribution/sources.json.
#include "goldbox/party.hpp"
#include "json_support.hpp"
#include <algorithm>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>

namespace goldbox {
namespace {
namespace j = json_support;
namespace bj = boost::json;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int number(const bj::value& value, int low, int high) {
    const auto n = j::integer(value,"creation value");
    require(n >= low && n <= high,"Creation value out of range");
    return static_cast<int>(n);
}
std::string id(const bj::value& value) {
    auto s = j::string(value,"creation ID");
    static const std::regex pattern("[a-z0-9][a-z0-9.-]{0,63}");
    require(std::regex_match(s,pattern),"Invalid creation ID"); return s;
}
std::string label(const bj::value& value) {
    auto s = j::string(value,"creation label");
    require(!s.empty() && s.size() <= 40 && std::all_of(s.begin(),s.end(),
        [](unsigned char c) { return c >= 32 && c < 127; }),"Invalid creation label"); return s;
}
template<class T> const auto& find(const std::vector<T>& values, std::string_view name) {
    const auto found = std::find_if(values.begin(),values.end(),[&](const auto& v) { return v.id == name; });
    require(found != values.end(),"Unknown character option"); return *found;
}
template<class T> void unique(const std::vector<T>& values) {
    require(!values.empty() && values.size() <= 64,"Invalid creation option count");
    std::set<std::string> ids;
    for (const auto& v : values) require(ids.insert(v.id).second,"Duplicate creation option ID");
}
std::vector<std::string> ids(const bj::value& value) {
    const auto& a = j::array(value,"creation IDs");
    require(!a.empty() && a.size() <= 64,"Invalid creation ID count");
    std::vector<std::string> result; std::set<std::string> seen;
    for (const auto& v : a) {
        auto s = id(v); require(seen.insert(s).second,"Duplicate creation reference"); result.push_back(std::move(s));
    }
    return result;
}
std::vector<int> scores(const bj::value& value, std::size_t count) {
    const auto& a = j::array(value,"ability limits"); require(a.size() == count,"Ability count mismatch");
    std::vector<int> result;
    for (const auto& v : a) result.push_back(number(v,1,1000));
    return result;
}
int die(HighWordLcg32& random, int sides) {
    return 1+random.draw(static_cast<std::uint16_t>(sides));
}
int die(std::mt19937& random, int sides) {
    const auto limit = std::numeric_limits<std::uint32_t>::max() -
        std::numeric_limits<std::uint32_t>::max()%static_cast<unsigned>(sides);
    for (int attempt = 0; attempt < 64; ++attempt) {
        const auto n = random();
        if (n < limit) return 1+static_cast<int>(n%static_cast<unsigned>(sides));
    }
    throw std::runtime_error("Dice rejection limit exceeded");
}
void discard_draw(HighWordLcg32& random, std::uint16_t bound) { (void)random.draw(bound); }
void discard_draw(std::mt19937& random, std::uint16_t bound) {
    if (bound) (void)die(random,bound);
    else (void)random();
}
bool race_scores(const CreationRules& rules, const Character& c) {
    const auto& race = find(rules.races,c.race); (void)find(rules.genders,c.gender);
    const auto& maximum = race.maximum.at(c.gender);
    if (c.abilities.size() != rules.abilities.size()) return false;
    for (std::size_t i = 0; i < c.abilities.size(); ++i)
        if (c.abilities[i].value < race.minimum[i] || c.abilities[i].value > maximum[i]) return false;
    return true;
}
std::string folded(std::string value) {
    for (auto& c : value) if (c >= 'a' && c <= 'z') c -= 'a'-'A';
    return value;
}
}
CreationRules read_creation_rules(const std::filesystem::path& path) {
    require(!std::filesystem::is_symlink(path),"Creation rules must not be a symlink");
    const auto parsed = j::read_json(path);
    const auto& root = j::object(parsed,"creation rules");
    j::fields_only(root,{"schemaVersion","id","partyLimit","nameLimit","dice","abilities","genders",
        "alignments","races","classes","percentile","initialRandomBounds"},"creation rules");
    const auto field = [&](const char* key) -> const bj::value& { return j::field(root,key,"creation rules"); };
    require(number(field("schemaVersion"),1,1) == 1,"Unsupported creation schema");
    CreationRules r; r.id = id(field("id"));
    if (const auto* value = root.if_contains("initialRandomBounds")) {
        const auto& bounds = j::array(*value,"initial random bounds");
        require(bounds.size() <= 16,"Too many initial random draws");
        for (const auto& bound : bounds) r.initial_random_bounds.push_back(number(bound,0,65535));
    }
    r.party_limit = number(field("partyLimit"),1,8); r.name_limit = number(field("nameLimit"),1,32);
    const auto& dice = j::object(field("dice"),"creation dice");
    j::fields_only(dice,{"count","sides","dropLowest","bounds"},"creation dice");
    r.dice_count = number(j::field(dice,"count","dice"),1,10);
    r.dice_sides = number(j::field(dice,"sides","dice"),2,100);
    r.drop_lowest = number(j::field(dice,"dropLowest","dice"),0,r.dice_count-1);
    if (const auto* value = dice.if_contains("bounds")) {
        const auto policy = j::string(*value,"ability bounds");
        require(policy == "reject" || policy == "clamp","Unknown ability bounds policy");
        if (policy == "clamp") r.bounds = CreationBounds::clamp;
    }
    const auto choices = [&](const char* key) {
        std::vector<CreationChoice> result;
        for (const auto& v : j::array(field(key),key)) {
            const auto& o = j::object(v,key); j::fields_only(o,{"id","label"},key);
            result.push_back({id(j::field(o,"id",key)),label(j::field(o,"label",key))});
        }
        unique(result); return result;
    };
    r.abilities = choices("abilities"); require(r.abilities.size() <= 16,"Too many abilities");
    r.genders = choices("genders"); r.alignments = choices("alignments");
    if (const auto* value = root.if_contains("percentile")) {
        const auto& o = j::object(*value,"percentile");
        j::fields_only(o,{"ability","at","rollMaximum"},"percentile");
        const auto& ability = find(r.abilities,id(j::field(o,"ability","percentile")));
        r.percentile_ability = static_cast<std::size_t>(&ability-r.abilities.data());
        r.percentile_at = number(j::field(o,"at","percentile"),1,1000);
        if (const auto* maximum = o.if_contains("rollMaximum")) r.percentile_roll_maximum = number(*maximum,1,100);
    }
    for (const auto& v : j::array(field("classes"),"classes")) {
        const auto& o = j::object(v,"class");
        j::fields_only(o,{"id","label","minimum","alignments","percentile","creationMinimum",
            "hitPoints","startingMoney"},"class");
        CreationClass c{id(j::field(o,"id","class")),label(j::field(o,"label","class")),
            scores(j::field(o,"minimum","class"),r.abilities.size()),ids(j::field(o,"alignments","class")),false,{}};
        const auto& flag = j::field(o,"percentile","class"); require(flag.is_bool(),"Invalid percentile flag");
        c.percentile = flag.as_bool(); require(!c.percentile || r.percentile_ability.has_value(),"Missing percentile policy");
        if (const auto* value = o.if_contains("hitPoints")) {
            const auto& hp = j::object(*value,"hit points");
            j::fields_only(hp,{"count","sides","minimum","ability","abilityMinimum","modifiers"},"hit points");
            CreationHitPoints rule;
            rule.count = number(j::field(hp,"count","hit points"),1,10);
            rule.sides = number(j::field(hp,"sides","hit points"),2,100);
            rule.minimum = number(j::field(hp,"minimum","hit points"),1,rule.count*rule.sides);
            const auto& ability = find(r.abilities,id(j::field(hp,"ability","hit points")));
            rule.ability = static_cast<std::size_t>(&ability-r.abilities.data());
            rule.ability_minimum = number(j::field(hp,"abilityMinimum","hit points"),1,1000);
            const auto& modifiers = j::array(j::field(hp,"modifiers","hit points"),"hit point modifiers");
            require(!modifiers.empty() && modifiers.size() <= 1000,"Invalid hit point modifier count");
            for (const auto& n : modifiers) rule.modifiers.push_back(number(n,-100,100));
            c.hit_points = std::move(rule);
        }
        if (const auto* value = o.if_contains("startingMoney")) {
            const auto& money = j::object(*value,"starting money");
            j::fields_only(money,{"currency","count","sides","multiplier"},"starting money");
            CreationMoney rule{id(j::field(money,"currency","starting money")),
                number(j::field(money,"count","starting money"),1,10),
                number(j::field(money,"sides","starting money"),2,100),
                number(j::field(money,"multiplier","starting money"),1,65535)};
            require(rule.count*rule.sides <= 65535/rule.multiplier,"Starting money overflows uint16");
            c.starting_money = std::move(rule);
        }
        if (const auto* value = o.if_contains("creationMinimum")) {
            const auto& values = j::array(*value,"class creation minimum");
            require(values.size() == r.abilities.size(),"Class creation minimum count mismatch");
            for (const auto& n : values) c.creation_minimum.push_back(number(n,0,1000));
        }
        for (const auto& a : c.alignments) (void)find(r.alignments,a);
        r.classes.push_back(std::move(c));
    }
    unique(r.classes);
    for (const auto& v : j::array(field("races"),"races")) {
        const auto& o = j::object(v,"race");
        j::fields_only(o,{"id","label","minimum","maximum","percentileMaximum","classes","adjustments",
            "startingAge","ageAdjustments"},"race");
        CreationRace race; race.id = id(j::field(o,"id","race")); race.label = label(j::field(o,"label","race"));
        race.minimum = scores(j::field(o,"minimum","race"),r.abilities.size());
        if (const auto* value = o.if_contains("adjustments")) {
            const auto& values = j::array(*value,"racial adjustments");
            require(values.size() == r.abilities.size(),"Racial adjustment count mismatch");
            for (const auto& n : values) race.adjustments.push_back(number(n,-100,100));
        }
        race.classes = ids(j::field(o,"classes","race"));
        for (const auto& c : race.classes) (void)find(r.classes,c);
        if (const auto* value = o.if_contains("ageAdjustments")) {
            const auto& entries = j::array(*value,"age adjustments");
            require(entries.size() <= 32,"Too many age adjustments");
            for (const auto& entry : entries) {
                const auto& age = j::object(entry,"age adjustment");
                j::fields_only(age,{"minimum","maximum","values","positiveOnly","wrapByte"},"age adjustment");
                CreationAgeAdjustment modifier;
                modifier.minimum = number(j::field(age,"minimum","age"),1,65535);
                modifier.maximum = number(j::field(age,"maximum","age"),modifier.minimum,65535);
                const auto& values = j::array(j::field(age,"values","age"),"age values");
                require(values.size() == r.abilities.size(),"Age adjustment count mismatch");
                for (const auto& n : values) modifier.values.push_back(number(n,-100,100));
                modifier.positive_only.assign(r.abilities.size(),false);
                if (const auto* positive = age.if_contains("positiveOnly")) for (const auto& name : ids(*positive)) {
                    const auto& ability = find(r.abilities,name);
                    modifier.positive_only[static_cast<std::size_t>(&ability-r.abilities.data())] = true;
                }
                if (const auto* wrap = age.if_contains("wrapByte")) {
                    require(wrap->is_bool(),"Invalid age wrap policy"); modifier.wrap_byte = wrap->as_bool();
                }
                race.age_adjustments.push_back(std::move(modifier));
            }
        }
        if (const auto* value = o.if_contains("startingAge")) {
            const auto& ages = j::object(*value,"starting age");
            require(ages.size() == race.classes.size(),"Starting age class count mismatch");
            for (const auto& c : race.classes) {
                const auto& dice = j::object(j::field(ages,c,"starting age"),"starting age dice");
                j::fields_only(dice,{"base","count","sides"},"starting age dice");
                StartingAgeDice age{number(j::field(dice,"base","age"),0,65535),
                    number(j::field(dice,"count","age"),1,10),number(j::field(dice,"sides","age"),2,100)};
                require(age.base+age.count*age.sides <= 65535,"Starting age overflows uint16");
                race.starting_ages.emplace(c,age);
            }
        }
        const auto& maxima = j::object(j::field(o,"maximum","race"),"maximum");
        const auto& percentile = j::object(j::field(o,"percentileMaximum","race"),"percentile maximum");
        require(maxima.size() == r.genders.size() && percentile.size() == r.genders.size(),"Gender limits mismatch");
        for (const auto& g : r.genders) {
            auto maximum = scores(j::field(maxima,g.id,"maximum"),r.abilities.size());
            for (std::size_t i = 0; i < maximum.size(); ++i)
                require(race.minimum[i] <= maximum[i],"Conflicting ability limits");
            race.maximum.emplace(g.id,std::move(maximum));
            race.percentile_maximum.emplace(g.id,number(j::field(percentile,g.id,"percentile maximum"),0,100));
        }
        r.races.push_back(std::move(race));
    }
    unique(r.races); return r;
}
template<class Random>
std::optional<std::uint16_t> roll_starting_age_impl(const CreationRules& rules, std::string_view race,
    std::string_view character_class, Random& random) {
    const auto& choice = find(rules.races,race);
    (void)find(rules.classes,character_class);
    require(std::find(choice.classes.begin(),choice.classes.end(),character_class) != choice.classes.end(),
        "Race cannot use class");
    if (choice.starting_ages.empty()) return std::nullopt;
    const auto it = choice.starting_ages.find(std::string(character_class));
    require(it != choice.starting_ages.end(),"Starting age unavailable for class");
    const auto& dice = it->second;
    require(dice.base >= 0 && dice.count >= 1 && dice.count <= 10 && dice.sides >= 2 && dice.sides <= 100 &&
        dice.base <= 65535-dice.count*dice.sides,"Invalid starting age dice");
    int age = dice.base;
    for (int i = 0; i < dice.count; ++i) age += die(random,dice.sides);
    return static_cast<std::uint16_t>(age);
}
std::optional<std::uint16_t> roll_starting_age(const CreationRules& rules, std::string_view race,
    std::string_view character_class, std::mt19937& random) {
    return roll_starting_age_impl(rules,race,character_class,random);
}
std::optional<std::uint16_t> roll_starting_age(const CreationRules& rules, std::string_view race,
    std::string_view character_class, HighWordLcg32& random) {
    return roll_starting_age_impl(rules,race,character_class,random);
}
namespace {
std::vector<std::size_t> class_choices(const CreationRules& rules, const Character& c, bool creation) {
    std::vector<std::size_t> result;
    if (!race_scores(rules,c)) return result;
    const auto& race = find(rules.races,c.race);
    for (std::size_t i = 0; i < rules.classes.size(); ++i) {
        const auto& choice = rules.classes[i];
        if (std::find(race.classes.begin(),race.classes.end(),choice.id) == race.classes.end()) continue;
        bool eligible = true;
        if (creation && !choice.creation_minimum.empty())
            require(choice.creation_minimum.size() == c.abilities.size(),"Class creation minimum count mismatch");
        for (std::size_t j = 0; j < c.abilities.size(); ++j) {
            const auto floor = creation && !choice.creation_minimum.empty() ? choice.creation_minimum[j] : 0;
            require(floor >= 0 && floor <= 1000,"Class creation minimum out of range");
            const auto value = std::max(c.abilities[j].value,floor);
            if (value < choice.minimum[j] || value > race.maximum.at(c.gender)[j]) eligible = false;
        }
        if (eligible) result.push_back(i);
    }
    return result;
}
}
std::vector<std::size_t> eligible_classes(const CreationRules& rules, const Character& c) {
    return class_choices(rules,c,false);
}
std::vector<std::size_t> creation_classes(const CreationRules& rules, const Character& c) {
    return class_choices(rules,c,true);
}
template<class Random>
std::vector<AbilityValue> roll_abilities_impl(const CreationRules& rules, std::string_view race,
    std::string_view gender, Random& random, const Character* context = nullptr) {
    require(rules.dice_count >= 1 && rules.dice_count <= 10 && rules.dice_sides >= 2 && rules.dice_sides <= 100 &&
        rules.drop_lowest >= 0 && rules.drop_lowest < rules.dice_count,"Invalid ability dice");
    require(rules.keep_best_of >= 1 && rules.keep_best_of <= 16,"Invalid ability trial count");
    require(rules.bounds == CreationBounds::reject || rules.bounds == CreationBounds::clamp,
        "Unknown ability bounds policy");
    Character c = context ? *context : Character{}; c.race = race; c.gender = gender;
    const auto& race_rule = find(rules.races,race);
    const auto& adjustments = race_rule.adjustments;
    (void)find(rules.genders,gender);
    require(adjustments.empty() || adjustments.size() == rules.abilities.size(),"Racial adjustment count mismatch");
    for (const auto n : adjustments) require(n >= -100 && n <= 100,"Racial adjustment out of range");
    for (int attempt = 0; attempt < 256; ++attempt) {
        c.abilities.clear();
        for (std::size_t i = 0; i < rules.abilities.size(); ++i) {
            int best = 0;
            for (int trial = 0; trial < rules.keep_best_of; ++trial) {
                std::vector<int> dice;
                for (int j = 0; j < rules.dice_count; ++j) dice.push_back(die(random,rules.dice_sides));
                std::sort(dice.begin(),dice.end());
                int score = 0;
                for (int j = rules.drop_lowest; j < rules.dice_count; ++j) score += dice[j];
                best = std::max(best,score);
            }
            // A fixed per-ability modifier commutes with keeping the best trial.
            auto value = best+(adjustments.empty() ? 0 : adjustments[i]);
            if (context && c.age) for (const auto& modifier : race_rule.age_adjustments) {
                require(modifier.minimum >= 1 && modifier.maximum >= modifier.minimum && modifier.maximum <= 65535 &&
                    modifier.values.size() == rules.abilities.size() &&
                    modifier.positive_only.size() == rules.abilities.size(),"Invalid age adjustment");
                if (*c.age < modifier.minimum || *c.age > modifier.maximum ||
                    (modifier.positive_only[i] && value <= 0)) continue;
                require(modifier.values[i] >= -100 && modifier.values[i] <= 100,"Age adjustment out of range");
                value += modifier.values[i];
                if (modifier.wrap_byte) value = static_cast<std::uint8_t>(value);
            }
            if (rules.bounds == CreationBounds::clamp) {
                const auto low = race_rule.minimum.at(i), high = race_rule.maximum.at(c.gender).at(i);
                require(low <= high,"Conflicting ability limits");
                value = std::clamp(value,low,high);
            }
            AbilityValue ability{value,0};
            if (context) {
                const auto& choice = find(rules.classes,c.character_class);
                if (!choice.creation_minimum.empty()) {
                    require(choice.creation_minimum.size() == rules.abilities.size(),"Class floor count mismatch");
                    ability.value = std::max(ability.value,choice.creation_minimum[i]);
                }
                if (choice.percentile && rules.percentile_ability == i && ability.value == rules.percentile_at) {
                    const auto maximum = race_rule.percentile_maximum.at(c.gender);
                    if (maximum) ability.percentile = std::min(die(random,
                        rules.percentile_roll_maximum.value_or(maximum)),maximum);
                }
            }
            c.abilities.push_back(ability);
        }
        if (context || !creation_classes(rules,c).empty()) return c.abilities;
    }
    throw std::runtime_error("No eligible ability roll within 256 attempts");
}
template<class Random>
Character roll_character_impl(const CreationRules& rules, Character draft, Random& random) {
    auto candidate = random;
    const auto& race = find(rules.races,draft.race);
    const auto& choice = find(rules.classes,draft.character_class);
    const auto selected = static_cast<std::size_t>(&choice-rules.classes.data());
    auto preview = draft; preview.abilities.clear();
    for (const auto n : race.maximum.at(draft.gender)) preview.abilities.push_back({n,0});
    const auto available = creation_classes(rules,preview);
    require(std::find(available.begin(),available.end(),selected) != available.end(),"Unavailable creation class");
    require(!rules.percentile_roll_maximum ||
        (*rules.percentile_roll_maximum >= 1 && *rules.percentile_roll_maximum <= 100),"Invalid percentile dice");
    if (!draft.age) {
        require(rules.initial_random_bounds.size() <= 16,"Too many initial random draws");
        require(rules.initial_random_bounds.empty() || !race.starting_ages.empty(),
            "Initial random draws require starting age configuration");
        for (const auto bound : rules.initial_random_bounds) discard_draw(candidate,bound);
        draft.age = roll_starting_age_impl(rules,draft.race,draft.character_class,candidate);
    }
    require(race.age_adjustments.empty() || draft.age.has_value(),"Age state unavailable");
    require(!draft.age || *draft.age > 0,"Invalid character age");
    draft.abilities = roll_abilities_impl(rules,draft.race,draft.gender,candidate,&draft);
    if (choice.starting_money) {
        const auto& money = *choice.starting_money;
        require(money.count >= 1 && money.count <= 10 && money.sides >= 2 && money.sides <= 100 &&
            money.multiplier >= 1 && money.multiplier <= 65535 &&
            money.count*money.sides <= 65535/money.multiplier,"Invalid starting money dice");
        (void)id(bj::value(money.currency));
        int balance = 0;
        for (int i = 0; i < money.count; ++i) balance += die(candidate,money.sides);
        draft.money[money.currency] = static_cast<std::uint16_t>(balance*money.multiplier);
    }
    if (choice.hit_points) {
        const auto& hp = *choice.hit_points;
        require(hp.count >= 1 && hp.count <= 10 && hp.sides >= 2 && hp.sides <= 100 &&
            hp.minimum >= 1 && hp.minimum <= hp.count*hp.sides && hp.ability < draft.abilities.size(),
            "Invalid hit point dice");
        const int index = draft.abilities[hp.ability].value-hp.ability_minimum;
        require(index >= 0 && static_cast<std::size_t>(index) < hp.modifiers.size(),"Missing hit point modifier");
        require(hp.modifiers[index] >= -100 && hp.modifiers[index] <= 100,"Invalid hit point modifier");
        int base = 0;
        for (int i = 0; i < hp.count; ++i) base += die(candidate,hp.sides);
        base = std::max(base,hp.minimum);
        const auto maximum = std::max(1,base+hp.modifiers[index]);
        draft.hit_points = HitPoints{base,maximum,maximum};
    }
    draft.status = "okay";
    random = candidate;
    return draft;
}
Character roll_character(const CreationRules& rules, Character draft, std::mt19937& random) {
    return roll_character_impl(rules,std::move(draft),random);
}
Character roll_character(const CreationRules& rules, Character draft, HighWordLcg32& random) {
    return roll_character_impl(rules,std::move(draft),random);
}
void validate_character(const CreationRules& rules, const Character& c) {
    require(!c.age || *c.age > 0,"Invalid character age");
    if (c.hit_points) {
        const auto& hp = *c.hit_points;
        require(hp.base >= 1 && hp.base <= 65535 && hp.maximum >= 1 && hp.maximum <= 65535 &&
            hp.current >= 0 && hp.current <= hp.maximum,"Invalid character hit points");
    }
    require(c.money.size() <= 64,"Too many character currencies");
    for (const auto& [currency,balance] : c.money) { (void)id(bj::value(currency)); (void)balance; }
    if (c.status) (void)id(bj::value(*c.status));
    require(!c.name.empty() && c.name.size() <= rules.name_limit && c.name.front() != ' ' && c.name.back() != ' ' &&
        std::all_of(c.name.begin(),c.name.end(),[](unsigned char b) { return b >= 32 && b < 127; }),"Invalid character name");
    const auto eligible = eligible_classes(rules,c); const auto& choice = find(rules.classes,c.character_class);
    const auto index = static_cast<std::size_t>(&choice-rules.classes.data());
    require(std::find(eligible.begin(),eligible.end(),index) != eligible.end(),"Character does not qualify for class");
    require(std::find(choice.alignments.begin(),choice.alignments.end(),c.alignment) != choice.alignments.end(),
        "Invalid character alignment");
    for (std::size_t i = 0; i < c.abilities.size(); ++i) {
        const bool exceptional = choice.percentile && rules.percentile_ability == i &&
            c.abilities[i].value == rules.percentile_at;
        const auto maximum = exceptional ? find(rules.races,c.race).percentile_maximum.at(c.gender) : 0;
        require(c.abilities[i].percentile >= (maximum ? 1 : 0) && c.abilities[i].percentile <= maximum,
            "Invalid percentile ability");
    }
}
template<class Random>
Character finish_character_impl(const CreationRules& rules, Character draft, Random& random) {
    draft.status = "okay";
    (void)find(rules.genders,draft.gender);
    const auto& choice = find(rules.classes,draft.character_class);
    if (!choice.creation_minimum.empty()) {
        const auto available = creation_classes(rules,draft);
        const auto selected = static_cast<std::size_t>(&choice-rules.classes.data());
        require(std::find(available.begin(),available.end(),selected) != available.end(),
            "Character cannot use class creation minimums");
        for (std::size_t i = 0; i < draft.abilities.size(); ++i)
            draft.abilities[i].value = std::max(draft.abilities[i].value,choice.creation_minimum[i]);
    }
    for (auto& score : draft.abilities) score.percentile = 0;
    if (choice.percentile && rules.percentile_ability &&
        *rules.percentile_ability < draft.abilities.size() &&
        draft.abilities[*rules.percentile_ability].value == rules.percentile_at) {
        const auto maximum = find(rules.races,draft.race).percentile_maximum.at(draft.gender);
        if (maximum) {
            const auto sides = rules.percentile_roll_maximum.value_or(maximum);
            require(sides >= 1 && sides <= 100,"Invalid percentile roll maximum");
            draft.abilities[*rules.percentile_ability].percentile = std::min(die(random,sides),maximum);
        }
    }
    validate_character(rules,draft); return draft;
}
std::vector<AbilityValue> roll_abilities(const CreationRules& rules, std::string_view race,
    std::string_view gender, std::mt19937& random) { return roll_abilities_impl(rules,race,gender,random); }
std::vector<AbilityValue> roll_abilities(const CreationRules& rules, std::string_view race,
    std::string_view gender, HighWordLcg32& random) { return roll_abilities_impl(rules,race,gender,random); }
Character finish_character(const CreationRules& rules, Character draft, std::mt19937& random) {
    return finish_character_impl(rules,std::move(draft),random);
}
Character finish_character(const CreationRules& rules, Character draft, HighWordLcg32& random) {
    return finish_character_impl(rules,std::move(draft),random);
}
void add_character(Party& party, const CreationRules& rules, Character character) {
    require(party.size() < rules.party_limit,"Party is full"); validate_character(rules,character);
    for (const auto& c : party) require(folded(c.name) != folded(character.name),"Character name already in party");
    party.push_back(std::move(character));
}
std::string serialize_party(const Party& party, const CreationRules& rules) {
    Party validated; bj::array members;
    for (const auto& c : party) {
        add_character(validated,rules,c); bj::array abilities;
        for (const auto& a : c.abilities) abilities.push_back(bj::object{{"value",a.value},{"percentile",a.percentile}});
        bj::object member{{"name",c.name},{"race",c.race},{"gender",c.gender},
            {"class",c.character_class},{"alignment",c.alignment},{"abilities",std::move(abilities)}};
        if (c.status) member["status"] = *c.status;
        if (c.age) member["age"] = *c.age;
        if (c.hit_points) member["hitPoints"] = bj::object{{"base",c.hit_points->base},
            {"current",c.hit_points->current},{"maximum",c.hit_points->maximum}};
        if (!c.money.empty()) {
            bj::object money;
            for (const auto& [currency,balance] : c.money) money[currency] = balance;
            member["money"] = std::move(money);
        }
        members.push_back(std::move(member));
    }
    return bj::serialize(bj::object{{"rulesId",rules.id},{"characters",std::move(members)}});
}
Party read_party(std::string_view json, const CreationRules& rules) {
    require(json.size() <= 64*1024,"Saved party too large"); const auto parsed = bj::parse(json);
    const auto& root = j::object(parsed,"party"); j::fields_only(root,{"rulesId","characters"},"party");
    require(j::string(j::field(root,"rulesId","party"),"rules ID") == rules.id,"Saved party rules mismatch");
    const auto& members = j::array(j::field(root,"characters","party"),"party characters");
    require(members.size() <= rules.party_limit,"Saved party is full"); Party result;
    for (const auto& v : members) {
        const auto& o = j::object(v,"character");
        j::fields_only(o,{"name","race","gender","class","alignment","abilities","status","age",
            "hitPoints","money"},"character");
        const auto s = [&](const char* key) { return j::string(j::field(o,key,"character"),key); };
        Character c{s("name"),s("race"),s("gender"),s("class"),s("alignment"),{}};
        if (const auto* value = o.if_contains("status")) c.status = id(*value);
        if (const auto* value = o.if_contains("age")) c.age = static_cast<std::uint16_t>(number(*value,1,65535));
        if (const auto* value = o.if_contains("hitPoints")) {
            const auto& hp = j::object(*value,"saved hit points");
            j::fields_only(hp,{"base","current","maximum"},"saved hit points");
            c.hit_points = HitPoints{number(j::field(hp,"base","hit points"),1,65535),
                number(j::field(hp,"current","hit points"),0,65535),
                number(j::field(hp,"maximum","hit points"),1,65535)};
        }
        if (const auto* value = o.if_contains("money")) {
            const auto& money = j::object(*value,"saved money"); require(money.size() <= 64,"Too many currencies");
            for (const auto& [currency,balance] : money)
                c.money.emplace(id(bj::value(currency)),static_cast<std::uint16_t>(number(balance,0,65535)));
        }
        const auto& abilities = j::array(j::field(o,"abilities","character"),"abilities");
        require(abilities.size() == rules.abilities.size(),"Saved ability count mismatch");
        for (const auto& value : abilities) {
            const auto& a = j::object(value,"ability"); j::fields_only(a,{"value","percentile"},"ability");
            c.abilities.push_back({number(j::field(a,"value","ability"),1,1000),
                number(j::field(a,"percentile","ability"),0,100)});
        }
        add_character(result,rules,std::move(c));
    }
    return result;
}
} // namespace goldbox

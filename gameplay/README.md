# Gameplay layer (planned)

- `exploration/`: first-person grid exploration, mapping, towns/wilderness.
- `events/`: normalized event model + SSI ECL-compatible interpreter.
- `combat/`: isometric tactical combat, initiative and actions.
- `rules/`: AD&D rules variations isolated from event data.

The bounded ECL VM and terminal exploration pilot currently live in
`engine/include/goldbox/ecl_vm.hpp`, `engine/include/goldbox/exploration.hpp`
and their matching sources. `apps/explore.cpp` drives an explicit imported
map/ECL pair with local save/restore. Full campaign startup, rules, combat,
doors and map transitions remain future work. See the main README and
`docs/research/local-validation-2026-10-15.md` for the narrow original-resource
validation scope.

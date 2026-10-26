# Pool native opcode and address map

Native map: 2026-10-25. Engine coverage includes the subsequent monster setup preparation increment.

All 256 opcode-byte values were exercised through the installed Pool native
dispatcher. 61 select 49 distinct handlers; 195 have no handler entry in this dispatcher.
The table below maps every selected handler, its file location, reference operand
layout and current engine path. This is an address/coverage catalog, not a claim
that every mapped handler's semantics have been verified or implemented.

## Release identity and method

This map applies only to the authorized installed DOS release with:

- `GAME.OVR` SHA-256:
  `872964a32cef0fc32bbf83bab68b15e04ad36d59076f37b00a179dfee7928614`.
- Reference load-image SHA-256 (68,592 bytes):
  `a42a57e68d3f7815831821cc9365bb917befda45a58e958544451fc7a878df8e`.
- Interpreter overlay ID `0x1F`, raw `GAME.OVR` base `0x0D46`, size `0x350D`.
- Dispatcher entry `1F:2F2D`; ordinary dispatcher return `1F:3170`.

The bounded private probe loads the unchanged native dispatcher, supplies each
opcode in its native opcode register, and stops immediately before the selected
handler call. For values without a selected call it observes the dispatcher
return. Limits: 5,000 native instructions / 100 ms per value, 20 seconds for the
batch. It does not execute the selected handler bodies or guess instruction
boundaries in game ECL records. Known class-check, SURPRISE and encounter handler
offsets were cross-checked against their earlier behavior probes.

Private reproducibility inputs remain outside the public repository:
`map_original_dispatch.py` and `original-opcode-dispatch-map.json` in the local
validation cache. No original instructions, assets, descriptions or captures
are included here.

Names and operand layouts are research labels from the pinned SSI Engine
[opcode metadata](https://github.com/TiCoKH/ssi-engine/blob/51d4b6d409fa798754f5ff2ccb249f3c0b6d029b/src/main/java/engine/script/EclOpCode.java)
and [Pool configuration](https://github.com/TiCoKH/ssi-engine/blob/51d4b6d409fa798754f5ff2ccb249f3c0b6d029b/src/main/resources/Pool%20of%20Radiance.properties),
cross-referenced with current C++ tables. Their presence does not establish
native operand or behavioral parity for unprobed handlers. In particular,
reference PARTY_SKILL_CHECK2 is actually the observed class-presence check here.
No upstream Java implementation or native code was adopted for this catalog.

## Address spaces

| Kind | Example | Interpretation |
| --- | --- | --- |
| Opcode ID | `0x1C` | Selects an operation; it is not an address. |
| Logical native overlay offset | `1F:125D` | Offset `0x125D` within overlay ID `0x1F`; the loaded real-mode CS is relocatable. |
| Raw file offset | `0x01FA3` | `GAME.OVR` base `0x0D46` plus handler `0x125D`. |
| ECL bytecode offset | `6319` / `0x18AF` | Offset within one ECL program after its optional record prefix. |
| ECL logical code address | `0xB1AF` | Pool code base `0x9900` plus bytecode offset `0x18AF`. |
| Decompressed DAX record offset | `6319` or `6321` | Bytecode offset plus a verified prefix length of zero or two bytes. |
| ECL logical memory address | `0x6DCB` | A script register, resolved through the game's memory adapter. |
| Native DS-relative field | `DS:49ED` | Interpreter bookkeeping in native memory; distinct from an ECL global with the same number. |

The earlier seeded replay stop at bytecode offset 6319 therefore selects opcode
`0x1C`, whose native handler is `1F:125D` / file `0x01FA3`. Those three numbers
describe different locations. Never use the raw native offset as an ECL target.

## Complete dispatcher catalog

`VM` means a C++ execution path exists, subject to ordinary unsupported operands,
host calls and memory-write restrictions. `VM / profile` additionally requires
the selected dialect/adapter. Neither means complete native parity. `Decode
only` means the provisional decoder recognizes the layout but the VM cannot
execute it. `Missing` means there is no supported runtime meaning. COMBAT has
only a host callback, with no complete runtime combat implementation.

Operand numbers in variable counts are one-based. Layouts are reference metadata,
not a bulk native arity-validation result. Shared native handler offsets are
intentional: a common routine examines the opcode to select its operation.

| Opcode | Reference label | Operands | Native handler | GAME.OVR byte offset | Engine path | Limits / evidence |
| --- | --- | --- | --- | --- | --- | --- |
| `0x00` | EXIT | 0 | `1F:0001` | `0x00D47` | VM |  |
| `0x01` | GOTO | 1 | `1F:00F4` | `0x00E3A` | VM |  |
| `0x02` | GOSUB | 1 | `1F:0111` | `0x00E57` | VM |  |
| `0x03` | COMPARE | 2 | `1F:0126` | `0x00E6C` | VM |  |
| `0x04` | ADD | 3 | `1F:01A3` | `0x00EE9` | VM |  |
| `0x05` | SUBTRACT | 3 | `1F:01A3` | `0x00EE9` | VM / profile |  |
| `0x06` | DIVIDE | 3 | `1F:01A3` | `0x00EE9` | VM / profile |  |
| `0x07` | MULTIPLY | 3 | `1F:01A3` | `0x00EE9` | VM / profile |  |
| `0x08` | RANDOM | 2 | `1F:0234` | `0x00F7A` | VM / profile |  |
| `0x09` | WRITE_MEM | 2 | `1F:0292` | `0x00FD8` | VM |  |
| `0x0A` | LOAD_CHAR | 1 | `1F:02E0` | `0x01026` | VM / profile | Read-only normalized party adapter |
| `0x0B` | LOAD_MON | 3 | `1F:0450` | `0x01196` | VM / profile | Saved preparation recipes; combat actors not decoded |
| `0x0C` | SPRITE_START3 | 3 | `1F:03B4` | `0x010FA` | VM / profile |  |
| `0x0D` | SPRITE_ADVANCE | 0 | `1F:0817` | `0x0155D` | VM / profile |  |
| `0x0E` | PICTURE | 1 | `1F:0856` | `0x0159C` | VM / profile |  |
| `0x0F` | INPUT_NUMBER | 2 | `1F:0951` | `0x01697` | Decode only |  |
| `0x10` | INPUT_STRING | 2 | `1F:0994` | `0x016DA` | Decode only |  |
| `0x11` | PRINT | 1 | `1F:0A09` | `0x0174F` | VM |  |
| `0x12` | PRINT_CLEAR | 1 | `1F:0A09` | `0x0174F` | VM |  |
| `0x13` | RETURN | 0 | `1F:0A9F` | `0x017E5` | VM |  |
| `0x14` | COMPARE_AND | 4 | `1F:0AED` | `0x01833` | VM |  |
| `0x15` | MENU_VERTICAL | 3 + count(operand 3) | `1F:0F08` | `0x01C4E` | VM |  |
| `0x16` | IF_EQUALS | 0 | `1F:0B52` | `0x01898` | VM |  |
| `0x17` | IF_NOT_EQUALS | 0 | `1F:0B52` | `0x01898` | VM |  |
| `0x18` | IF_LESS | 0 | `1F:0B52` | `0x01898` | VM |  |
| `0x19` | IF_GREATER | 0 | `1F:0B52` | `0x01898` | VM |  |
| `0x1A` | IF_LESS_EQUALS | 0 | `1F:0B52` | `0x01898` | VM |  |
| `0x1B` | IF_GREATER_EQUALS | 0 | `1F:0B52` | `0x01898` | VM |  |
| `0x1C` | CLEAR_MON | 0 | `1F:125D` | `0x01FA3` | VM / profile | Native setup reset; preserves sprite/party |
| `0x1D` | PARTY_STRENGTH | 1 | `1F:12C6` | `0x0200C` | VM / profile | Native weighted rating verified |
| `0x1E` | PARTY_CHECK | 6 | `1F:1435` | `0x0217B` | Missing |  |
| `0x20` | NEW_ECL | 1 | `1F:0BD1` | `0x01917` | VM |  |
| `0x21` | LOAD_AREA_MAP | 3 | `1F:0C74` | `0x019BA` | VM / profile |  |
| `0x22` | PARTY_SKILL_CHECK2 | 2 | `1F:1629` | `0x0236F` | VM / profile | Native class-presence outputs; reference name misleading |
| `0x23` | SURPRISE | 4 | `1F:16C9` | `0x0240F` | VM / profile | Two-roll signed-threshold adapter |
| `0x24` | COMBAT | 0 | `1F:179B` | `0x024E1` | Host boundary only | Callback only; complete combat absent |
| `0x25` | ON_GOTO | 2 + count(operand 2) | `1F:18F8` | `0x0263E` | VM |  |
| `0x26` | ON_GOSUB | 2 + count(operand 2) | `1F:18F8` | `0x0263E` | VM |  |
| `0x27` | TREASURE_MULTICOIN | 8 | `1F:19B0` | `0x026F6` | Missing |  |
| `0x28` | ROB | 3 | `1F:1DB1` | `0x02AF7` | Missing |  |
| `0x29` | ENCOUNTER_MENU | 14 | `1F:1EF8` | `0x02C3E` | VM / profile | Movement/effects provider and map-edge visibility incomplete |
| `0x2A` | COPY_MEM | 3 | `1F:0E5E` | `0x01BA4` | VM |  |
| `0x2B` | MENU_HORIZONTAL | 2 + count(operand 2) | `1F:10D1` | `0x01E17` | VM |  |
| `0x2C` | PARLAY | 6 | `1F:25CB` | `0x03311` | Missing |  |
| `0x2D` | CALL | 1 | `1F:2CCF` | `0x03A15` | VM | Only configured native-call adapters |
| `0x2E` | DAMAGE | 5 | `1F:2775` | `0x034BB` | Missing |  |
| `0x2F` | AND | 3 | `1F:0DEF` | `0x01B35` | VM |  |
| `0x30` | OR | 3 | `1F:0DEF` | `0x01B35` | Missing |  |
| `0x31` | SPRITE_OFF | 0 | `1F:2AA2` | `0x037E8` | VM / profile |  |
| `0x32` | FIND_ITEM | 1 | `1F:266A` | `0x033B0` | Missing |  |
| `0x33` | PRINT_RETURN | 0 | `1F:2AEB` | `0x03831` | Missing |  |
| `0x34` | CLOCK1 | 1 | `1F:2AC1` | `0x03807` | Missing | Clock formatting exists; advancement opcode absent |
| `0x35` | WRITE_MEM_BASE_OFF | 3 | `1F:0EBC` | `0x01C02` | Missing |  |
| `0x36` | NPC_ADD | 2 | `1F:2B7E` | `0x038C4` | Missing |  |
| `0x37` | LOAD_AREA_DECO | 3 | `1F:0C74` | `0x019BA` | VM / profile |  |
| `0x38` | PROGRAM | 1 | `1F:2E10` | `0x03B56` | Missing | Separate from NEW_ECL; semantics unverified |
| `0x39` | WHO | 1 | `1F:2B33` | `0x03879` | VM / profile |  |
| `0x3A` | DELAY | 0 | `1F:271F` | `0x03465` | VM |  |
| `0x3B` | SPELL | 3 | `1F:2C03` | `0x03949` | Missing |  |
| `0x3C` | PRINT_RUNES | 1 | `1F:2E7D` | `0x03BC3` | Missing |  |
| `0x3D` | CLEAR_BOX | 0 | `1F:2B14` | `0x0385A` | Missing |  |

The remaining byte values are also accounted for:

| Opcode values | Count | Native dispatcher observation | Engine policy |
| --- | --- | --- | --- |
| `0x1F` | 1 | No selected handler; dispatcher returns | Unsupported; reference calls it UNUSED_1F. |
| `0x3E–0xFF` | 194 | No selected handler; dispatcher returns | No meaning enabled by this observation. |

This establishes dispatch coverage only. It does not prove that the complete
native fetch/lifecycle path treats every unrouted value as a valid no-op.
Do not add permissive no-ops to the VM from this result. SSI Engine's Pool
configuration names HAS_EFFECT (`0x3F`), DESTROY_ITEM (`0x40`) and UNUSED_42
(`0x42`), and its shared enum has still more later opcodes. This installed
dispatcher routes none of those values. Other releases or preprocessing paths
require independent evidence.

Current coverage: 25 ordinary VM paths, 18 profile-selected paths, two
decode-only instructions, 15 missing instructions and one combat host boundary.
These counts describe source coverage, not a percentage of game compatibility.

## Native bookkeeping and script registers already located

These are verified narrow bindings from previous work, not a complete native
memory-layout specification. Per-game addresses stay in profiles/adapters.

| Space | Address / offset | Known role |
| --- | --- | --- |
| ECL | `0x9900` | Pool code base for the current dispatch dialect. |
| ECL | `0xC04B`, `0xC04C`, `0xC04D` | Current X, Y and normalized facing context. |
| ECL | `0xC04E`, `0xC04F` | Configured forced-step wall register and raw square/event context. |
| ECL | `0x49C7`, `0x49C8`, `0x49C9` | Clock minute units, minute tens and hours. Formatting is verified; elapsed-time advancement is unfinished. |
| ECL | `0x49E6` | Zero/nonzero view mode consumed by encounter visibility and action selection. |
| ECL | `0x49F0`, `0x49F1` | Previous X/Y movement context. |
| ECL | `0x4A00–0x4A1F`, `0x6E79–0x6E82` | Profile-selected temporary ranges cleared on program transfer. |
| ECL | `0x6B00` | Selected-character virtual record base; supported fields are individually profile-bound. |
| ECL | `0x6DB1` | Selected character index. |
| ECL | `0x6DC0`, `0x6DC1` | Requested/current encounter distance. |
| ECL | `0x6DC9`, `0x6DD5` | Movement block and tried-to-leave-map flags. |
| ECL | `0x6DCB` | Observed SURPRISE result, with second-check precedence. |
| Native DS | `0x4428` | Current 32-bit RNG state. |
| Native DS | `0x49D2`, `0x49D6` | Far pointers used by the observed 0x4900/0x6B00-based global buffers. |
| Native DS | `0x49DE`, `0x49ED`, `0x6F7F` | Code-buffer far pointer, bytecode PC and current opcode byte. |
| Native DS | `0x5D96` | Linked-party head far pointer. |
| Native character | `+0x104` | Next-character far pointer. |
| Native character | `+0x11C`, `+0x7F` | Current movement byte and active-effect-list far pointer. No equivalent current-state provider is implemented yet. |

Native word-slot buffers and normalized C++ byte-addressed memory are not
interchangeable dumps. The selected-character window also has its own field
mapping. See the [existing memory/VM documentation](../formats/ecl-v1.md) and
[behavior validation evidence](local-validation-2026-10-15.md) before binding a
new address.

## Work exposed by the map

| Related operations | Dependency to implement/verify together |
| --- | --- |
| CLEAR_MON `1C`, LOAD_MON `0B`, COMBAT `24` | Setup recipes, resource boundaries and persistence now exist. Combat-ready actors, inventory/effect instances and a real combat host remain missing. Both seeded paths reach COMBAT at bytecode offset 6330. |
| ENCOUNTER_MENU `29`, PARTY_CHECK `1E` | Current character state, movement/effects and verified map-edge visibility; menu support alone does not supply those values. |
| TREASURE `27`, ROB `28`, FIND_ITEM `32` | Inventory/currency ownership, item resources and explicit state writes/persistence. |
| PARLAY `2C`, INPUT_NUMBER `0F`, INPUT_STRING `10` | Typed host input and verified result/state semantics. |
| CLOCK `34`, indexed write `35`, OR `30`, PROGRAM `38` | Native arithmetic/address behavior, time progression and lifecycle differences; PROGRAM must not be assumed equivalent to NEW_ECL. |
| DAMAGE `2E`, NPC_ADD `36`, SPELL `3B` | Party mutations and effects, with rules-profile-specific behavior. |
| PRINT_RETURN `33`, PRINT_RUNES `3C`, CLEAR_BOX `3D` | Native presentation/input behavior and script resumption. |

This catalog intentionally does not invent offsets for every instruction inside
every ECL record. Those records mix instructions, strings and tables; a complete
record map needs traversal from verified lifecycle entries and typed branch
targets, preserving state-dependent unresolved edges. Native handler mapping
removes the one-opcode-at-a-time address discovery without pretending that a
linear byte sweep would establish game compatibility.

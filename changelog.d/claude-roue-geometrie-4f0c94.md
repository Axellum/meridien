### Added

- **Press-and-hold wheel: pure geometry core (no screen yet)**: `components/core/include/ecran/roue_geometrie.hpp` ports the
  geometry of the Tab5's wheel to Meridien. For a touch point, a screen and a ring of N buttons it gives the hub position, each
  button, each button's word box (left / right / above / below, from the button's axis), the second ring of a family (20° between
  choices, centred on the family) and the button under a finger (zone = the button's disc plus 8 px, the nearest centre wins).
  The edge "pivot" of the Tab5 is kept line for line. **Deliberate difference**: the Tab5 never opens its wheel in a corner;
  Meridien opens it where the finger lands, so when the Tab5 rule would stack buttons (corners, a six-button fan at a side edge)
  the hub slides toward the screen centre until everything fits, and `Roue::tient` says so when nothing fits. The Waveshare 7B
  uses an explicit 0.8 scale on 1024 x 600. No content, colour or text: buttons are numbers, a word's width is a number the
  caller measured. Tested on: PC only (reference values produced by the Tab5's own `disposer()` compiled apart, plus a
  hand-written trigonometry cross-check; invariants swept over the whole screen with AddressSanitizer and UBSan; targeted
  mutations of the radius, the angle step, the edge pivot and the rounding, all detected). Not compiled for a board (nothing in
  `main/` uses it yet), **not flashed and not run on any board**, never seen on a screen.
  Details: [docs/decisions/0017-roue-d-appui-long-geometrie-pure.md](../docs/decisions/0017-roue-d-appui-long-geometrie-pure.md).

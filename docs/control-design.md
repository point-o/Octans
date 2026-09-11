# Capture control design

The circular handle remains the visual anchor and drag affordance. Its lens mark
represents Original; P, D, or T identifies an active simulation without relying on
color. The simulation picker becomes a single rounded panel, with an explicit
Original option and named modes rather than isolated letter-only targets.

## Accessibility acceptance criteria

This control pass uses [WCAG 2.2](https://www.w3.org/TR/WCAG22/) and
[WCAG2ICT guidance for native software](https://www.w3.org/TR/wcag2ict-22/) as
engineering references. It is not an ADA conformance certification.

- At least 44 logical pixels for each picker row's height and handle target.
- Full accessible names and native checkable button state for selected modes.
- Selection indicated by text/shape as well as color; visible keyboard focus.
- Arrow, Home/End, Tab, Space/Enter navigation and activation; Escape dismisses
  without changing modes and returns focus to the handle.
- Font-relative panel layout checked at 12 and 24 point application text.
- Windows contrast themes use system palette colors in the redesigned controls.
- No movement or opacity animation, so reduced-motion users receive the same UI.

## Verification scope

`tests/picker.pro` exercises target geometry, accessible checked states, keyboard
selection/dismissal, and enlarged text. It renders normal and enlarged panel
images for visual inspection. Existing capture-flow checks cover movement,
resizing, and native capture behavior around the redesigned picker.

Manual release verification still includes Narrator announcements, Windows
contrast themes, mixed-DPI monitor edges, and end-to-end keyboard-only operation.
Passing automated checks does not establish app-wide accessibility conformance.

## Results for this pass

- Release application build passed.
- Picker tests: five passed on both offscreen and native Windows platforms.
- Capture-flow checks passed offscreen and on native Windows.
- Normal and 200% application-font panel renders inspected without clipped labels.
- Default palette contrast calculations: body text/panel 16.09:1, secondary text
  on hover/selection 6.93:1, yellow selection accent on hover/selection 8.88:1.
- Fixed a pre-existing smoke-test iterator lifetime error exposed by this build.

The native tests verify focus return and Qt accessibility states; Narrator speech
and real Windows contrast themes remain manual checks.

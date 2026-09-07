# Live-view and edge-analysis delivery plan

## Objective

Make the live color-vision preview more responsive while preserving bounded,
reusable image storage; support resizing and uninterrupted updates during movement;
establish a testable edge-contrast analysis baseline; document the resulting app.

## Sequence and ownership

| Stage | Owner | Scope | Dependency / acceptance gate |
| --- | --- | --- | --- |
| 1. Capture pipeline | Capture-performance agent | Frame pacing, reusable image buffers, backpressure, ownership safety | Preserve the worker's public interface; never mutate a frame still held by a consumer; measure throughput rather than promising 60 FPS |
| 2. Live-view interaction | Live-view agent | Updates during movement, resize controls, keyboard support, lifecycle tests | Can be developed alongside stage 1; integrate against its frame-lifetime contract; viewing area stays click-through |
| 3. Edge baseline | Edge-analysis agent | Standalone luminance-edge and local-contrast API, synthetic tests, cost measurements | Independent development; leave out of live capture until correctness and processing cost are understood |
| 4. Integration and documentation | Primary agent / PM | Build, regression checks, README, test instructions, limitations | Review stages 1–3 together; record actual validation and unresolved manual checks |

## Design boundaries

- Keep color simulation in place on exclusively owned frames. Capture storage may
  be recreated when the requested image dimensions change.
- Bound the worker's pool and queued frame count. A slow consumer should apply
  backpressure rather than cause growing memory use.
- A frame sampled just before the view moves may briefly be displayed at its new
  position. Rejecting every such frame causes starvation during continuous motion.
- Use an interactive control surface for resizing, preserving the transparent
  input behavior of the image surface.
- Edge analysis is a baseline measurement module, not a contrast compliance
  verdict. Define color-space, alpha, edge sampling, and output semantics.

## Verification

1. Existing color-transform tests: known colors, neutral values, alpha, input
   validation, storage reuse, and shared-image isolation.
2. Worker checks: frame ownership, retained frames, bounded allocation behavior,
   pacing, and interruption.
3. Capture-flow checks: movement, resize limits and controls, simulation picker,
   input transparency, cancellation, and clean shutdown.
4. Edge checks: uniform fields, known contrast boundaries, orientations, unsupported
   inputs, and repeat-use behavior; benchmark representative image sizes.
5. Build the complete application and update documentation to match implementation.

Mixed-DPI monitor transitions, HDR/color-managed content, and GPU-rendered desktop
content remain explicit native/manual validation items.

## Follow-on decision

After baseline measurements, choose the edge-analysis cadence and sampling
resolution before connecting it to live capture. That integration and any edge
overlay or contrast-report UI are separate work from this baseline.

## Delivery status

Implemented and integrated in the working tree:

- Capture worker: 60 FPS pacing target, three reusable image slots, one pending
  frame, retained-image safety, and wakeable waits. One GDI-to-image copy remains;
  simulation transforms that owned image in place.
- Live view: accepts completed frames during movement and resizing; bottom-right
  mouse/keyboard resize grip; consistent 88 x 72 minimum selection/view size.
- Edge analysis: reusable independent analyzer, synthetic tests and CPU benchmarks;
  compiled into the project but not invoked by live capture.
- Documentation: root README, expanded test instructions, edge-model description.

Validation completed with Qt 6.10.2 / MinGW GCC 13.1 on Windows:

- Full release application build passed.
- Pixel-transform tests: 7 passed, 0 failed.
- Edge-analysis tests: 9 passed, 0 failed (including benchmark rows).
- Desktop worker tests: 4 passed, 0 failed after granting desktop access. The
  initial sandboxed run could not read desktop pixels.
- Capture-flow smoke checks passed both offscreen and on the native Windows
  desktop, including live color simulation and input transparency.

Measured CPU copy-plus-simulation cost was approximately 2.1–2.5 ms at 640 x 480
and 12.8–14.4 ms at 1080p. Edge analysis alone measured approximately 5 ms at 720p
and 14–15 ms at 1080p. These are local release-build microbenchmarks, not sustained
live FPS or before/after speedup measurements. The results support scheduling
future edge analysis separately and at a lower cadence.

Remaining validation: sustained native drag/resize responsiveness, mixed-DPI
monitor transitions, and end-to-end FPS on representative target hardware.

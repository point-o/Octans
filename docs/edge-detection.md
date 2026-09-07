# Edge contrast analysis and Hazard Mode

`EdgeDetection::RegionAnalyzer` groups connected chroma boundaries into regions
and measures the luminance contrast between the two sides of each boundary. It
feeds **Hazard Mode** in the live capture view. A single instance runs on the
capture worker; the dense pixel analyzer that preceded it has been removed.

## API and storage

Call `analyze(const QImage&)`, then read `regions()`. Results are valid until
the next call, and `regions()` storage is reused across calls. Keep one analyzer
per worker; concurrent calls on the same analyzer are unsupported. The input is
never modified or detached.

Only RGB32 input is accepted; ARGB32 is deliberately rejected because raw median
bytes cannot also describe compositing translucent pixels. Oversized input and
unsupported formats return false, preserving the previous result. Null input
clears the result. Images with no qualifying edges yield no regions.

Each `Region` carries:

- `startPixel` / `endPixel`: inclusive bounding box in input-image coordinates.
- `color1` / `color2`: raw channel-wise median RGB bytes of the two edge sides.
- `contrastRatio`: WCAG-style `(L1 + 0.05) / (L2 + 0.05)` on the two medians.
- `indeterminate`: true when neither side has a solid color anchor (see below).

## Metric

1. Mark a pixel as an edge when any RGB channel differs from its right or below
   neighbor by at least `MinimumChannelDifference` (12). The marking direction
   (horizontal or vertical) records which neighbor pair the edge runs between.
2. Flood-fill 8-connected edge pixels into regions, dropping regions smaller than
   `MinimumEdgePixels` (4).
3. For each edge pair, take the lexicographically smaller RGB triple as side A
   and the larger as side B, then take the channel-wise median of each side in
   raw byte space (no averaging, no linearization before the median).
4. Contrast uses an sRGB linear-luminance LUT that follows the WCAG math's 0.03928
   breakpoint, so reported ratios match standard contrast calculations.
5. Reliability: each side's pixels are binned by quantized linear luminance
   (64 bins). If neither side's most common bin reaches `MinimumDominantShare`
   (0.45) of that side, the boundary is blend-only — thin antialiased text or a
   gradient — and neither median describes a real surface, so the region is
   marked `indeterminate`.

## Thresholds and presentation

The alarm threshold is a UI setting (`CapturePreview`'s `hazardThreshold`), not a
worker concern; the worker always computes raw ratios. Presets:

- **2:1** (default): heuristic floor for "can you still tell these apart at all".
- **3:1**: WCAG graphics and large-text program.
- **4.5:1**: WCAG normal-text program.

`EdgeDetection::severityFor()` maps a ratio plus the chosen threshold to a
severity: critical below half the threshold, warn below the threshold, marginal
below 3:1, hidden otherwise; `indeterminate` overrides to its own severity.
Outlines draw a black underlay plus a colored core so geometry reads for every
color-vision type; indeterminate boundaries use white/black dashes.

These are local image measurements, **not WCAG compliance results**. A screenshot
does not establish text semantics, font sizing, or intended foreground and
background colors. Thin antialiased text is the specific case the ratio cannot
vouch for; that is what the dashed style exists to say.

## Integration and cadence

On each capture-frame pass in `DesktopCapture::run()`, when Hazard Mode is on and
the analysis cadence (at most ten analyses per second) is due, the transformed
frame is downsampled by nearest-neighbor sampling to no more than 640 x 360, run
through the analyzer, and region bounds are mapped back to full captured-image
coordinates. Nearest sampling preserves exact transformed bytes; filtering would
invent blend colors before the comparison. When the region, simulation mode, or
analysis revision changes, stale annotations are dropped so outlines never apply
to a different frame than the one displayed.

Frames smaller than the reduction target skip reduction. Reductions can erase
tiny features and regular single-pixel patterns; warnings can lag changing
content between passes, and blurred edges underestimate the contrast of the wider
regions on either side.

## Validation

`tests/edgedetection.pro` covers flat fields, edge detection, orientations,
medians, the channel-difference boundary, padded rows, unsupported formats,
region limits and buffer reuse, plus reliability classification (solid,
blend-only, and narrow-core cases) and the severity classifier. A benchmark image
of 640 x 360 checkered boundaries measures roughly 1.8 ms per analysis on the
development machine (Qt 6.10.2, MinGW GCC 13.1, release), well within the ten
per-second budget. Run the benchmark again on target hardware.

Native capture and end-to-end Hazard Mode behavior require an interactive Windows
desktop session; see the test instructions.
# Edge contrast baseline

`EdgeDetection::Analyzer` is an independent CPU prototype. It is not connected to
live capture or displayed in the interface yet. Keeping it separate allows its
cost and interpretation to be checked before adding work to every capture frame.

## API and storage

Call `analyze(const QImage&)`, then read `size()` and the row-major `samples()`.
Each pixel has a floating-point `strength` and `contrast`. Output references are
valid until the next call. Keep one analyzer per worker; concurrent calls on the
same analyzer are unsupported. The input is never modified or detached.

Supported inputs are RGB32 and straight-alpha ARGB32, interpreted as 8-bit sRGB.
ARGB32 is composited onto opaque white in linear light. This is an explicit
baseline background assumption, not a reconstruction of arbitrary desktop
compositing. Premultiplied alpha and other formats are rejected without changing
the previous result. Null input clears the result. Images smaller than three
pixels on either axis return neutral samples.

The analyzer reuses vector storage across calls. It uses eight bytes per output
pixel and three float luminance rows (approximately 15.84 MiB plus 22.5 KiB at
1920 x 1080). Storage capacity follows the largest previously processed image
until the analyzer is destroyed. Resizing can allocate; steady dimensions reuse
buffers. No separate full-frame luminance copy or per-edge allocations are made.

## Metric

1. Decode sRGB with its piecewise transfer function and calculate linear relative
   luminance: `0.2126 R + 0.7152 G + 0.0722 B`.
2. Compute central differences between the left/right and above/below neighbors.
   Strength is `sqrt(dx*dx + dy*dy) / sqrt(2)`, in the range zero to one.
3. Select left/right when `abs(dx) >= abs(dy)`, otherwise above/below. Contrast is
   `(higher luminance + 0.05) / (lower luminance + 0.05)` for those two neighbors.

The outermost border has strength zero and contrast one. No threshold, edge
thinning, smoothing, text recognition, or foreground/background segmentation is
applied. A vertical or horizontal black/white step gives strength about 0.7071
on its two adjacent columns or rows and contrast 21. A diagonal step can give
strength one. Strength is therefore orientation-dependent. Thin features,
noise, antialiasing and alternating single-pixel patterns can yield misleading
or absent gradients. Opposing samples are only two pixels apart, so blurred
edges can underestimate the contrast of the larger regions on either side.

These are local image measurements, **not WCAG compliance results**. A screenshot
does not establish text semantics, font sizing, or intended foreground and
background colors. The shared form of a luminance ratio does not make an edge
sample a valid accessibility pass/fail judgment.

## Validation and next integration

Build `tests/edgedetection.pro` with Qt GUI and Qt Test. The synthetic tests cover
uniform and tiny images, horizontal/vertical/diagonal steps, independent sRGB gray
luminance, alpha compositing, padded rows, unsupported formats, and buffer reuse.
Qt Test benchmarks cover warmed 1280 x 720 and 1920 x 1080 RGB32 images.

On the development machine with Qt 6.10.2, MinGW GCC 13.1 and an optimized build,
an initial ten-iteration run measured approximately 4.7 ms at 720p and 15.0 ms at
1080p. These are indicative CPU-only measurements, not sustained capture FPS or
a performance guarantee. Run the benchmarks again on the target hardware.

Integrate only after capture/transform timing is stable. Run the analyzer on a
worker with bounded latest-frame scheduling, initially at a lower analysis rate
than display refresh. Analyze the original captured frame for source contrast;
analyze the transformed frame for a separate simulation comparison. Label those
results clearly and reuse one analyzer per stream. Define the noise threshold,
sampling scale and presentation policy before adding an overlay or pass/fail
language.

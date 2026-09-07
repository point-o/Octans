# Tests

Build each test project in its own directory using a terminal configured with Qt
and the matching compiler. These examples use Windows MinGW and a release build.

```powershell
mkdir build/pixel-tests
cd build/pixel-tests
qmake ../../tests/pixeltransform.pro CONFIG+=release
mingw32-make -j4
./release/pixeltransform-test.exe -o results.txt,txt
```

Use the same pattern with the following projects and executables:

| Project | Executable | Coverage |
| --- | --- | --- |
| `pixeltransform.pro` | `pixeltransform-test` | Known colors, neutral values, alpha, formats, shared-image isolation, in-place reuse, metadata, stride |
| `edgedetection.pro` | `edgedetection-test` | Uniform fields, known edges, orientations, luminance, alpha, formats, stride, buffer reuse, CPU benchmarks |
| `desktopcapture.pro` | `desktopcapture_test` | Windows GDI worker, acknowledgement backpressure, retained-frame safety, pool exhaustion/recovery, storage reuse, resizing |
| `captureflow.pro` | `captureflow-smoke` | Selection, launcher restoration, picker, movement, resizing, input transparency, preview close |
| `capture_pipeline_benchmark.pro` | `capture_pipeline_benchmark` | Preallocated copy-plus-transform timings, with a storage-reuse check |

The Qt Test executables support `-o results.txt,txt` to save results. The capture
smoke executable is a standalone check program: success is exit code zero; a failed
check terminates with a numbered failure. It writes `selection-test.png` and
`capture-test.png` in its working directory.

## Offscreen interaction checks

After building `captureflow.pro` in a separate build directory:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
./release/captureflow-smoke.exe
```

This checks reverse-direction selection, cancellation, accidental clicks,
keyboard selection, the P/D/T picker, control positioning and masks, keyboard and
mouse resizing, the 88 x 72 minimum size, and presentation of completed frames
after the requested capture region moves. It does not exercise real desktop
capture or prove native mouse hit-testing.

## Native Windows checks

```powershell
$env:QT_QPA_PLATFORM = 'windows'
./release/captureflow-smoke.exe
```

This briefly opens native windows and additionally verifies native input
transparency, hit-testing, capture exclusion, live updates from a colored test
window, and the actual simulation modes. Run it in an interactive desktop session.
The separate `desktopcapture_test` needs an accessible Windows desktop even though
it does not open its own windows. A `BitBlt` failure indicates capture could not
read the desktop and must not be reported as a passing worker check.

The worker permits one unacknowledged frame and reuses up to three owned images.
Tests observing signals must release retained images to allow slots to be reused.
The worker tests deliberately retain them to verify backpressure and immutability.

## Benchmarks and manual checks

Run `edgedetection-test benchmark -iterations 10 -o benchmark.txt,txt` for warmed
720p and 1080p analysis timings. These measure CPU analysis only, not live capture
FPS. See [the edge baseline](../docs/edge-detection.md) for interpretation.

Run `capture_pipeline_benchmark results.txt` to measure a preallocated copy plus
in-place simulation at 640 x 480 and 1920 x 1080, with five warmups and 30 measured
frames per case. It excludes GDI capture and UI rendering and does not compare
against an older version. Example development-machine results: about 2.1–2.5 ms
for simulated 640 x 480 frames and 12.8–14.4 ms for simulated 1080p frames.

Mixed-DPI monitor transitions, HDR/color accuracy, GPU-accelerated creative-app
content, and sustained live movement/resizing need native manual checks. The
capture worker's 60 FPS pacing target is not a measured throughput guarantee.

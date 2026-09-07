# Octans

Octans is a Windows desktop tool for inspecting screen content with color-vision
simulations. Select a region of the desktop and view it through a live,
click-through overlay while continuing to interact with the application beneath it.

The launcher uses a yellow-and-black design, constellation artwork, and the bundled
Space Grotesk font. Windows high-contrast colors are respected by the launcher.

## Use

1. Click **Capture**, then drag to select a screen region. Press **Escape** to cancel.
   You can also use arrow keys to create or move a selection, **Shift + arrow keys**
   to resize it, and **Enter** to confirm.
2. Click the overlay's handle to open the **P / D / T** simulation picker:
   Protanopia, Deuteranopia, or Tritanopia. Click the active mode again to return to
   Original. The picker supports arrow-key navigation and keyboard activation.
3. Drag the handle to move the viewing region. Arrow keys move it when the handle
   has focus.
4. Drag the bottom-right resize grip to resize the live view. When that grip has
   focus, arrow keys adjust width or height in 10-pixel steps.
5. Click **Hazard Mode**, beside the top-left handle, to outline detected edge
   regions whose contrast is below the alarm level. The default alarm is **2:1**;
   right-click the button to choose **3:1** or **4.5:1**. Click again to hide the
   warnings. Analysis uses the image after the selected color-vision simulation
   (or Original).
6. Close the capture with its **×** control to return to the launcher.

These simulations support visual inspection. They do not correct colors or predict
exactly what an individual sees. See [the color-model documentation](docs/color-simulation.md)
for matrices, color-space assumptions, and limitations.

## Build

The project uses **C++17, Qt Widgets, and qmake**. Live desktop capture uses Windows
GDI and User32. The local development setup uses Qt 6.10.2 with MinGW 64-bit.

Open `Octans.pro` in Qt Creator, select a matching desktop kit, and build/run.
Alternatively, from a terminal configured with Qt and the matching compiler:

```powershell
mkdir build/app
cd build/app
qmake ../../Octans.pro CONFIG+=release
mingw32-make -j4
./release/Octans.exe
```

Run from the configured Qt environment, or use `windeployqt` when preparing a
standalone Windows distribution. Building with another compiler requires its
matching Qt kit and build tool.

## Code map

| Files | Responsibility |
| --- | --- |
| `main.cpp`, `fonts.qrc` | Application startup and bundled font |
| `mainwindow.*` | Launcher layout, appearance, native window behavior, capture lifecycle |
| `captureflow.*` | Region selection, preview, movement, resizing, and simulation controls |
| `desktopcapture.*` | Windows desktop capture worker and frame delivery |
| `pixeltransform.*` | In-place sRGB color-vision simulation |
| `edgedetection.*` | Luminance baseline plus connected edge regions, median RGB samples, and contrast ratios |
| `tests/` | Color, edge-analysis, capture-worker, and interaction checks |

## Capture performance and edge analysis

The capture worker targets a 60 FPS cadence. Actual throughput depends on region
size, simulation mode, desktop capture cost, and the display environment. It reuses
a GDI capture buffer and at most three owned image buffers, allows one pending
frame, and applies color simulation in place. A single copy from GDI storage into
an available image keeps asynchronous readers safe; steady-size frames do not
require new image storage. Resizing may reallocate buffers.

The preview displays completed frames during movement and resizing instead of
discarding frames whose sampled position has just changed. A briefly delayed frame
can appear while the next capture catches up to the current region.

[Hazard analysis](docs/edge-detection.md) groups connected color boundaries into
regions. Each region includes inclusive `startPixel` / `endPixel` bounds, two
median RGB byte samples, their luminance contrast ratio, and a reliability flag.
Outlines are severity-graded with a dual-tone black-plus-color stroke so the
geometry reads for every color-vision type: critical (below half the alarm),
warn (below the alarm), marginal (below 3:1), and dashed "indeterminate" bands
where neither side of the boundary has a solid color to trust.

Analysis runs on the capture worker after color simulation, at most ten times per
second on a reduced image no larger than 640 x 360. Its buffers are reused, and
region bounds are mapped back to the captured image. Turning Hazard Mode off skips
this work. Warnings can lag changing content between analysis passes, and tiny
features can disappear during reduction. The warnings are local image estimates,
not a contrast-compliance verdict.

## Tests and development

See [test instructions](tests/README.md) for the separate qmake test projects and
the distinction between offscreen checks and native Windows capture checks.

The [delivery plan](docs/delivery-plan.md) describes the capture-performance,
resizing, movement, and edge-analysis work and its integration gates.

## Limitations

- Live capture currently requires Windows and successful capture exclusion for the
  preview and its controls, to avoid visual feedback.
- Desktop pixels are treated as SDR sRGB. HDR, wide-gamut profiles, and
  application-specific color management need separate validation.
- Mixed-DPI monitor placement and GPU-accelerated application content require
  manual checks.
- This tool captures pixels; it does not perform OCR or identify UI elements.

The bundled Space Grotesk font is distributed under the
[SIL Open Font License](fonts/OFL.txt).

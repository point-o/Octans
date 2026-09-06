# Capture flow checks

Build `tests/captureflow.pro` with qmake in a separate build directory, then run
`captureflow-smoke` with `QT_QPA_PLATFORM=offscreen` for selection/lifecycle checks.

Checks cover cancellation and launcher restoration, reverse-direction selection,
region placement, moving through the handle's arrow keys, accidental clicks,
keyboard selection, input-transparency flags, control placement, and preview close.
The executable exits unsuccessfully on a failed check and writes selection-test.png
and capture-test.png in its working directory for inspection.

With `QT_QPA_PLATFORM=windows`, the executable briefly opens native windows. It
also checks native no-activation/input-transparency flags and hit-testing, then
captures a colored test window beneath the preview. Changing that test window's
color verifies live updates without capturing the overlay itself.

Live capture uses a Windows GDI worker at approximately 10 FPS. Both preview
windows must accept display-affinity exclusion before the worker starts. The
worker reuses its region-sized DIB and permits only one queued image. Closing the
preview interrupts and joins the worker before destroying its native windows.

Mixed-DPI monitor placement, HDR/color accuracy, and GPU-accelerated creative-app
content still need manual checks. Pixel capture is not OCR. No accessibility
transformation has been applied to the captured image yet.

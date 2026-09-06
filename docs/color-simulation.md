# Color vision simulation

Octans starts with Original, Protanopia, Deuteranopia and Tritanopia modes.
These are visual inspection simulations, not color correction, a diagnostic
test, or a guarantee of what any particular person sees.

## Model and color space

The three 3-by-3 matrices use severity 1.0 from the precomputed sRGB table
associated with Machado, Oliveira and Fernandes, *A Physiologically-based
Model for Simulation of Color Vision Deficiency*, IEEE TVCG 15(6), 2009,
1291–1298, DOI: 10.1109/TVCG.2009.113.

- [Author-hosted corrected paper](https://profs.ic.uff.br/~laffernandes/content/publications/journal/2009_tvcg_15(6)/machado_oliveira_fernandes-tvcg-15(6)-2009-corrected.pdf)
- [Authors' matrix table and errata](https://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/CVD_Simulation.html)
- [Colour project's model documentation and tritan limitation](https://colour.readthedocs.io/en/develop/_modules/colour/blindness/machado2009.html)
- [W3C sRGB conversion reference](https://www.w3.org/TR/css-color-4/#color-conversion-code)

Input bytes are treated as standard sRGB: decode the piecewise sRGB transfer
function to linear-light RGB, multiply by the selected matrix, clamp each channel
to [0,1], and encode to sRGB. This differs from simulators that multiply encoded
RGB bytes directly. A 256-entry float decoding table and a 65,536-entry byte
encoding table remove per-pixel powers. Encoding quantization can differ by one
8-bit channel step from exact floating-point evaluation.

The full-severity tritan matrix is a blue-yellow simulation approximation;
Machado's shift model is limited for tritanopia. This first version has no
severity slider or individual calibration. Captured desktop pixels are assumed
to be SDR sRGB; monitor profiles, wide-gamut/HDR content, and creative-app color
management need dedicated validation and a future explicit capture color pipeline.

## API and memory

`PixelTransform::applyInPlace(QImage &, PixelTransform::Mode)` accepts RGB32 or
straight-alpha ARGB32. Alpha bytes and image metadata are preserved. Null images
and Original are successful no-ops. Other formats (including premultiplied alpha)
and invalid enum values return false without modifying or converting the image.
Callers must supply sRGB pixels and convert other formats explicitly if needed.

Uniquely owned frames reuse storage. Shared or read-only QImages follow Qt's
detach behavior and may allocate a copy; the worker should transform an owned
frame before publishing it. There is no full-frame temporary image. Shared
immutable tables take about 65 KiB and initialize once with C++ thread safety.
Distinct images can be transformed concurrently; simultaneous access to the
same mutable image still needs caller synchronization.

Build `tests/pixeltransform.pro` in a separate build directory and run
`pixeltransform-test`. It checks known saturated and mixed-color outputs,
all 256 neutral values and alpha values, empty/unsupported inputs, invalid modes,
shared-image isolation, in-place reuse, metadata and padded scanlines.

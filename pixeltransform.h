#ifndef PIXELTRANSFORM_H
#define PIXELTRANSFORM_H

#include <QImage>

namespace PixelTransform {

enum class Mode { Original, Protanopia, Deuteranopia, Tritanopia };

// Input is 8-bit sRGB, RGB32 or straight-alpha ARGB32. Unsupported formats and
// invalid modes return false without modification. Original and null images are
// successful no-ops. Shared images detach; uniquely owned frames reuse storage.
// Safe to call concurrently on distinct QImages; never mutate the same image
// concurrently. Alpha, image dimensions, stride and metadata are preserved.
bool applyInPlace(QImage &image, Mode mode);

} // namespace PixelTransform

#endif

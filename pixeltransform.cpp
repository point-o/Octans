#include "pixeltransform.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
using Matrix = std::array<std::array<float, 3>, 3>;

// Machado, Oliveira & Fernandes (2009), published sRGB matrices at severity 1.0.
// See docs/color-simulation.md for the model, color-space contract and sources.
constexpr Matrix protan{{
    {{0.152286f, 1.052583f, -0.204868f}},
    {{0.114503f, 0.786281f, 0.099216f}},
    {{-0.003882f, -0.048116f, 1.051998f}}
}};
constexpr Matrix deutan{{
    {{0.367322f, 0.860646f, -0.227968f}},
    {{0.280085f, 0.672501f, 0.047413f}},
    {{-0.011820f, 0.042940f, 0.968881f}}
}};
constexpr Matrix tritan{{
    {{1.255528f, -0.076749f, -0.178779f}},
    {{-0.078411f, 0.930809f, 0.147602f}},
    {{0.004733f, 0.691367f, 0.303900f}}
}};

struct TransferTables {
    std::array<float, 256> decode{};
    std::array<quint8, 65536> encode{};

    TransferTables()
    {
        for (std::size_t i = 0; i < decode.size(); ++i) {
            const double value = double(i) / 255.0;
            decode[i] = float(value <= 0.04045 ? value / 12.92
                : std::pow((value + 0.055) / 1.055, 2.4));
        }
        for (std::size_t i = 0; i < encode.size(); ++i) {
            const double value = double(i) / double(encode.size() - 1);
            const double srgb = value <= 0.0031308 ? value * 12.92
                : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
            encode[i] = quint8(std::lround(srgb * 255.0));
        }
    }

    int toSrgb(float value) const
    {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return encode[std::size_t(clamped * 65535.0f + 0.5f)];
    }
};
} // namespace

bool PixelTransform::applyInPlace(QImage &image, Mode mode)
{
    const Matrix *matrix = nullptr;
    switch (mode) {
    case Mode::Original: return true;
    case Mode::Protanopia: matrix = &protan; break;
    case Mode::Deuteranopia: matrix = &deutan; break;
    case Mode::Tritanopia: matrix = &tritan; break;
    default: return false;
    }
    if (image.isNull())
        return true;
    if (image.format() != QImage::Format_RGB32
        && image.format() != QImage::Format_ARGB32)
        return false;

    // C++11 static initialization is synchronized; tables are immutable thereafter.
    static const TransferTables tables;
    uchar *const data = image.bits();
    if (!data)
        return false;
    const auto stride = image.bytesPerLine();
    for (int y = 0; y < image.height(); ++y) {
        auto *pixels = reinterpret_cast<QRgb *>(data + qsizetype(y) * stride);
        for (int x = 0; x < image.width(); ++x) {
            const QRgb input = pixels[x];
            const float r = tables.decode[qRed(input)];
            const float g = tables.decode[qGreen(input)];
            const float b = tables.decode[qBlue(input)];
            const auto &m = *matrix;
            pixels[x] = qRgba(
                tables.toSrgb(m[0][0] * r + m[0][1] * g + m[0][2] * b),
                tables.toSrgb(m[1][0] * r + m[1][1] * g + m[1][2] * b),
                tables.toSrgb(m[2][0] * r + m[2][1] * g + m[2][2] * b),
                qAlpha(input));
        }
    }
    return true;
}

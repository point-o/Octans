#include "edgedetection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace EdgeDetection {
namespace {
const std::array<float, 256> &linearTable()
{
    static const auto table = [] {
        std::array<float, 256> values{};
        for (int i = 0; i < 256; ++i) {
            const float v = i / 255.0f;
            values[i] = v <= 0.04045f ? v / 12.92f
                                     : std::pow((v + 0.055f) / 1.055f, 2.4f);
        }
        return values;
    }();
    return table;
}
}

bool Analyzer::analyze(const QImage &image)
{
    if (!image.isNull() && image.format() != QImage::Format_RGB32
            && image.format() != QImage::Format_ARGB32)
        return false; // Keep the last valid result on unsupported input.

    m_size = image.size();
    const int width = image.width();
    const int height = image.height();
    m_samples.resize(size_t(width) * size_t(height));
    std::fill(m_samples.begin(), m_samples.end(), Sample{});
    if (width < 3 || height < 3)
        return true;

    m_rows.resize(size_t(width) * 3);
    const auto &linear = linearTable();
    const bool alpha = image.format() == QImage::Format_ARGB32;
    auto loadRow = [&](int y) {
        auto *out = m_rows.data() + size_t(y % 3) * width;
        const auto *pixels = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const QRgb p = pixels[x];
            const float luminance = 0.2126f * linear[qRed(p)]
                + 0.7152f * linear[qGreen(p)] + 0.0722f * linear[qBlue(p)];
            const float a = alpha ? qAlpha(p) / 255.0f : 1.0f;
            out[x] = a * luminance + (1.0f - a);
        }
    };
    loadRow(0);
    loadRow(1);
    for (int y = 1; y < height - 1; ++y) {
        loadRow(y + 1);
        const auto *above = m_rows.data() + size_t((y - 1) % 3) * width;
        const auto *row = m_rows.data() + size_t(y % 3) * width;
        const auto *below = m_rows.data() + size_t((y + 1) % 3) * width;
        auto *out = m_samples.data() + size_t(y) * width;
        for (int x = 1; x < width - 1; ++x) {
            const float dx = row[x + 1] - row[x - 1];
            const float dy = below[x] - above[x];
            const bool horizontal = std::abs(dx) >= std::abs(dy);
            const float first = horizontal ? row[x - 1] : above[x];
            const float second = horizontal ? row[x + 1] : below[x];
            out[x].strength = std::sqrt(dx * dx + dy * dy) * 0.70710678118f;
            out[x].contrast = (std::max(first, second) + 0.05f)
                / (std::min(first, second) + 0.05f);
        }
    }
    return true;
}
} // namespace EdgeDetection

#include "edgedetection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace EdgeDetection {

bool RegionAnalyzer::analyze(const QImage &image)
{
    const int width = image.width();
    const int height = image.height();
    const size_t count = size_t(width) * size_t(height);
    if (!image.isNull() && (image.format() != QImage::Format_RGB32
            || width > MaximumDimension || height > MaximumDimension
            || count > MaximumPixels))
        return false;

    m_regions.clear();
    m_edges.resize(count);
    std::fill(m_edges.begin(), m_edges.end(), 0);
    m_queue.clear();
    if (!count)
        return true;
    // Reserve once at the current image size; subsequent same-size frames reuse
    // the queue even if their component topology changes completely.
    m_queue.reserve(count);
    m_regions.reserve(MaximumRegions);
    auto pixel = [&](int x, int y) {
        return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x] & 0xffffff;
    };
    auto difference = [](QRgb a, QRgb b) {
        return std::max({std::abs(qRed(a) - qRed(b)),
                         std::abs(qGreen(a) - qGreen(b)),
                         std::abs(qBlue(a) - qBlue(b))});
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QRgb p = pixel(x, y);
            const int right = x + 1 < width ? difference(p, pixel(x + 1, y)) : 0;
            const int below = y + 1 < height ? difference(p, pixel(x, y + 1)) : 0;
            if (std::max(right, below) >= MinimumChannelDifference)
                m_edges[size_t(y) * width + x] = right >= below ? 1 : 2;
        }
    }
    // This LUT intentionally follows the WCAG contrast recommendation's 0.03928
    // sRGB breakpoint, so the reported ratios match standard contrast math.
    static const auto contrastLinear = [] {
        std::array<double, 256> table{};
        for (int i = 0; i < 256; ++i) {
            const double c = i / 255.0;
            table[i] = c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
        }
        return table;
    }();
    auto luminanceOf = [](QRgb color) {
        return 0.2126 * contrastLinear[qRed(color)] + 0.7152 * contrastLinear[qGreen(color)]
            + 0.0722 * contrastLinear[qBlue(color)];
    };
    auto luminance = [&](const std::array<quint8, 3> &color) {
        return luminanceOf(qRgb(color[0], color[1], color[2]));
    };
    for (size_t start = 0; start < count && m_regions.size() < MaximumRegions; ++start) {
        if (!m_edges[start] || (m_edges[start] & 4))
            continue;
        m_queue.clear();
        m_queue.push_back(quint32(start));
        m_edges[start] |= 4;
        for (size_t head = 0; head < m_queue.size(); ++head) {
            const int x = int(m_queue[head] % width);
            const int y = int(m_queue[head] / width);
            for (int ny = std::max(0, y - 1); ny <= std::min(height - 1, y + 1); ++ny) {
                for (int nx = std::max(0, x - 1); nx <= std::min(width - 1, x + 1); ++nx) {
                    const size_t neighbor = size_t(ny) * width + nx;
                    if (m_edges[neighbor] && !(m_edges[neighbor] & 4)) {
                        m_edges[neighbor] |= 4;
                        m_queue.push_back(quint32(neighbor));
                    }
                }
            }
        }
        if (m_queue.size() < MinimumEdgePixels)
            continue;
        std::array<std::array<quint32, 256>, 6> histograms{};
        // Side luminance spread decides whether a boundary has solid anchors
        // on both sides. Quantized linear luminance, 64 bins over [0, 1).
        std::array<std::array<quint32, 64>, 2> sideLuminance{};
        int left = width, top = height, right = 0, bottom = 0;
        for (quint32 index : m_queue) {
            const int x = int(index % width);
            const int y = int(index / width);
            const bool horizontal = (m_edges[index] & 3) == 1;
            const int otherX = x + int(horizontal);
            const int otherY = y + int(!horizontal);
            QRgb a = pixel(x, y), b = pixel(otherX, otherY);
            // RGB numeric order is lexicographic R/G/B, independent of the
            // direction in which an outline happens to run.
            if (a > b)
                std::swap(a, b);
            ++histograms[0][qRed(a)];
            ++histograms[1][qGreen(a)];
            ++histograms[2][qBlue(a)];
            ++histograms[3][qRed(b)];
            ++histograms[4][qGreen(b)];
            ++histograms[5][qBlue(b)];
            sideLuminance[0][std::min(63, int(luminanceOf(a) * 64.0))]
                += 1;
            sideLuminance[1][std::min(63, int(luminanceOf(b) * 64.0))]
                += 1;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, otherX);
            bottom = std::max(bottom, otherY);
        }
        Region region;
        region.startPixel = QPoint(left, top);
        region.endPixel = QPoint(right, bottom);
        // A side with a solid color anchors its median in that color: the most
        // common luminance bin holds a clear majority. Blend ramps (thin text,
        // gradients) spread pixels across many bins and cannot be trusted.
        for (int side = 0; side < 2; ++side) {
            quint32 total = 0, dominant = 0;
            for (quint32 bin : sideLuminance[side]) {
                total += bin;
                dominant = std::max(dominant, bin);
            }
            region.indeterminate |= total > 0
                && float(dominant) / float(total) < MinimumDominantShare;
        }
        // Lower median for even sample counts keeps output in raw byte space.
        const quint32 rank = quint32((m_queue.size() - 1) / 2);
        for (size_t channel = 0; channel < 6; ++channel) {
            quint32 cumulative = 0;
            for (int value = 0; value < 256; ++value) {
                cumulative += histograms[channel][value];
                if (cumulative > rank) {
                    (channel < 3 ? region.color1 : region.color2)[channel % 3] = quint8(value);
                    break;
                }
            }
        }
        const double first = luminance(region.color1);
        const double second = luminance(region.color2);
        region.contrastRatio = float((std::max(first, second) + 0.05)
            / (std::min(first, second) + 0.05));
        m_regions.push_back(region);
    }
    return true;
}

Severity severityFor(float contrastRatio, bool indeterminate, float threshold)
{
    if (indeterminate)
        return Severity::Indeterminate;
    if (contrastRatio < threshold * 0.5f)
        return Severity::Critical;
    if (contrastRatio < threshold)
        return Severity::Warn;
    if (contrastRatio < 3.0f)
        return Severity::Marginal;
    return Severity::Hidden;
}
} // namespace EdgeDetection

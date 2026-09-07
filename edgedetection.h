#ifndef EDGEDETECTION_H
#define EDGEDETECTION_H

#include <QImage>
#include <QPoint>
#include <array>
#include <vector>

namespace EdgeDetection {

struct Sample {
    float strength = 0.0f; // Normalized central luminance difference, [0, 1].
    float contrast = 1.0f; // Opposing-neighbor luminance ratio, [1, 21].
};

// Reuse one Analyzer per worker. Input remains untouched. Samples are row-major
// and remain valid until the next analyze() call. No per-edge allocations.
// RGB32 and straight ARGB32 only; ARGB32 is composited onto opaque white in
// linear light. Untagged input is assumed sRGB; no color-space conversion.
class Analyzer {
public:
    bool analyze(const QImage &image);
    QSize size() const { return m_size; }
    const std::vector<Sample> &samples() const { return m_samples; }

private:
    QSize m_size;
    std::vector<Sample> m_samples;
    std::vector<float> m_rows;
};

struct Region {
    QPoint startPixel; // Inclusive bounding box in input-image coordinates.
    QPoint endPixel;
    std::array<quint8, 3> color1{}; // Raw channel-wise median RGB bytes.
    std::array<quint8, 3> color2{};
    float contrastRatio = 1.0f;
};

// Hazard Mode outlines regions whose contrast ratio falls below this value.
// Regions at or above the threshold are considered safely distinguishable.
inline constexpr float ContrastAlarmThreshold = 2.0f;

// Analyze the already simulated, caller-sized RGB32 image. No rescaling or input
// mutation. Connected RGB edges retain low-contrast and equal-luminance colors.
// Own one instance per worker. Results remain valid until the next analyze().
class RegionAnalyzer {
public:
    static constexpr size_t MaximumPixels = 1024 * 1024;
    static constexpr int MaximumDimension = 4096;
    static constexpr size_t MaximumRegions = 1024;
    static constexpr int MinimumChannelDifference = 12;
    static constexpr size_t MinimumEdgePixels = 4;

    // Unsupported formats / oversized input return false, preserving results.
    // Null input succeeds and clears results. RGB32 is deliberately required:
    // raw median bytes cannot also describe compositing translucent ARGB input.
    bool analyze(const QImage &image);
    const std::vector<Region> &regions() const { return m_regions; }

private:
    std::vector<quint8> m_edges;
    std::vector<quint32> m_queue;
    std::vector<Region> m_regions;
};

} // namespace EdgeDetection

#endif

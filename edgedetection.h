#ifndef EDGEDETECTION_H
#define EDGEDETECTION_H

#include <QImage>
#include <QPoint>
#include <array>
#include <vector>

namespace EdgeDetection {

struct Region {
    QPoint startPixel; // Inclusive bounding box in input-image coordinates.
    QPoint endPixel;
    std::array<quint8, 3> color1{}; // Raw channel-wise median RGB bytes.
    std::array<quint8, 3> color2{};
    float contrastRatio = 1.0f;
    // True when either edge side lacks sufficient stable solid color anchors, so the
    // medians come from blend ramps and contrastRatio is untrustworthy.
    bool indeterminate = false;
};

// Default Hazard Mode alarm: regions below this contrast ratio are outlined.
// The UI offers 3:1 and 4.5:1 (WCAG graphics / text programs) as alternatives.
inline constexpr float DefaultAlarmThreshold = 2.0f;

// An edge side whose most common luminance bin holds at least this share of
// the side's pixels has a solid color anchor; otherwise the boundary is a
// blend ramp (thin text, gradients) and the ratio cannot be trusted.
inline constexpr float MinimumDominantShare = 0.45f;

// Severity for one region, derived from its contrast ratio relative to the
// chosen alarm threshold. Severity::Indeterminate overrides all ratio bands.
enum class Severity { Hidden, Marginal, Warn, Critical, Indeterminate };

// Only trustworthy ratios strictly below the chosen threshold are visible.
// Invalid/indeterminate measurements are hidden. Critical is below half the
// threshold; all other visible results are Warn.
Severity severityFor(float contrastRatio, bool indeterminate, float threshold);

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

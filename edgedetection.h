#ifndef EDGEDETECTION_H
#define EDGEDETECTION_H

#include <QImage>
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

} // namespace EdgeDetection

#endif

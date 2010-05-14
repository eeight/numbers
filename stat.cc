#include "stat.hh"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace {

template <class T>
T sqr(T x) {
    return x*x;
}

} // namespace

void Stat::addSample(double value) {
    samples_.push_back(value);
}

std::pair<double, double> Stat::getAverageAndStandardDeviation(
            size_t sampleRunSize) const {
    if (samples_.empty()) {
        throw std::logic_error("Cannot compute stats with no samples");
    }

    double average = std::accumulate(
            samples_.begin(), samples_.end(), 0.0)/samples_.size();
    double variation = 0.0;

    for (size_t i = 0; i != samples_.size(); ++i) {
        variation += sqr(samples_[i] - average);
    }

    double standardDeviation = std::sqrt(
            variation/samples_.size());

    return std::make_pair(
            average/sampleRunSize, standardDeviation/sampleRunSize);
}

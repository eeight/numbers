#include "stat.hh"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include <iostream>

namespace {

template <class T>
T sqr(T x) {
    return x*x;
}

} // namespace

Stat operator -(const Stat &lhs, const Stat &rhs) {
    return Stat(
            lhs.average() - rhs.average(),
            lhs.standardDeviation() + rhs.standardDeviation());
}

void StatCollector::addSample(double value) {
    samples_.push_back(value);
}

Stat StatCollector::getStat(size_t sampleRunSize) const {
    if (samples_.empty()) {
        throw std::logic_error("Cannot compute stats with no samples");
    }

    double average = std::accumulate(
            samples_.begin(), samples_.end(), 0.0)/samples_.size();
    double variation = 0.0;


    for (size_t i = 0; i != samples_.size(); ++i) {
        variation += sqr(samples_[i] - average);
    }

    double standardDeviation = std::sqrt(variation/samples_.size());

    return Stat(average/sampleRunSize, standardDeviation/sampleRunSize);
}

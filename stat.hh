#ifndef STAT_H
#define STAT_H

#include <vector>
#include <utility>

class Stat {
public:
    void addSample(double value);
    std::pair<double, double> getAverageAndStandardDeviation(
            size_t sampleRunSize) const;

private:
    std::vector<double> samples_;
};

#endif

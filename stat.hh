#pragma once

#include <vector>

class Stat {
public:
    Stat() :
        average_(0), standardDeviation_(0)
    {}

    Stat(double average, double standardDeviation) :
        average_(average), standardDeviation_(standardDeviation)
    {}

    double average() const { return average_; }
    double standardDeviation() const { return standardDeviation_; }

private:
    double average_, standardDeviation_;
};

// Assumes that lhs and rhs are not correlated.
Stat operator -(const Stat &lhs, const Stat &rhs);

class StatCollector {
public:
    void addSample(double value);
    Stat getStat(size_t sampleRunSize) const;

private:
    std::vector<double> samples_;
};

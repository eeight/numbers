#include "timer.hh"

#include <stdexcept>

namespace {

double doubleTime(const timeval& time) {
    return time.tv_sec + 1e-6*time.tv_usec;
}

} // namespace

Timer::Timer() {
    if (gettimeofday(&startTime_, 0) < 0) {
        throw std::runtime_error("gettimeofday failed");
    }
}

double Timer::elapsed() const {
    timeval endTime;
    if (gettimeofday(&endTime, 0) < 0) {
        throw std::runtime_error("gettimeofday failed");
    }
    return doubleTime(endTime) - doubleTime(startTime_);
}


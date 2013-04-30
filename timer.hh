#pragma once

#include <sys/time.h>

class Timer {
public:
    Timer();
    double elapsed() const;

private:
    timeval startTime_;
};

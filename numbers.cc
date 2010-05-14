#include "stat.hh"

#include "virtual.hh"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

#include <boost/format.hpp>
#include <boost/timer.hpp>
#include <boost/tuple/tuple.hpp>

/* TODO
 * cache miss
 * branch mispredict
 * syscall
 * mutex lock/unlock
 * boost::function call
 * malloc/free
 * read 1mb sequentially from disk (read and mmap)
 * write 1mb sequentially to disk (write and mmap)
 * create thread
 * sync one cache line in L1 caches
 * int + * /
 * float + * /
 * double + * /
 * copy 1mb vector
 * table-based switch
 * sort 1mb vector
 * stable_sort 1mb vector
 * sort 1mb list * traverse 1mb vector
 * traverse 1mb deque
 * traverse 1mb list
 * traverse 1mb map
 * traverse 1mb set
 * shared_ptr copy
 * shared_ptr deref
 * __sync_bool_compare_and_swap
 * lexical_cast<string>(int)
 * lexical_cast<int>(string)
 * sprintf
 * sscanf
 * boost::format
 * cancat strings
 * output 2 strings to stringstream and obtain str()
 */

class StatInfo {
public:
    StatInfo(const char *name, size_t sampleRunSize, 
            double (*sampler)()) :
        name_(name), sampleRunSize_(sampleRunSize), sampler_(sampler)
    {}

    const char *name() const { return name_; }
    size_t sampleRunSize() const { return sampleRunSize_; }
    double (*sampler() const)() { return sampler_; }

private:
    const char *name_;
    size_t sampleRunSize_;
    double (*sampler_)();
};

std::vector<StatInfo> g_StatInfos;

struct StatInfoRegistrator {
    StatInfoRegistrator(const StatInfo &statInfo) {
        g_StatInfos.push_back(statInfo);
    }
};

class TimerPitcher {
public:
    operator bool() const { return false; }
    void useMe() const  {}
    ~TimerPitcher() {
        throw time_.elapsed();
    }

private:
    boost::timer time_;
};

/*
 * There is a bit of black magic going here.
 * We want to define sampling piece of function with one use of SAMPLE().
 * In order to achieve this, before target code is ran, TimerPitcher
 * object is beging created. It throws elapsed time in destructor thus
 * returing result of sampling. Target sampling function is wrapped in
 * auxiliary function catching the value and returning it the normal way.
 */
#define STAT(name, samples) \
    template <size_t SAMPLE_RUN_SIZE> \
    void stat_aux_##name(); \
    double stat_##name() { \
        try { \
            stat_aux_##name<samples>(); \
        } \
        catch (double time) { \
            return time; \
        } \
        return 0; \
    } \
    StatInfoRegistrator statRegistrator_##name( \
            StatInfo(#name, samples, &stat_##name)); \
    template <size_t SAMPLE_RUN_SIZE> \
    void stat_aux_##name()

#define SAMPLE() \
    if (const TimerPitcher &timerPitcher = TimerPitcher()) { \
        timerPitcher.useMe(); \
    } else \
    for (size_t i = 0; i != SAMPLE_RUN_SIZE;  ++i)

/*
 * Measures overhead of making a virtual call.
 *
 * Definitions of the classes are moved to separate translation unit
 * in order to prevent compiler of being too smart and inlining the call.
 */
STAT(virtual_call, 1024*1024*10) {
    const virt::Base &base = virt::Derived();
    SAMPLE() {
       base.f();
    }
}

/*
 * Measure overhead of making a syscall.
 *
 * Use write() with invalid argument, so it returns almost
 * immediately and time spend in actual syscall body is insignificant.
 */
STAT(syscall, 1024*1020) {
    SAMPLE() {
        write(10, 0, 0);
    }
}

void collectAndPrintStat(const StatInfo &statInfo) {
    Stat stat;
    const size_t SAMPLE_COUNT = 100;

    for (size_t i = 0; i != SAMPLE_COUNT; ++i) {
        stat.addSample(statInfo.sampler()());
    }

    double average, standardDeviation;
    boost::tie(average, standardDeviation) =
        stat.getAverageAndStandardDeviation(statInfo.sampleRunSize());
    std::cout << boost::format(
            "%s:\taverage: %fns;\tstandard deviation: %fns\n") %
        statInfo.name() % (average*1e9) % (standardDeviation*1e9);
}

int main()
{
    std::for_each(g_StatInfos.begin(), g_StatInfos.end(),
            std::ptr_fun(&collectAndPrintStat));
    return 0;
}

#include "stat.hh"

#include "branch_mispredict.hh"
#include "virtual.hh"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/timer.hpp>
#include <boost/tuple/tuple.hpp>

/* TODO
 * cache miss
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

namespace {

const double MAX_RELATIVE_STANDARD_DEVIATION = 0.15;

class StatSample {
public:
    StatSample(const char *name) : name_(name)
    {}

    virtual ~StatSample() {}

    virtual Stat collectStat(
            size_t sampleRunSize, size_t sampleCount) const = 0;

    const char *name() const { return name_; }

private:
    const char *name_;
};

typedef double (*Sampler)(size_t sampleRunSize);

class PlainStatSample : public StatSample {
public:
    PlainStatSample(const char *name, Sampler sampler) :
        StatSample(name), sampler_(sampler)
    {}

    Stat collectStat(size_t sampleRunSize, size_t sampleCount) const {
        StatCollector statCollector;

        for (size_t i = 0; i != sampleCount; ++i) {
            statCollector.addSample(sampler_(sampleRunSize));
        }

        return statCollector.getStat(sampleRunSize);
    }

private:
    Sampler sampler_;
};

class DifferenceStatSample : public StatSample {
public:
    DifferenceStatSample(const char *name,
            Sampler firstSampler, Sampler secondSampler) :
        StatSample(name),
        firstSampler_(firstSampler), secondSampler_(secondSampler)
    {}

    Stat collectStat(size_t sampleRunSize, size_t sampleCount) const {
        StatCollector firstStatCollector;
        StatCollector secondStatCollector;

        for (size_t i = 0; i != sampleCount; ++i) {
            firstStatCollector.addSample(firstSampler_(sampleRunSize));
        }
        for (size_t i = 0; i != sampleCount; ++i) {
            secondStatCollector.addSample(secondSampler_(sampleRunSize));
        }

        return firstStatCollector.getStat(sampleRunSize) -
            secondStatCollector.getStat(sampleRunSize);
    }

private:
    Sampler firstSampler_;
    Sampler secondSampler_;
};

boost::ptr_vector<StatSample> g_StatInfos;

struct StatSampleRegistrator {
    StatSampleRegistrator(StatSample *statSample) {
        g_StatInfos.push_back(statSample);
    }
};

class TimerPitcher {
public:
    TimerPitcher(bool value) : value_(value)
    {}

    ~TimerPitcher() {
        throw time_.elapsed();
    }

    operator bool() const { return value_; }

private:
    bool value_;
    boost::timer time_;
};

} // namespace

#define STAT_PRELUDE(name) \
    template <bool IS_FIRST> \
    void stat_aux_##name(size_t sampleRunSize);

#define STAT_FIN(name) \
    template <bool IS_FIRST> \
    void stat_aux_##name(size_t sampleRunSize)

#define STAT_WRAPPER(name, aux_name, isFirst) \
    double stat_##name(size_t sampleRunSize) { \
        try { \
            stat_aux_##aux_name<isFirst>(sampleRunSize); \
        } \
        catch (double time) { \
            return time; \
        } \
        throw std::runtime_error( \
                "Expected sampler to return elapsed time via throw"); \
    }

/*
 * There is a bit of black magic going here.
 * We want to define sampling piece of function with one use of SAMPLE().
 * In order to achieve this, before target code is ran, TimerPitcher
 * object is beging created. It throws elapsed time in destructor thus
 * returing result of sampling. Target sampling function is wrapped in
 * auxiliary function catching the value and returning it the normal way.
 */
#define STAT(name) \
    STAT_PRELUDE(name) \
    STAT_WRAPPER(name, name, true) \
    StatSampleRegistrator statRegistrator_##name( \
            new PlainStatSample(#name, &stat_##name)); \
    STAT_FIN(name)

#define STAT_DIFF(name) \
    STAT_PRELUDE(name) \
    STAT_WRAPPER(first_##name, name, true) \
    STAT_WRAPPER(second_##name, name, false) \
    StatSampleRegistrator statRegistrator_##name(new DifferenceStatSample( \
                #name, &stat_first_##name, &stat_second_##name)); \
    STAT_FIN(name)

#define SAMPLE() \
    if (const TimerPitcher &timerPitcher __attribute__((unused)) = TimerPitcher(IS_FIRST)) \
        for (size_t i = 0; i != sampleRunSize;  ++i)

#define AND() else for (size_t i = 0; i != sampleRunSize;  ++i)

/*
 * Definitions of the classes are moved to separate compilation unit
 * in order to prevent compiler of being too smart and inlining the call.
 */
STAT_DIFF(virtual_call) {
    const virt::Derived &base = virt::DDerived();

    SAMPLE() {
        virt::f(&base);
    } AND() {
        virt::g(&base);
    }
}

/*
 * Use write() with invalid argument, so it returns almost
 * immediately and time spend in actual syscall body is insignificant.
 */
STAT(syscall) {
    SAMPLE() {
        // Currently (until gcc 4.5 arrives), there is no way to
        // disable
        // "warning: ignoring return value of `...', declared
        // with attribute warn_unused_result"
        //
        // So as a workaround we here declare unused variable.
        int a __attribute__((unused));
        a = write(10, 0, 0);
    }
}

STAT(mutex) {
    boost::mutex mutex;

    SAMPLE() {
        boost::mutex::scoped_lock lock(mutex);
    }
}

void boost_function_test() {
}

STAT(boost_function) {
    boost::function<void ()> functor(&boost_function_test);

    SAMPLE() {
        functor();
    }
}

STAT_DIFF(branch_mispredict) {
    SAMPLE() {
        if (__builtin_expect(branch_mispredict::returns1(), 0)) {
            branch_mispredict::f();
        } else {
            branch_mispredict::g();
        }
    } AND() {
        if (__builtin_expect(branch_mispredict::returns1(), 1)) {
            branch_mispredict::f();
        } else {
            branch_mispredict::g();
        }
    }
}

void collectAndPrintStat(const StatSample &statSample) {
    const size_t SAMPLE_COUNT = 100;
    const size_t MAX_SAMPLE_RUN_SIZE = 16777216;
    size_t sampleRunSize = 1024/2;
    Stat stat;

    do {
        sampleRunSize *= 2;
        if (stat.average() > 0) {
            std::cerr << boost::format(
                    "avg: %fμs\tstddev: %fμs\t stddev/avg: %f is too bad\n") %
                (stat.average()*1e9) %
                (stat.standardDeviation()*1e9) %
                (stat.standardDeviation() / stat.average());
        }
        std::cerr << boost::format("Using sample run of %d for %s\n") %
            sampleRunSize % statSample.name();
        stat = statSample.collectStat(sampleRunSize, SAMPLE_COUNT);
    } while ((stat.average() == 0 ||
            stat.average()*MAX_RELATIVE_STANDARD_DEVIATION <
                stat.standardDeviation()) &&
            sampleRunSize < MAX_SAMPLE_RUN_SIZE);

    std::cout << boost::format(
            "%s:\taverage: %fμs;\tstandard deviation: %fμs\n") %
        statSample.name() %
        (stat.average()*1e9) %
        (stat.standardDeviation()*1e9);
}

int main()
{
    std::for_each(g_StatInfos.begin(), g_StatInfos.end(),
            std::ptr_fun(&collectAndPrintStat));
    return 0;
}

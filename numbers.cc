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

class StatSample {
public:
    StatSample(const char *name, size_t sampleRunSize) :
        name_(name), sampleRunSize_(sampleRunSize)
    {}

    virtual ~StatSample() {}

    virtual Stat collectStat(size_t sampleCount) const = 0;

    const char *name() const { return name_; }
    size_t sampleRunSize() const { return sampleRunSize_; }

private:
    const char *name_;
    size_t sampleRunSize_;
};

class PlainStatSample : public StatSample {
public:
    PlainStatSample(const char *name, size_t sampleRunSize,
            double (*sampler)()) :
        StatSample(name, sampleRunSize), sampler_(sampler)
    {}

    Stat collectStat(size_t sampleCount) const {
        StatCollector statCollector;

        for (size_t i = 0; i != sampleCount; ++i) {
            statCollector.addSample(sampler_());
        }

        return statCollector.getStat(sampleRunSize());
    }

private:
    double (*sampler_)();
};

class DifferenceStatSample : public StatSample {
public:
    DifferenceStatSample(const char *name, size_t sampleRunSize,
            double (*firstSampler)(), double (*secondSampler)()) :
        StatSample(name, sampleRunSize),
        firstSampler_(firstSampler), secondSampler_(secondSampler)
    {}

    Stat collectStat(size_t sampleCount) const {
        StatCollector firstStatCollector;
        StatCollector secondStatCollector;

        for (size_t i = 0; i != sampleCount; ++i) {
            firstStatCollector.addSample(firstSampler_());
        }
        for (size_t i = 0; i != sampleCount; ++i) {
            secondStatCollector.addSample(secondSampler_());
        }

        return firstStatCollector.getStat(sampleRunSize()) -
            secondStatCollector.getStat(sampleRunSize());
    }

private:
    double (*firstSampler_)();
    double (*secondSampler_)();
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
    template <size_t SAMPLE_RUN_SIZE, bool IS_FIRST> \
    void stat_aux_##name();

#define STAT_FIN(name) \
    template <size_t SAMPLE_RUN_SIZE, bool IS_FIRST> \
    void stat_aux_##name()

#define STAT_WRAPPER(name, aux_name, samples, isFirst) \
    double stat_##name() { \
        try { \
            stat_aux_##aux_name<samples, isFirst>(); \
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
#define STAT(name, samples) \
    STAT_PRELUDE(name) \
    STAT_WRAPPER(name, name, samples, true) \
    StatSampleRegistrator statRegistrator_##name( \
            new PlainStatSample(#name, samples, &stat_##name)); \
    STAT_FIN(name)

#define STAT_DIFF(name, samples) \
    STAT_PRELUDE(name) \
    STAT_WRAPPER(first_##name, name, samples, true) \
    STAT_WRAPPER(second_##name, name, samples, false) \
    StatSampleRegistrator statRegistrator_##name(new DifferenceStatSample( \
                #name, samples, &stat_first_##name, &stat_second_##name)); \
    STAT_FIN(name)

#define SAMPLE() \
    if (const TimerPitcher &timerPitcher __attribute__((unused)) = TimerPitcher(IS_FIRST)) \
        for (size_t i = 0; i != SAMPLE_RUN_SIZE;  ++i)

#define AND() else for (size_t i = 0; i != SAMPLE_RUN_SIZE;  ++i)

/*
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
 * Use write() with invalid argument, so it returns almost
 * immediately and time spend in actual syscall body is insignificant.
 */
STAT(syscall, 1024*1024) {
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

STAT(mutex, 1024*1024*4) {
    boost::mutex mutex;

    SAMPLE() {
        boost::mutex::scoped_lock lock(mutex);
    }
}

void boost_function_test() {
}

STAT(boost_function, 1024*1024*10) {
    boost::function<void ()> functor(&boost_function_test);

    SAMPLE() {
        functor();
    }
}

STAT_DIFF(branch_mispredict, 1024*1024*10) {
    SAMPLE() {
        if (__builtin_expect(branch_mispredict::returns1(), 0)) {
            branch_mispredict::f();
        } else {
            branch_mispredict::g();
        }
    }
    AND() {
        if (__builtin_expect(branch_mispredict::returns1(), 1)) {
            branch_mispredict::f();
        } else {
            branch_mispredict::g();
        }
    }
}

void collectAndPrintStat(const StatSample &statSample) {
    const size_t SAMPLE_COUNT = 100;
    Stat stat = statSample.collectStat(SAMPLE_COUNT);

    std::cout << boost::format(
            "%s:\taverage: %fns;\tstandard deviation: %fns\n") %
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

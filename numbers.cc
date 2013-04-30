#include "stat.hh"
#include "timer.hh"

#include "separate.hh"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>
#include <stdexcept>

#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/tuple/tuple.hpp>

/* TODO
 * cache miss
 * read 1mb sequentially from disk (read and mmap)
 * write 1mb sequentially to disk (write and mmap)
 * sync one cache line in L1 caches
 * int + * /
 * float + * /
 * double + * /
 * table-based switch
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
        throw timer_.elapsed();
    }

    operator bool() const { return value_; }

private:
    bool value_;
    Timer timer_;
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
    const Derived &d = DDerived();

    SAMPLE() {
        callDerivedF(&d);
    } AND() {
        doNothingWithDerived(&d);
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

#if 0
STAT_DIFF(branch_mispredict) {
    SAMPLE() {
        if (__builtin_expect(return1(), 0)) {
            doNothing1();
        } else {
            doNothing2();
        }
    } AND() {
        if (__builtin_expect(return1(), 1)) {
            doNothing1();
        } else {
            doNothing2();
        }
    }
}
#endif

STAT(malloc_free_1kb) {
    SAMPLE() {
        free(disguisedMalloc(1024));
    }
}

STAT(malloc_free_1mb) {
    SAMPLE() {
        free(disguisedMalloc(1024*1024));
    }
}

void create_thread_worker() {
}

STAT(create_thread) {
    SAMPLE() {
        boost::thread thread(&create_thread_worker);
        thread.join();
    }
}

const size_t COPY_SIZE = 1024*10;

STAT(copy_vector_10k) {
    std::vector<int> v;
    std::fill_n(std::back_inserter(v), COPY_SIZE, 42);

    SAMPLE() {
        copyVector(v);
    }
}

STAT(copy_deque_10k) {
    std::deque<int> d;
    std::fill_n(std::back_inserter(d), COPY_SIZE, 42);

    SAMPLE() {
        copyDeque(d);
    }
}

STAT(copy_list_10k) {
    std::list<int> l;
    std::fill_n(std::back_inserter(l), COPY_SIZE, 42);

    SAMPLE() {
        copyList(l);
    }
}

STAT(copy_set_10k) {
    std::set<int> s;
    for (size_t i = 0; i != COPY_SIZE; ++i) {
        s.insert(i);
    }

    SAMPLE() {
        copySet(s);
    }
}

STAT(copy_map_10k) {
    std::map<int, int> m;
    for (size_t i = 0; i != COPY_SIZE; ++i) {
        m[i] = i;
    }

    SAMPLE() {
        copyMap(m);
    }
}

const size_t TRAVERSE_SIZE = 1024*100;

STAT(traverse_vector_100k) {
    std::vector<int> v;
    std::fill_n(std::back_inserter(v), TRAVERSE_SIZE, 42);

    SAMPLE() {
        for (std::vector<int>::const_iterator i = v.begin();
                i != v.end(); ++i) {
            doNothingWithParam(*i);
        }
    }
}

STAT(traverse_vector_index_100k) {
    std::vector<int> v;
    std::fill_n(std::back_inserter(v), TRAVERSE_SIZE, 42);

    SAMPLE() {
        for (size_t i = 0; i != v.size(); ++i) {
            doNothingWithParam(v[i]);
        }
    }
}

STAT(traverse_deque_100k) {
    std::deque<int> d;
    std::fill_n(std::back_inserter(d), TRAVERSE_SIZE, 42);

    SAMPLE() {
        for (std::deque<int>::const_iterator i = d.begin();
                i != d.end(); ++i) {
            doNothingWithParam(*i);
        }
    }
}

STAT(traverse_list_100k) {
    std::list<int> l;
    std::fill_n(std::back_inserter(l), TRAVERSE_SIZE, 42);

    SAMPLE() {
        for (std::list<int>::const_iterator i = l.begin();
                i != l.end(); ++i) {
            doNothingWithParam(*i);
        }
    }
}

STAT(traverse_set_100k) {
    std::set<int> s;
    for (size_t i = 0; i != TRAVERSE_SIZE; ++i) {
        s.insert(i);
    }

    SAMPLE() {
        for (std::set<int>::const_iterator i = s.begin();
                i != s.end(); ++i) {
            doNothingWithParam(*i);
        }
    }
}

STAT(traverse_map_100k) {
    std::map<int, int> m;
    for (size_t i = 0; i != TRAVERSE_SIZE; ++i) {
        m[i] = i;
    }

    SAMPLE() {
        for (std::map<int, int>::const_iterator i = m.begin();
                i != m.end(); ++i) {
            doNothingWithParam(i->second);
        }
    }
}

const size_t SORT_SIZE = 1024*10;

STAT(sort_10k_vector) {
    std::vector<int> v;

    SAMPLE() {
        v.clear();
        for (size_t i = 0; i != SORT_SIZE; ++i) {
            v.push_back(rand());
        }
        std::sort(v.begin(), v.end());
    }
}

STAT(stable_sort_10k_vector) {
    std::vector<int> v;

    SAMPLE() {
        v.clear();
        for (size_t i = 0; i != SORT_SIZE; ++i) {
            v.push_back(rand());
        }
        std::stable_sort(v.begin(), v.end());
    }
}

STAT(sort_10k_deque) {
    std::deque<int> d;

    SAMPLE() {
        d.clear();
        for (size_t i = 0; i != SORT_SIZE; ++i) {
            d.push_back(rand());
        }
        std::sort(d.begin(), d.end());
    }
}

STAT(sort_10k_list) {
    std::list<int> l;

    SAMPLE() {
        l.clear();
        for (size_t i = 0; i != SORT_SIZE; ++i) {
            l.push_back(rand());
        }
        l.sort();
    }
}

STAT(shared_ptr_copy) {
    boost::shared_ptr<Base> sharedPtr(new Derived);

    SAMPLE() {
        copySharedPtr(sharedPtr);
    }
}

STAT(shared_ptr_deref) {
    boost::shared_ptr<Derived> sharedPtr(new Derived);

    SAMPLE() {
        doNothingWithDerived(&*sharedPtr);
    }
}

STAT(cas) {
    int a = 1, b = 2;

    SAMPLE() {
        __sync_bool_compare_and_swap(&a, 1, b);
    }

    doNothingWithParam(a);
    doNothingWithParam(b);
}

const int INT_VALUE = 2255;
const char STRING_VALUE[] = "2255";

STAT(lexical_cast_int_string) {
    int a = INT_VALUE;

    SAMPLE() {
        doNothingWithParam(boost::lexical_cast<std::string>(a));
    }
}

STAT(lexical_cast_string_int) {
    std::string s = STRING_VALUE;

    SAMPLE() {
        doNothingWithParam(boost::lexical_cast<int>(s));
    }
}

STAT(boost_format) {
    int a = INT_VALUE;

    SAMPLE() {
        doNothingWithParam((boost::format("%d")%a).str());
    }
}

STAT(sprintf) {
    int a = INT_VALUE;
    char buffer[1024];

    SAMPLE() {
        doNothingWithParam(snprintf(buffer, sizeof(buffer), "%d", a));
    }
}

STAT(sscanf) {
    const char *s = STRING_VALUE;
    int a;

    SAMPLE() {
        doNothingWithParam(sscanf(s, "%d", &a));
    }
    doNothingWithParam(a);
}

const char FIRST_STRING[] = "firstfirstfirstfirstfirstfirst";
const char SECOND_STRING[] = "secondsecondsecondsecondsecondsecond";

STAT(string_concat) {
    std::string a = FIRST_STRING;
    std::string b = SECOND_STRING;

    SAMPLE() {
        doNothingWithParam(a + b);
    }
}

STAT(string_stream_concat) {
    std::string a = FIRST_STRING;
    std::string b = SECOND_STRING;

    SAMPLE() {
        std::stringstream ss;

        ss << a << b;
        doNothingWithParam(ss.str());
    }
}

STAT(time) {
    SAMPLE() {
        time_t a __attribute__((unused));

        a = time(0);
    }
}

STAT(throw_exception) {
    SAMPLE() {
        try {
            throwException();
        } catch (const std::exception &) {
        }
    }
}

void collectAndPrintStat(const StatSample &statSample) {
    const size_t SAMPLE_COUNT = 100;
    const double MAX_RUNNING_TIME = 60;

    size_t sampleRunSize = 4;
    Stat stat;
    double runningTime;

    do {
        sampleRunSize *= 2;
        Timer timer_;
        stat = statSample.collectStat(sampleRunSize, SAMPLE_COUNT);
        runningTime = timer_.elapsed();
    } while ((stat.average() == 0 ||
            stat.average()*MAX_RELATIVE_STANDARD_DEVIATION <
                stat.standardDeviation()) &&
            runningTime < MAX_RUNNING_TIME);

    std::cout << boost::format(
            "\"%s\": (%f, %f),\n") % 
        statSample.name() %
        (stat.average()*1e9) %
        (stat.standardDeviation()*1e9);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        std::string command = argv[1];

        if (command == "list") {
            for (size_t i = 0; i != g_StatInfos.size(); ++i) {
                std::cout << g_StatInfos[i].name() << std::endl;
            }
            return 0;
        }
        for (size_t i = 0; i != g_StatInfos.size(); ++i) {
            if (command == g_StatInfos[i].name()) {
                collectAndPrintStat(g_StatInfos[i]);
                return 0;
            }
        }

        std::cerr << "Unknown stat name: " << command << std::endl;
        return 1;
    }
    std::cout << "{" << std::endl;;
    std::for_each(g_StatInfos.begin(), g_StatInfos.end(),
            std::ptr_fun(&collectAndPrintStat));
    std::cout << "}" << std::endl;;
    return 0;
}

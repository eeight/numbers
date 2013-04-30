#include "separate.hh"

#include <stdexcept>

void Base::f() const {}
void Derived::f() const {}
void DDerived::f() const {}

template <class T>
T return1() {
    return 1;
}

template int return1<int>();
template float return1<float>();
template double return1<double>();
template long double return1<long double>();
template __float128 return1<__float128>();

void doNothing1() {}
void doNothing2() {}

void copyVector(std::vector<int> v __attribute__((unused))) {}
void copyDeque(std::deque<int> v __attribute__((unused))) {}
void copyList(std::list<int> v __attribute__((unused))) {}
void copySet(std::set<int> v __attribute__((unused))) {}
void copyMap(std::map<int, int> v __attribute__((unused))) {}

void copySharedPtr(boost::shared_ptr<Base> ptr __attribute__((unused))) {}

template <class T>
void doNothingWithParam(T v __attribute__((unused))) {}

template void doNothingWithParam<int>(int);
template void doNothingWithParam<const char *>(const char *);
template void doNothingWithParam<const std::string &>(const std::string &);
template void doNothingWithParam<float>(float);
template void doNothingWithParam<double>(double);
template void doNothingWithParam<long double>(long double);
template void doNothingWithParam<__float128>(__float128);

void callDerivedF(const Derived *derived) {
    derived->f();
}

void doNothingWithDerived(const Derived *derived __attribute__((unused))) {}

void throwException() {
    throw std::runtime_error("You've asked for an exception");
}

void* disguisedMalloc(size_t size) {
    return malloc(size);
}

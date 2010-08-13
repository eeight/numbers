#include "separate.hh"

#include <stdexcept>

void Base::f() const {}
void Derived::f() const {}
void DDerived::f() const {}

int return1() {
    return 1;
}

void doNothing1() {}
void doNothing2() {}

void copyVector(std::vector<int> v __attribute__((unused))) {}
void copyDeque(std::deque<int> v __attribute__((unused))) {}
void copyList(std::list<int> v __attribute__((unused))) {}
void copySet(std::set<int> v __attribute__((unused))) {}
void copyMap(std::map<int, int> v __attribute__((unused))) {}

void copySharedPtr(boost::shared_ptr<Base> ptr __attribute__((unused))) {}

void doNothingWithParam(int v __attribute__((unused))) {}
void doNothingWithParam(const std::string &s __attribute__((unused))) {}
void doNothingWithParam(const char *c __attribute__((unused))) {}

void callDerivedF(const Derived *derived) {
    derived->f();
}

void doNothingWithDerived(const Derived *derived __attribute__((unused))) {}

void throwException() {
    throw std::runtime_error("You've asked for an exception");
}

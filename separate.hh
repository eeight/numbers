#pragma once

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

struct Base {
    virtual void f() const;
    virtual ~Base() {}
};

struct Derived : Base {
    void f() const;
};

struct DDerived : Derived {
    void f() const;
};

template <class T>
T return1();

void doNothing1();
void doNothing2();

void copyVector(std::vector<int> v);
void copyDeque(std::deque<int> v);
void copyList(std::list<int> v);
void copySet(std::set<int> v);
void copyMap(std::map<int, int> v);

void copySharedPtr(boost::shared_ptr<Base> ptr);

template <class T>
void doNothingWithParam(T v);

void callDerivedF(const Derived *derived);

void doNothingWithDerived(const Derived *derived);

void throwException();

void* disguisedMalloc(size_t size);

#ifndef VIRTUAL_H
#define VIRTUAL_H

namespace virt {

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

void f(const Derived *derived);

void g(const Derived *derived);


} // namespace virt

#endif

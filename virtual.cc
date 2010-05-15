#include "virtual.hh"

namespace virt {

void Base::f() const {}

void Derived::f() const {}

void DDerived::f() const {}

void f(const Derived *derived) {
    derived->f();
}

void g(const Derived *derived __attribute__((unused))) {
}

} // namespace virt

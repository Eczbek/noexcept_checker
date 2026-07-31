#include "../throws.hpp"

struct A {
	A(int) noexcept {};
};

void g() THROWS(int) {
	A((throw 0, 0));
}

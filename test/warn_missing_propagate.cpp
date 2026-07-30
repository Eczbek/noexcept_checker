#include "../throws.hpp"

void f() THROWS(int) {}

void g() noexcept {
	f();
}

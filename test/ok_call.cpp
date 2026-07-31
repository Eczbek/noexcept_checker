#include "../throws.hpp"

void f(int) noexcept {}

void g() THROWS(int) {
	f((throw 0, 0));
}

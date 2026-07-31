#include "../throws.hpp"

void f() THROWS(int) {
	([](int) noexcept {})((throw 0, 0));
}

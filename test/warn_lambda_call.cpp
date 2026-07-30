#include "../throws.hpp"

auto f = [] THROWS(int) {
	throw 0;
};

void g() {
	f();
}

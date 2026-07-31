#include "../throws.hpp"

void f() noexcept(false) {
	throw;
}

void g() THROWS(int) {
	try {
		throw 0;
	} catch (const int&) {
		f();
	}
}

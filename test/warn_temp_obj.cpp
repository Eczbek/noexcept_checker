#include "../throws.hpp"

struct A {
	A() THROWS(int) {
		throw 0;
	}
};

void f() THROWS(int) {
	try {
		A();
	} catch (...) {
		throw;
	}
}

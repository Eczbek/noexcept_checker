void f() noexcept {
	try {
		throw 0;
	} catch (const int&) {

	} catch (...) {

	}
}

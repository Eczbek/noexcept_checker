void f() noexcept {
	try {
		throw 0;
	} catch (int) {

	}
}

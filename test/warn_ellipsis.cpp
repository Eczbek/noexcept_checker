void f() noexcept {
	try {
		throw 0;
		throw 0.f;
		throw 'a';
	} catch (...) {

	}
}

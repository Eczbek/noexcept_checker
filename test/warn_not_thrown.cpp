void f() noexcept {
	try {
		throw 0;
	} catch (char) {

	} catch (const int&) {

	}
}

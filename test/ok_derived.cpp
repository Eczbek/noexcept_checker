struct Base {};
struct Derived : Base {};

void h() noexcept try {
	throw Derived();
} catch (const Derived&) {
}

import os;

passed = True
for file, expected_lines in [
	["test/warn_duplicate.cpp", [
		"exception specifier of function 'f' contains duplicate entry 'int'"]],
	["test/warn_extra.cpp", [
		"exception specifier of function 'f' should not contain 'int'"]],
	["test/warn_missing.cpp", [
		"exception specifier of function 'f' should contain 'int' (from uncaught throw expression)"]],
	["test/warn_missing_propagate.cpp", [
		"exception specifier of function 'g' should contain 'int' (from uncaught call to function 'f')",
		"exception specifier of function 'f' should not contain 'int'"]]
]:
	lines = set(filter(lambda s: len(s) > 0, os.popen("clang++ -c -fplugin=build/noexcept_checker.so -Xclang -plugin -Xclang noexcept_checker -o/dev/null " + file + " 2>&1").read().split("\n")))
	if lines != set(expected_lines):
		print(file + ":\n" + "\n".join(lines) + "\n")
		passed = False

if passed:
	print("all tests passed")

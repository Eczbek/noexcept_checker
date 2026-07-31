import os;

passed = True
for file, expected_lines in [
	["test/ok_call.cpp", []],
	["test/ok_call2.cpp", []],
	["test/ok_ctor.cpp", []],
	["test/ok_derived.cpp", []],
	["test/warn_const_ref.cpp", [
		"test/warn_const_ref.cpp:4:4: caught type should be const reference: 'int'"]],
	["test/warn_ctor.cpp", [
		"test/warn_ctor.cpp:2:2: exception specifier should explicitly list uncaught type(s): 'int'"]],
	["test/warn_duplicate.cpp", [
		"test/warn_duplicate.cpp:3:1: exception specifier lists duplicate type: 'int'"]],
	["test/warn_ellipsis.cpp", [
		"test/warn_ellipsis.cpp:6:4: ellipsis catches 3 type(s): 'char', 'float', 'int'"]],
	["test/warn_ellipsis_none.cpp", [
		"test/warn_ellipsis_none.cpp:4:4: ellipsis catches no types"]],
	["test/warn_ellipsis_none2.cpp", [
		"test/warn_ellipsis_none2.cpp:6:4: ellipsis catches no types"]],
	["test/warn_extra.cpp", [
		"test/warn_extra.cpp:3:1: exception specifier lists caught or unthrown type: 'int'"]],
	["test/warn_lambda.cpp", [
		"test/warn_lambda.cpp:1:11: exception specifier should explicitly list uncaught type(s): 'int'"]],
	["test/warn_lambda_call.cpp", [
		"test/warn_lambda_call.cpp:7:1: exception specifier should explicitly list uncaught type(s): 'int'"]],
	["test/warn_member_func.cpp", [
		"test/warn_member_func.cpp:2:2: exception specifier should explicitly list uncaught type(s): 'int'"]],
	["test/warn_missing.cpp", [
		"test/warn_missing.cpp:3:1: exception specifier should explicitly list uncaught type(s): 'int'"]],
	["test/warn_missing_propagate.cpp", [
		"test/warn_missing_propagate.cpp:3:1: exception specifier lists caught or unthrown type: 'int'"]],
	["test/warn_none.cpp", [
		"test/warn_none.cpp:1:1: exception specifier should be noexcept"]],
	["test/warn_not_thrown.cpp", [
		"test/warn_not_thrown.cpp:4:4: caught type was not thrown: 'char'"]],
	["test/warn_rethrow.cpp", [
		"test/warn_rethrow.cpp:4:2: rethrow outside catch statement may terminate",
		"test/warn_rethrow.cpp:3:1: exception specifier should be noexcept",
		"test/warn_rethrow.cpp:7:1: exception specifier lists caught or unthrown type: 'int'"]],
	["test/warn_rethrow_unscoped.cpp", [
		"test/warn_rethrow_unscoped.cpp:1:10: rethrow outside catch statement may terminate"]],
	["test/warn_temp_obj.cpp", [
		"test/warn_temp_obj.cpp:12:4: ellipsis catches 1 type(s): 'int'"]],
	["test/warn_throw_unscoped.cpp", [
		"test/warn_throw_unscoped.cpp:1:10: throw in namespace scope"]]
]:
	lines = set(filter(lambda s: len(s) > 0, os.popen("clang++ -c -fplugin=build/noexcept_checker.so -Xclang -plugin -Xclang noexcept_checker -std=c++23 -w -o/dev/null " + file + " 2>&1").read().split("\n")))
	if lines != set(expected_lines):
		print("\n".join(lines) + "\n")
		passed = False

if passed:
	print("all tests passed")

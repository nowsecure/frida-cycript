filters += ObjectiveC
header += Struct.hpp ObjectiveC.hpp
code += ObjectiveC.o Library.o

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

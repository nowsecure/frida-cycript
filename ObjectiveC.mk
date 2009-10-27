filters += ObjectiveC
header += Struct.hpp ObjectiveC/Internal.hpp ObjectiveC/Syntax.hpp
code += ObjectiveC/Output.o ObjectiveC/Replace.o Library.o

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

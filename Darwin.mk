dll := dylib

# XXX: objective-c exists on non-Darwin

header += Struct.hpp ObjectiveC.hpp
code += ObjectiveC.o
filters += ObjC

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

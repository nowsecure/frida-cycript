dll := dylib

# XXX: objective-c exists on non-Darwin

header += Struct.hpp ObjectiveC.hpp
code += ObjectiveC.o Library.o
filters += ObjC
flags += -DCY_ATTACH -DCY_EXECUTE

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

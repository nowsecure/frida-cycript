# XXX: objective-c exists on non-Darwin

dll := dylib
header += Struct.hpp ObjectiveC.hpp
code += ObjectiveC.o Library.o
filters += ObjC
flags += -DCY_ATTACH -DCY_EXECUTE
flags += -lobjc -framework CoreFoundation
link += -framework Foundation
library += -install_name /usr/lib/libcycript.$(dll)
library += -framework Foundation -framework CFNetwork
library += -framework JavaScriptCore -framework WebCore

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

# XXX: objective-c exists on non-Darwin

dll := dylib
flags += -DCY_ATTACH -DCY_EXECUTE
link += -lobjc -framework CoreFoundation
console += -framework Foundation
library += -install_name /usr/lib/libcycript.$(dll)
library += -framework Foundation -framework CFNetwork
library += -framework JavaScriptCore -framework WebCore
library += -lsubstrate

include ObjectiveC.mk

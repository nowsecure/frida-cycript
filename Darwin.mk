dll := dylib
link += -lobjc -framework CoreFoundation
console += -framework Foundation
library += -install_name /usr/lib/libcycript.$(dll)
library += -framework Foundation
library += -framework JavaScriptCore
# XXX: do I just need WebCore?
library += -framework WebKit
library += -liconv

include Execute.mk
include ObjectiveC.mk

export PATH := /usr/local/bin:$(PATH)

ifneq ($(shell pkg-config webkit-1.0 --modversion 2>/dev/null),)
flags += $(shell pkg-config --cflags webkit-1.0)
library += -lwebkit-1.0
include Execute.mk
else
ifneq ($(shell pkg-config WebKitGtk --modversion 2>/dev/null),)
flags += $(shell pkg-config --cflags WebKitGtk)
library += $(shell pkg-config --libs WebKitGtk)
include Execute.mk
endif
endif

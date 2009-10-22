objc += $(shell gnustep-config --objc-flags)
link += $(shell gnustep-config --base-libs)
include ObjectiveC.mk

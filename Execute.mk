CY_EXECUTE := 1
flags += -DCY_EXECUTE
code += sig/ffi_type.o sig/parse.o sig/copy.o
code += Execute.o Bridge.o
library += $(apr) -lffi
filters += C

Bridge.gperf: Bridge.def Bridge.sh
	./Bridge.sh Bridge.def >Bridge.gperf

Bridge.hpp: Bridge.gperf
	gperf $< | sed -e 's/defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__/0/' >$@

Bridge.o: Bridge.hpp

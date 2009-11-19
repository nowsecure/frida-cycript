CY_EXECUTE :=
flags += -DCY_EXECUTE
code += sig/ffi_type.o sig/parse.o sig/copy.o
code += Execute.o Bridge.o
library += $(apr) -lffi
filters += C

Bridge.gperf: Bridge.def Bridge.sh
	./Bridge.sh Bridge.def >Bridge.gperf

Bridge.hpp: Bridge.gperf
	gperf $< >$@

Bridge.o: Bridge.hpp

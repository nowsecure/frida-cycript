export PATH := /usr/local/bin:$(PATH)
flags += -I/usr/include/webkit-1.0
depends += libffi4 libreadline5
include GNUstep.mk

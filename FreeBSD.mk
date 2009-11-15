export PATH := /usr/local/bin:$(PATH)
include Execute.mk
flags += -I/usr/local/include -I/usr/local/include/webkit-1.0
library += -lwebkit-1.0

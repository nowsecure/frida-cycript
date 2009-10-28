export PATH := /usr/local/bin:$(PATH)
flags += -I/usr/include/webkit-1.0
flags += -DCY_EXECUTE
depends += libffi4 libreadline5
library += -lwebkit-1.0

CY_EXECUTE :=
flags += -DCY_EXECUTE
code += sig/ffi_type.o sig/parse.o sig/copy.o
code += Execute.o
library += $(apr) -lffi -lsqlite3
all += libcycript.db
filters += C

extra::
	cp -pR libcycript.db package/usr/lib

libcycript.db: Bridge.def
	rm -f libcycript.db
	{ \
	    echo 'create table "bridge" ("mode" int not null, "name" text not null, "value" text null);'; \
	    grep '^[CFV]' Bridge.def | sed -e 's/^C/0/;s/^F/1/;s/^V/2/' | sed -e 's/"/\\"/g;s/^\([^ ]*\) \([^ ]*\) \(.*\)$$/insert into "bridge" ("mode", "name", "value") values (\1, '"'"'\2'"'"', '"'"'\3'"'"');/'; \
	    grep '^:' Bridge.def | sed -e 's/^: \([^ ]*\) \(.*\)/insert into "bridge" ("mode", "name", "value") values (-1, '"'"'\1'"'"', '"'"'\2'"'"');/'; \
	    grep '^[EST]' Bridge.def | sed -e 's/^S/3/;s/^T/4/;s/^E/5/' | sed -e 's/^5\(.*\)$$/4\1 i/' | sed -e 's/^\([^ ]*\) \([^ ]*\) \(.*\)$$/insert into "bridge" ("mode", "name", "value") values (\1, '"'"'\2'"'"', '"'"'\3'"'"');/'; \
	} | tee libcycript.sql | sqlite3 libcycript.db

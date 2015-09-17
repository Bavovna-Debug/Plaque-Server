MODULES = vp_dispatcher
PG_CONFIG = /usr/pgsql-9.4/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

vp_dispatcher.so: main.o apns.o
	$(CC) -shared -pthread -L/usr/pgsql-9.4/lib/ -o $@ $^ -lpq

%.o: %.c
	$(CC) -c $(CFLAGS) -fPIC -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/ $< -o $@

install: all
	cp vp_dispatcher.so /opt/vp/

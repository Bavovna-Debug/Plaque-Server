CFLAGS += -Wall

MODULES = vp_scheduler

PG_CONFIG := /usr/pgsql-9.4/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

INCLUDE += -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/ -I../lib/
INCLUDE += -I../lib/

DEFINES += -D PGBGW

include $(PGXS)

vp_scheduler.so: main.o kernel.o
	$(CC) -shared -pthread -lpq -L/usr/pgsql-9.4/lib/ -o $@ $^

main.o: main.c kernel.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

kernel.o: kernel.c kernel.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

install: vp_scheduler.so
	install vp_scheduler.so /opt/vp/

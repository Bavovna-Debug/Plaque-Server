MODULES = vp_broadcaster
PG_CONFIG = /usr/pgsql-9.4/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
INCLUDE := -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/ -I../lib/ -I../api/
DEFINES := -D PGBGW -D BUFFERS_INIT_BANK -D BUFFERS_PEEK_POKE

include $(PGXS)

vp_broadcaster.so: main.o kernel.o listener.o
	$(CC) -shared -pthread -lpq -L/usr/pgsql-9.4/lib/ -o $@ $^

main.o: main.c desk.h kernel.h listener.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

listener.o: listener.c desk.h listener.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

kernel.o: kernel.c desk.h kernel.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

install: vp_broadcaster.so
	install vp_broadcaster.so /opt/vp/
#	install vp_broadcaster.so /opt/vp/broadcaster.so
#	install broadcaster.control /opt/vp/broadcaster.control
#	install broadcaster.sql /opt/vp/broadcaster.sql

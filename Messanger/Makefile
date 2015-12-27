MODULES = vp_messanger
PG_CONFIG = /usr/pgsql-9.4/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
INCLUDE := -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/ -I../lib/
DEFINES := -D PGBGW -D BUFFERS_INIT_BANK -D BUFFERS_PEEK_POKE -D APNS_XMIT

include $(PGXS)

vp_messanger.so: main.o apns.o buffers.o notification.o
	$(CC) -shared -pthread -lpq -lssl -lcrypto -L/usr/pgsql-9.4/lib/ -o $@ $^
	#-Wl,--unresolved-symbols=report-all

main.o: main.c apns.h desk.h notification.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

apns.o: apns.c apns.h desk.h notification.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

notification.o: notification.c notification.h desk.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

buffers.o: ../lib/buffers.c ../lib/buffers.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) ../lib/buffers.c -o buffers.o

install: all
	cp vp_messanger.so /opt/vp/

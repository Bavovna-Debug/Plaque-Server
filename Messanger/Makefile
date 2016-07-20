CFLAGS += -Wall

MODULES = vp_messanger

PG_CONFIG := /usr/pgsql-9.4/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

INCLUDE += -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/
INCLUDE += -I../lib/

DEFINES += -D PGBGW
DEFINES += -D BUFFERS_INIT_BANK -D BUFFERS_PEEK_POKE
DEFINES += -D APNS_XMIT

include $(PGXS)

vp_messanger.so: main.o apns.o apns_thread.o chalkboard.o mmps.o notification.o
	$(CC) -shared -pthread -lpq -lssl -lcrypto -L/usr/pgsql-9.4/lib/ -o $@ $^
	#-Wl,--unresolved-symbols=report-all

main.o: main.c apns.h chalkboard.h notification.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

apns.o: apns.c apns.h chalkboard.h notification.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

apns_thread.o: apns_thread.c apns.h apns_thread.h chalkboard.h notification.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

chalkboard.o: chalkboard.c apns.h chalkboard.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

notification.o: notification.c notification.h chalkboard.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) $< -o $@

mmps.o: ../lib/mmps.c ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) ../lib/mmps.c -o mmps.o

install: all
	cp vp_messanger.so /opt/vp/

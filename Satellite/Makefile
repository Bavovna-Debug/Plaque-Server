INCLUDE := -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/ -I../lib/ -I../api/
DEFINES := -D NETWORK_RECEIVE -D NETWORK_SEND -D DB_PEEK_POKE -D STATISTICS -D TASKS -D TASK_THREAD -D ANTICIPANT_ALL

all: vp_satellite

vp_satellite: main.o anticipant.o broadcaster.o buffers.o db.o listener.o paquet.o paquet_broadcast.o plaques_edit.o plaques_query.o profiles.o session.o tasks.o task_kernel.o task_list.o task_xmit.o
	$(CC) -pthread -L/usr/pgsql-9.4/lib/ -o $@ $^ -lpq

main.o: main.c broadcaster.h db.h desk.h listener.h paquet.h session.h tasks.h task_list.h ../lib/buffers.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

anticipant.o: anticipant.c api.h anticipant.h db.h paquet.h tasks.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

broadcaster.o: broadcaster.c broadcaster.h desk.h tasks.h task_list.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

db.o: db.c db.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

listener.o: listener.c desk.h listener.h tasks.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

paquet.o: paquet.c api.h db.h paquet.h paquet_broadcast.h tasks.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

paquet_broadcast.o: paquet_broadcast.c api.h db.h paquet.h paquet_broadcast.h tasks.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

plaques_query.o: plaques_query.c api.h db.h desk.h paquet.h plaques_query.h tasks.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

plaques_edit.o: plaques_edit.c api.h db.h paquet.h plaques_edit.h tasks.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

profiles.o: profiles.c api.h db.h desk.h paquet.h tasks.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

session.o: session.c api.h db.h tasks.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

tasks.o: tasks.c desk.h paquet.h tasks.h task_kernel.h task_xmit.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

task_kernel.o: task_kernel.c anticipant.h api.h desk.h paquet.h plaques_edit.h plaques_query.h profiles.h session.h tasks.h task_kernel.h task_xmit.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

task_list.o: task_list.c desk.h tasks.h task_list.h ../lib/buffers.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

task_xmit.o: task_xmit.c desk.h paquet.h tasks.h task_kernel.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

buffers.o: ../lib/buffers.c ../lib/buffers.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) ../lib/buffers.c -o buffers.o

install: all
	cp vp_satellite /opt/vp/

clean:
	rm *.o vp_satellite

xfer:
	scp Makefile *.c *.h aquarium:satellite/

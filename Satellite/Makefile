CFLAGS += -Wall

INCLUDE := -I/usr/pgsql-9.4/include/ -I/usr/pgsql-9.4/include/server/ -I../lib/ -I../api/

#DEFINES += -D BUFFERS_INIT_BANK
#DEFINES += -D BUFFERS_PEEK_POKE
DEFINES += -D SYSLOG
#DEFINES += -D DUPLEX
DEFINES += -D NETWORK_RECEIVE -D NETWORK_SEND
DEFINES += -D DB_PEEK_POKE
DEFINES += -D STATISTICS
DEFINES += -D TASKS
DEFINES += -D TASK_THREAD
DEFINES += -D ANTICIPANT_DIALOGUE
DEFINES += -D ANTICIPANT_DIALOGUE_REGULAR
DEFINES += -D ANTICIPANT_DIALOGUE_AUTH

all: vp_satellite

vp_satellite: main.o anticipant.o broadcaster.o chalkboard.o db.o listener.o mmps.o paquet.o paquet_broadcast.o paquet_displacement.o plaques_edit.o plaques_query.o profiles.o reports.o session.o tasks.o task_kernel.o task_list.o task_xmit.o
	$(CC) -pthread -L/usr/pgsql-9.4/lib/ -o $@ $^ -lpq

main.o: main.c broadcaster.h db.h chalkboard.h listener.h paquet.h session.h tasks.h task_list.h ../lib/mmps.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

anticipant.o: anticipant.c api.h anticipant.h db.h paquet.h tasks.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

chalkboard.o: chalkboard.c chalkboard.h db.h paquet.h tasks.h ../api/broadcaster_api.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

broadcaster.o: broadcaster.c broadcaster.h chalkboard.h tasks.h task_list.h ../api/broadcaster_api.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

db.o: db.c db.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

listener.o: listener.c chalkboard.h listener.h tasks.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

paquet.o: paquet.c anticipant.h api.h db.h paquet.h paquet_broadcast.h paquet_displacement.h plaques_edit.h plaques_query.h profiles.h reports.h tasks.h task_xmit.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

paquet_broadcast.o: paquet_broadcast.c api.h chalkboard.h db.h paquet.h paquet_broadcast.h session.h tasks.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

paquet_displacement.o: paquet_displacement.c chalkboard.h db.h paquet.h paquet_displacement.h tasks.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

plaques_query.o: plaques_query.c api.h chalkboard.h db.h paquet.h plaques_query.h tasks.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

plaques_edit.o: plaques_edit.c api.h chalkboard.h db.h paquet.h plaques_edit.h tasks.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

profiles.o: profiles.c api.h chalkboard.h db.h paquet.h tasks.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

reports.o: reports.c api.h chalkboard.h db.h paquet.h reports.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

session.o: session.c api.h chalkboard.h db.h tasks.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

tasks.o: tasks.c api.h chalkboard.h paquet.h tasks.h task_kernel.h task_xmit.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

task_kernel.o: task_kernel.c anticipant.h api.h chalkboard.h paquet.h plaques_edit.h plaques_query.h profiles.h session.h tasks.h task_kernel.h task_xmit.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

task_list.o: task_list.c chalkboard.h tasks.h task_list.h ../lib/mmps.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

task_xmit.o: task_xmit.c chalkboard.h paquet.h tasks.h task_kernel.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(DEFINES) $< -o $@

mmps.o: ../lib/mmps.c ../lib/mmps.h
	$(CC) -c $(CFLAGS) -fPIC $(INCLUDE) $(DEFINES) ../lib/mmps.c -o mmps.o

install: all
	cp vp_satellite /opt/vp/

clean:
	rm -fv *.o vp_satellite

xfer:
	scp Makefile *.c *.h aquarium:satellite/

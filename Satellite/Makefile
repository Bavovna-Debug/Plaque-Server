all: vp_satellite

vp_satellite: main.o db.o buffers.o tasks.o paquet.o session.o profiles.o anticipant.o plaques_query.o plaques_edit.o
	$(CC) -pthread -L/usr/pgsql-9.4/lib/ -o $@ $^ -lpq

%.o: %.c
	$(CC) -c $(CFLAGS) -I/usr/pgsql-9.4/include/ $< -o $@

install: all
	cp vp_satellite /opt/vp/

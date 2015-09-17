all: server

server: main.o db.o buffers.o queue.o tasks.o bonjour.o anticipant.o plaques.o
	$(CC) -pthread -o $@ $^ -I/usr/include/postgresql/9.3/server/ -L/usr/local/pgsql/lib -lpq

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

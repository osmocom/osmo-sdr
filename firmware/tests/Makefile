CFLAGS=-I../include/ -O0 -g

all: tuner-test cmd-test

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

tuner-test: tuner-test.o ../src/tuner_e4k.o
	$(CC) $(LDFLAGS) -o $@ $^

cmd-test: cmd-test.o ../src/uart_cmd.o
	$(CC) $(LDFLAGS) -o $@ $^

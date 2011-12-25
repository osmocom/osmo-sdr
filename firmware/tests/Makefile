CFLAGS=-I../include/ -O2

all: tuner-test

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

tuner-test: tuner-test.o ../src/tuner_e4k.o
	$(CC) $(LDFLAGS) -o $@ $^

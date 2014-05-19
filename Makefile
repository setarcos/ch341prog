CFLAGS=-std=gnu99 -Wall
ch341prog: main.c ch341a.c
	gcc $(CFLAGS) ch341a.c main.c -o ch341prog  -lusb-1.0

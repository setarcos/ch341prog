CFLAGS=-std=gnu99 -Wall
ch341prog: main.c ch341a.c ch341a.h
	gcc $(CFLAGS) ch341a.c main.c -o ch341prog  -lusb-1.0
clean:
	rm *.o ch341prog -f
install-udev-rule:
	cp 99-ch341a-prog.rules /etc/udev/rules.d/
	udevadm control --reload-rules
.PHONY: clean install-udev-rule

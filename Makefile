CFLAGS = -Wall -O2
LIBS = -lusb -lutil

all: usb-as296-to-tty-d

usb-as296-to-tty-d: as296_lib.o as296_main.o
	$(CC) $(CFLAGS) -o $@ as296_lib.o as296_main.o $(LIBS)

install:
	install -m 755 $(CURDIR)/usb-as296-to-tty-d /usr/sbin/
	install -m 644 $(CURDIR)/aatis-modem.default /etc/default/aatis-modem
	install -m 744 $(CURDIR)/aatis-modem /etc/init.d/

install-bin:
	install -m 755 $(CURDIR)/usb-as296-to-tty-d /usr/sbin/


clean:
	-rm  *.o usb-as296-to-tty-d

.PHONY: all clean

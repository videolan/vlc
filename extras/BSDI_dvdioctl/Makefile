all: libdvd.a libdvd.so blkreset

blkreset: libdvd.a blkreset.c
	cc -O2 -g -o blkreset blkreset.c libdvd.a

dvdioctl.o: dvdioctl.c dvd.h
	cc -O2 -g -c -I. dvdioctl.c

dvdioctl.po: dvdioctl.c dvd.h
	cc -O2 -fPIC -DPIC -g -c -o dvdioctl.po -I. dvdioctl.c

libdvd.a: dvdioctl.o
	ar rcv libdvd.a dvdioctl.o

libdvd.so: dvdioctl.po
	cc -shared -o libdvd.so dvdioctl.po

install: libdvd.a libdvd.so blkreset
	install -c -m 644 libdvd.a /usr/local/lib/libdvd.a
	install -c -m 755 libdvd.so /usr/local/lib/libdvd.so
	ranlib /usr/local/lib/libdvd.a
	install -c -m 644 dvd.h /usr/local/include/dvd.h
	install -c -m 755 blkreset /usr/local/bin/blkreset

clean:
	rm -f *.a *.o *.so *.po blkreset

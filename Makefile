VERSION=0.3

all: xmacroplay xmacrorec xmacrorec2

xmacroplay: xmacroplay.cpp chartbl.h
	g++ -O2  -I/usr/X11R6/include -Wall -pedantic -DVERSION=$(VERSION) xmacroplay.cpp -o xmacroplay -L/usr/X11R6/lib -lXtst -lX11

xmacrorec: xmacrorec.cpp
	g++ -O2  -I/usr/X11R6/include -Wall -pedantic -DVERSION=$(VERSION) xmacrorec.cpp -o xmacrorec -L/usr/X11R6/lib -lXtst -lX11

xmacrorec2: xmacrorec2.cpp
	g++ -O2  -I/usr/X11R6/include -Wall -pedantic -DVERSION=$(VERSION) xmacrorec2.cpp -o xmacrorec2 -L/usr/X11R6/lib -lXtst -lX11

clean:
	rm xmacrorec xmacroplay xmacrorec2

deb:
	umask 022 && epm -f deb -nsm xmacro

rpm:
	umask 022 && epm -f rpm -nsm xmacro

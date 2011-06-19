CC = gcc
CFLAGS = -w -Os -s	#size
#CFLAGS = -w -O3 -s
#CFLAGS = -w -g		#debug
#CFLAGS = -w
OBJECTS = opgui.o deviceRW.o progP12.o progP16.o progP18.o progP24.o progEEPROM.o progAVR.o fileIO.o I2CSPI.o
GTKFLAGS = `pkg-config --libs --cflags gtk+-2.0`

all: opgui

opgui : $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o opgui $(GTKFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) $(GTKFLAGS) -c $<

clean:
	rm -f opgui $(OBJECTS)


.PHONY: clean

prefix	:= /usr/local

install: opgui
	test -d $(prefix) || mkdir $(prefix)
	test -d $(prefix)/bin || mkdir $(prefix)/bin
	install -m 0755 opgui $(prefix)/bin;
	
.PHONY: install

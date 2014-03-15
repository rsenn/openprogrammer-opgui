CC = gcc
CFLAGS = -Wall -Os -s #size
#CFLAGS = -w -O3 -s
#CFLAGS = -w -g		#debug
OBJECTS = opgui.o \
	deviceRW.o \
	progP12.o \
	progP16.o \
	progP18.o \
	progP24.o \
	progEEPROM.o \
	progAVR.o \
	fileIO.o \
	I2CSPI.o \
	coff.o \
	icd.o \
	strings.o \
	icons.o
GTKFLAGS = `pkg-config --libs --cflags gtk+-2.0`
UNAME := $(shell uname)
ifneq (, $(findstring _NT-, $(UNAME)))
	OPFLAG = -mwindows
else
	OPFLAG = 
endif
	

all: opgui

opgui : $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(GTKFLAGS) -o opgui $(OPFLAG)

%.o : %.c
	$(CC) $(CFLAGS) $(GTKFLAGS) -c $<

icons.c : write.png read.png sys.png
	echo "#include <gtk/gtk.h>" > icons.c
	gdk-pixbuf-csource --extern --build-list write_icon write.png read_icon read.png \
	system_icon sys.png go_icon go.png halt_icon halt.png step_icon step.png \
	stepover_icon stepover.png stop_icon stop.png >> icons.c

clean:
	rm -f $(OBJECTS) icons.c
	
.PHONY: clean

prefix	:= /usr/local

install: all
	test -d $(prefix) || mkdir $(prefix)
	test -d $(prefix)/bin || mkdir $(prefix)/bin
	install -m 0755 opgui $(prefix)/bin;

.PHONY: install

package:
	tar -cvzf opgui.tar.gz ../opgui/*.c ../opgui/*.h ../opgui/gpl-2.0.txt ../opgui/Makefile ../opgui/readme ../opgui/*.png


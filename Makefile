VERSION = 0.11.0# equivalent to #define in c code
CC = gcc
PREFIX = /usr/local
CFLAGS =  '-DVERSION="$(VERSION)"' -w `pkg-config --libs --cflags gtk+-2.0` 
CFLAGS += -Os -s #size
#CFLAGS += -O3 -s #speed
#CFLAGS += -g #debug

OBJECTS = opgui.o \
	deviceRW.o \
	progP12.o \
	progP16.o \
	progP18.o \
	progP24.o \
	progP32.o \
	progEEPROM.o \
	progAVR.o \
	fileIO.o \
	I2CSPI.o \
	coff.o \
	icd.o \
	strings.o \
	icons.o

# Check if we are running on windows
UNAME := $(shell uname)
ifneq (, $(findstring _NT-, $(UNAME)))
	CFLAGS += -mwindows
else
	CFLAGS += -lrt
endif
	

# Targets
all: opgui

opgui : $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(CFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

icons.c : write.png read.png sys.png
	echo "#include <gtk/gtk.h>" > icons.c
	gdk-pixbuf-csource --extern --build-list write_icon write.png read_icon read.png \
	system_icon sys.png go_icon go.png halt_icon halt.png step_icon step.png \
	stepover_icon stepover.png stop_icon stop.png >> icons.c

clean:
	rm -f opgui $(OBJECTS) icons.c
	
install: all
	#test -d $(prefix) || mkdir $(prefix)
	#test -d $(prefix)/bin || mkdir $(prefix)/bin
	@echo "Installing opgui"
	mkdir -p $(PREFIX)/bin
	install -m 0755 opgui $(PREFIX)/bin;

package:
	@echo "Creating opgui_$(VERSION).tar.gz"
	@mkdir opgui-$(VERSION)
	@cp *.c *.h *.png gpl-2.0.txt Makefile readme opgui-$(VERSION)
	@tar -czf opgui-$(VERSION).tar.gz opgui-$(VERSION)
	@rm -rf opgui-$(VERSION)

.PHONY: all clean install package

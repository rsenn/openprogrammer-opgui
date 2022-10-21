# equivalent to #define in c code
VERSION = 0.12.0
CC = gcc
PREFIX = /usr/local

CFLAGS =  '-DVERSION="$(VERSION)"' `pkg-config --libs --cflags gtk+-3.0` 
CFLAGS += -Os -s #size
#CFLAGS += -O3 -s #speed
#CFLAGS += -g #debug

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
	resources.o
#	progP32.o \

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

resources.c: resources.xml opgui.glade
	glib-compile-resources resources.xml --target=resources.c --generate-source

clean:
	rm -f opgui $(OBJECTS) resources.c
	
install: all
	#test -d $(prefix) || mkdir $(prefix)
	#test -d $(prefix)/bin || mkdir $(prefix)/bin
	@echo "Installing opgui"
	mkdir -p $(PREFIX)/bin
	install -m 0755 opgui $(PREFIX)/bin;
	
package:
	@echo "Creating opgui_$(VERSION).tar.gz"
	@mkdir opgui-$(VERSION)
	@cp *.c *.h *.png gpl-2.0.txt Makefile readme resources.xml opgui.glade style.css opgui-$(VERSION)
	@tar -czf opgui_$(VERSION).tar.gz opgui-$(VERSION)
	@rm -rf opgui-$(VERSION)

.PHONY: all clean install package

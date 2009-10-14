
#
# MinGW makefile
#

CC=gcc
CFLAGS=-Wall

.SUFFIXES:

default: seamlessrdp.zip

seamlessrdp.zip: seamlessrdpshell.exe seamlessrdp.dll vchannel.dll
	-rm $@
	zip $@ $^

seamlessrdpshell.exe: main.c vchannel.dll
	$(CC) $(CFLAGS) -mwindows -o $@ $^ -lwtsapi32

vchannel.dll: vchannel.c
	$(CC) $(CFLAGS) -shared -o $@ $^ -lwtsapi32

seamlessrdp.dll: hookdll.c vchannel.dll
	$(CC) $(CFLAGS) -shared -o $@ $^ -lgdi32

clean:
	-rm -f *.o
	-rm -f seamlessrdpshell.exe vchannel.dll seamlessrdp.dll seamlessrdp.zip

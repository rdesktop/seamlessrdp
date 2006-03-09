
#
# MinGW makefile
#

CC=gcc
CFLAGS=-Wall

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/hookdll.dll

seamlessrdpshell.exe: main.c vchannel.o
	$(CC) $(CFLAGS) -mwindows -o $@ $^ -lwtsapi32

vchannel.o: vchannel.c vchannel.h
	$(CC) $(CFLAGS) -c -o $@ vchannel.c

HookDll/hookdll.dll: HookDll/hookdll.c vchannel.o
	$(CC) $(CFLAGS) -shared -o $@ $^ -lwtsapi32

clean:
	-rm -f *.o HookDll/*.o
	-rm -f seamlessrdpshell.exe HookDll/hookdll.dll

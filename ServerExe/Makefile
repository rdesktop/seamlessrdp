
#
# MinGW makefile
#

CC=gcc
CFLAGS=-Wall

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/seamlessrdp.dll

seamlessrdpshell.exe: main.c vchannel.dll
	$(CC) $(CFLAGS) -mwindows -o $@ $^ -lwtsapi32

vchannel.dll: vchannel.c
	$(CC) $(CFLAGS) -shared -o $@ $^ -lwtsapi32

HookDll/seamlessrdp.dll: HookDll/hookdll.c vchannel.dll
	$(CC) $(CFLAGS) -shared -o $@ $^

clean:
	-rm -f *.o HookDll/*.o
	-rm -f seamlessrdpshell.exe vchannel.dll HookDll/seamlessrdp.dll

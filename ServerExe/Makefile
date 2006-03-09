
#
# MinGW makefile
#

CC=gcc
CFLAGS=-Wall

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/hookdll.dll

seamlessrdpshell.exe: main.c
	$(CC) $(CFLAGS) -mwindows -o $@ $^

HookDll/hookdll.dll: HookDll/hookdll.c
	$(CC) $(CFLAGS) -shared -o $@ $^ -lwtsapi32

clean:
	-rm -f *.o HookDll/*.o
	-rm -f seamlessrdpshell.exe HookDll/hookdll.dll

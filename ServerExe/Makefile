
#
# MinGW makefile
#

CC=gcc
CFLAGS=-Wall

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/hookdll.dll

seamlessrdpshell.exe: main.c
	$(CC) $(CFLAGS) -o $@ $^

HookDll/hookdll.dll: HookDll/hookdll.c
	$(CC) $(CFLAGS) -shared -o $@ $^ -lwtsapi32

clean:
	-rm *.exe *.o *.dll HookDll/*.exe HookDll/*.o HookDll/*.dll

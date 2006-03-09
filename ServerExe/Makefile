
#
# MinGW makefile
#

CC=gcc

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/hookdll.dll

seamlessrdpshell.exe: main.c
	$(CC) -o $@ $^

HookDll/hookdll.dll: HookDll/hookdll.c
	$(CC) -shared -o $@ $^ -lwtsapi32

clean:
	-rm *.exe *.o *.dll HookDll/*.exe HookDll/*.o HookDll/*.dll


#
# MinGW makefile
#

CXX=g++

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/hookdll.dll

HookDll/hook.o: HookDll/hook.cpp HookDll/hook.h
	$(CXX) -c -o $@ $< 

seamlessrdpshell.exe: main.cpp HookDll/hook.o
	$(CXX) -o $@ $^ -lgdi32

HookDll/hookdll.dll: HookDll/hookdll.cpp
	$(CXX) -shared -o $@ $^ -lwtsapi32

clean:
	-rm *.exe *.o *.dll HookDll/*.exe HookDll/*.o HookDll/*.dll

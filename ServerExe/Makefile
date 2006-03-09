
#
# MinGW makefile
#

CXX=g++

.SUFFIXES:

default: seamlessrdpshell.exe HookDll/hookdll.dll

seamlessrdpshell.exe: main.cpp
	$(CXX) -o $@ $^

HookDll/hookdll.dll: HookDll/hookdll.cpp
	$(CXX) -shared -o $@ $^ -lwtsapi32

clean:
	-rm *.exe *.o *.dll HookDll/*.exe HookDll/*.o HookDll/*.dll

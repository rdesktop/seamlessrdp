bin_PROGRAMS = seamlessrdpshell

noinst_LTLIBRARIES = libvchannel.la
libvchannel_la_SOURCES = vchannel.c
libvchannel_la_LIBADD = -lwtsapi32

seamlessrdpshell_SOURCES = main.c
seamlessrdpshell_LDADD = libvchannel.la -lwtsapi32
seamlessrdpshell_LDFLAGS = -mwindows

if OS_IS_WIN32
bin_PROGRAMS += seamlessrdphook32
seamlessrdphook32_SOURCES = seamlessrdphook.c
seamlessrdphook32_LDADD =
seamlessrdphook32_LDFLAGS = -mwindows
endif

# seamlessrdphook64 is not used in practice, but built for completeness
if OS_IS_WIN64
bin_PROGRAMS += seamlessrdphook64
seamlessrdphook64_SOURCES = seamlessrdphook.c
seamlessrdphook64_LDADD =
seamlessrdphook64_LDFLAGS = -mwindows
endif

if OS_IS_WIN32
lib_LTLIBRARIES = seamlessrdp32.la
seamlessrdp32_la_SOURCES = hookdll.c
seamlessrdp32_la_LDFLAGS = -avoid-version -module -no-undefined 
seamlessrdp32_la_LIBADD = libvchannel.la -lgdi32
endif

if OS_IS_WIN64
lib_LTLIBRARIES = seamlessrdp64.la
seamlessrdp64_la_SOURCES = hookdll.c
seamlessrdp64_la_LDFLAGS = -avoid-version -module -no-undefined 
seamlessrdp64_la_LIBADD = libvchannel.la -lgdi32
endif

bin_PROGRAMS = seamlessrdpshell

lib_LTLIBRARIES = \
	seamlessrdp.la

noinst_LTLIBRARIES = libvchannel.la
libvchannel_la_SOURCES = vchannel.c
libvchannel_la_LIBADD = -lwtsapi32

seamlessrdpshell_SOURCES = main.c
seamlessrdpshell_LDADD = libvchannel.la -lwtsapi32
seamlessrdpshell_LDFLAGS = -mwindows

seamlessrdp_la_SOURCES = hookdll.c
seamlessrdp_la_LDFLAGS = -avoid-version -module -no-undefined 
seamlessrdp_la_LIBADD = libvchannel.la -lgdi32

bin_PROGRAMS = seamlessrdpshell

lib_LTLIBRARIES = \
	vchannel.la \
	seamlessrdp.la

seamlessrdpshell_SOURCES = main.c
seamlessrdpshell_LDADD = vchannel.la

vchannel_la_SOURCES = vchannel.c vchannel.h
vchannel_la_LDFLAGS = -avoid-version -module -no-undefined -lwtsapi32

seamlessrdp_la_SOURCES = hookdll.c
seamlessrdp_la_LIBADD = vchannel.la
seamlessrdp_la_LDFLAGS = -avoid-version -module -no-undefined -lgdi32


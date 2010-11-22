bin_PROGRAMS = taucmd
taucmd_SOURCES = taucmd.c tau-utils.c
taucmd_LDADD = $(top_builddir)/src/.libs/libtau.a

lib_LTLIBRARIES = libtau.la

libtau_la_SOURCES = libtau.c
libtau_la_LDFLAGS = -release @PACKAGE_VERSION@ -version-info 1:0:0

include_HEADERS = tau.h tau-utils.h

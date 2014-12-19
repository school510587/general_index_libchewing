INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/internal \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/porting_layer/include \
	-I$(top_srcdir)/src/tools \
	$(NULL)

CC = $(CC_FOR_BUILD)
if MULTI_IM
CFLAGS_FOR_BUILD = -DSUPPORT_MULTI_IM
endif
AM_CFLAGS = $(CFLAGS_FOR_BUILD)

if MULTI_IM
noinst_PROGRAMS = init_database gen_IM_data dump_database

gen_IM_data_SOURCES = \
	gen_IM_data.c \
	build_tool.c \
	$(top_srcdir)/src/common/chewing-utf8-util.c \
	$(top_srcdir)/src/common/key2pho.c \
	$(top_srcdir)/src/porting_layer/src/plat_mmap_posix.c \
	$(top_srcdir)/src/porting_layer/src/plat_mmap_windows.c \
	$(NULL)

else
noinst_PROGRAMS = init_database dump_database
endif

init_database_SOURCES = \
	init_database.c \
	build_tool.c \
	$(top_srcdir)/src/common/chewing-utf8-util.c \
	$(top_srcdir)/src/common/key2pho.c \
	$(NULL)

dump_database_SOURCES = \
	dump_database.c \
	$(NULL)

all: $(noinst_PROGRAMS)
	-mv -f gen_IM_data $(top_builddir)/data

if MULTI_IM
CLEANFILES = $(top_builddir)/data/gen_IM_data
endif

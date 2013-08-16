INCLUDES = \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/internal \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/porting_layer/include \
	$(NULL)

CC = $(CC_FOR_BUILD)
AM_CFLAGS = $(CFLAGS_FOR_BUILD)

noinst_PROGRAMS = init_database

init_database_SOURCES = \
	init_database.c \
	build_tool.c \
	$(NULL)

init_database_LDADD = \
	$(top_srcdir)/src/common/libcommon.la

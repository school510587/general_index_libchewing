AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/internal \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/porting_layer/include \
	-I$(top_srcdir)/src/tools \
	-I$(top_srcdir)/thirdparty/sqlite-amalgamation \
	$(DEFAULT_CPPFLAGS) \
	$(NULL)

CC = $(CC_FOR_BUILD)
if SUPPORT_MULTI_IM
	AM_CFLAGS = $(CFLAGS_FOR_BUILD) -DSUPPORT_MULTI_IM
else
	AM_CFLAGS = $(CFLAGS_FOR_BUILD)
endif

noinst_PROGRAMS = init_database

init_database_SOURCES = \
	init_database.c \
	build-tool.c \
	$(top_srcdir)/src/common/chewing-utf8-util.c \
	$(top_srcdir)/src/common/key2pho.c \
	$(NULL)

tooldir = $(top_builddir)/src/tools
datas = \
	dictionary.dat \
	index_tree.dat \
	$(NULL)
static_tables = pinyin.tab swkb.dat symbols.dat

dist_noinst_DATA = \
	NOTE \
	phone.cin \
	phone.cin-CNS11643-complete.patch \
	svnrev \
	tsi.src \
	$(NULL)

chewing_datadir = $(pkglibdir)
chewing_data_DATA = \
	$(static_tables) \
	$(datas) \
	$(NULL)

all: $(datas)

$(datas): gendata_stamp

gendata_stamp: phone.cin tsi.src
	$(MAKE) gendata && \
	touch $@

gendata:
	env LC_ALL=C $(tooldir)/init_database$(EXEEXT) $(top_srcdir)/data/phone.cin $(top_srcdir)/data/tsi.src
INCLUDES = \
	-I$(tooldir) \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/include/internal \
        -I$(top_srcdir)/src \
        -I$(top_srcdir)/src/porting_layer/include \
        $(NULL)
CC = $(CC_FOR_BUILD)
AM_CFLAGS = $(CFLAGS_FOR_BUILD)
noinst_PROGRAMS = gen_IM_data
gen_IM_data_SOURCES = \
        $(tooldir)/gen_IM_data.c \
	$(tooldir)/build_tool.c \
        $(top_srcdir)/src/common/chewing-utf8-util.c \
        $(top_srcdir)/src/common/key2pho.c \
        $(NULL)

CLEANFILES = $(datas) gendata_stamp gen_IM_data

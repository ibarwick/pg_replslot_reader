# Makefile for replslot_reader
#
# Copyright
#

PGFILEDESC = "pg_replslot_reader - "

PROGRAM = pg_replslot_reader
OBJS	= pg_replslot_reader.o

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

# CPPFLAGS += -DFRONTEND
override CPPFLAGS := -DFRONTEND $(CPPFLAGS)

EXTRA_CLEAN = $(RMGRDESCSOURCES)

all: pg_replslot_reader

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif


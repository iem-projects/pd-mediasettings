#!/usr/bin/make -f
# Makefile for pure data externals in lib creb.
# Needs Makefile.pdlibbuilder to work (https://github.com/pure-data/pd-lib-builder)

lib.name = mediasettings

# special file that does not provide a class
lib.setup.sources = 

# all other C and C++ files in subdirs are source files per class
# (alternatively, enumerate them by hand)
class.sources = audiosettings.c  midisettings.c

datafiles = \
audiosettings-help.pd  midisettings-help.pd \
LICENSE.txt \
README.txt \
mediasettings-meta.pd

cflags = -DVERSION='"$(lib.version)"'

################################################################################
### pdlibbuilder ###############################################################
################################################################################

# This Makefile is based on the Makefile from pd-lib-builder written by
# Katja Vetter. You can get it from:
# https://github.com/pure-data/pd-lib-builder
PDLIBBUILDER_DIR=pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

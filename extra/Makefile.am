#########################################
##### Files, Binaries, & Libs #####

SUBDIRS = bob~ bonk~ choice fiddle~ loop~ lrshift~ pd~ pique sigmund~ stdout

DIST_SUBDIRS = $(SUBDIRS)

PATCHES = \
    complex-mod~.pd \
    hilbert~.pd \
    rev1-final.pd \
    rev1~.pd \
    rev1-stage.pd \
    rev2~.pd \
    rev3~.pd \
    output~.pd

HELPPATCHES = \
    complex-mod~-help.pd \
    hilbert~-help.pd \
    rev1~-help.pd \
    rev2~-help.pd \
    rev3~-help.pd \
    output~-help.pd

EXTRA_DIST = makefile.subdir

libpdextradir = $(pkglibdir)/extra

dist_libpdextra_DATA = $(PATCHES) $(HELPPATCHES)

dist_libpdextra_DATA += \
	README.txt

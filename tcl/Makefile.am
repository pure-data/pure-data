#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign

CLEANFILES=

SUFFIXES = .tcl

libpdtcldir = $(pkglibdir)/tcl

#########################################
##### Files, Binaries, & Libs #####

# install gui launcher script to $(prefix)/bin
dist_bin_SCRIPTS = pd-gui
CLEANFILES += pd-gui

dist_libpdtcl_SCRIPTS = pd-gui.tcl
dist_libpdtcl_DATA = \
    apple_events.tcl \
    dialog_canvas.tcl \
    dialog_gatom.tcl \
    dialog_path.tcl \
    pd_bindings.tcl \
    pd_menus.tcl \
    pdwindow.tcl \
    scrollboxwindow.tcl \
    AppMain.tcl \
    dialog_data.tcl \
    dialog_iemgui.tcl \
    dialog_startup.tcl \
    pd_connect.tcl \
    pkgIndex.tcl \
    wheredoesthisgo.tcl \
    dialog_array.tcl \
    dialog_find.tcl \
    dialog_message.tcl \
    helpbrowser.tcl \
    pdtk_canvas.tcl \
    pkg_mkIndex.tcl \
    dialog_audio.tcl \
    dialog_font.tcl \
    dialog_midi.tcl \
    opt_parser.tcl \
    pd_menucommands.tcl \
    pdtk_text.tcl \
    pdtk_textwindow.tcl \
    scrollbox.tcl \
    pd_guiprefs.tcl \
    pd_deken.tcl \
    pd_docsdir.tcl

# we want these in the dist tarball
EXTRA_DIST = pd.gif pd.ico pd.xpm

# install icons based on the platform
if HURD
libpdtcl_DATA = pd.gif
endif
if LINUX
# should this be gif or xpm?
libpdtcl_DATA = pd.gif
endif
if WINDOWS
libpdtcl_DATA = pd.ico
endif
# MACOSX only loads icon within app bundle

#########################################
##### Targets #####

.PHONY: convenience-links

all-local: convenience-links

# create symlinks to the tcl files for out of tree builds:
# test ! path1 -ef path2 checks if top_srcdir & top_builddir are different
#
# this allows running locally from $(top_builddir) to work
convenience-links:
	if test ! $(top_srcdir) -ef $(top_builddir) ; then \
	    rm -f $(top_builddir)/tcl/*.tcl ; \
	    $(LN_S) $(top_srcdir)/tcl/*.tcl $(top_builddir)/tcl/ ; \
	fi

# remove convenience links for out of tree builds
clean-local:
	if test ! $(top_srcdir) -ef $(top_builddir) ; then \
	    rm -f $(top_builddir)/tcl/*.tcl ; \
	fi

# generate Emacs tags
etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl

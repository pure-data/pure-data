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
    AppMain.tcl \
    apple_events.tcl \
    dialog_array.tcl \
    dialog_audio.tcl \
    dialog_canvas.tcl \
    dialog_data.tcl \
    dialog_find.tcl \
    dialog_font.tcl \
    dialog_gatom.tcl \
    dialog_preferences.tcl \
    dialog_iemgui.tcl \
    dialog_message.tcl \
    dialog_midi.tcl \
    dialog_path.tcl \
    dialog_startup.tcl \
    helpbrowser.tcl \
    opt_parser.tcl \
    pd_bindings.tcl \
    pd_connect.tcl \
    pd_deken.tcl \
    pd_docsdir.tcl \
    pd_guiprefs.tcl \
    pd_i18n.tcl \
    pd_menucommands.tcl \
    pd_menus.tcl \
    pdtcl_compat.tcl \
    pdtk_canvas.tcl \
    pdtk_text.tcl \
    pdtk_textwindow.tcl \
    pdwindow.tcl \
    pkgIndex.tcl \
    pkg_mkIndex.tcl \
    preferencewindow.tcl \
    scrollbox.tcl \
    scrollboxwindow.tcl \
    wheredoesthisgo.tcl \
    $(empty)

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

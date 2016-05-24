#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign

SUFFIXES = .tcl

libpdtcldir = $(pkglibdir)/tcl

#########################################
##### Files, Binaries, & Libs #####
# install gui launcher script to $(prefix)/bin
dist_bin_SCRIPTS = pd-gui

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
    pd.ico

#########################################
##### Targets #####

# generate Emacs tags
etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl

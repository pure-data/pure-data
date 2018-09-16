#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign

SUFFIXES = .po .pot .msg

if MSGFMT
XGETTEXT=xgettext
MSGMERGE=msgmerge
MSGFMT=msgfmt
else
XGETTEXT=echo fake:xgettext
MSGMERGE=echo fake:msgmerge
MSGFMT=echo fake:msgfmt
endif

#########################################
##### Files, Binaries, & Libs #####

TCLFILES = \
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
    scrollbox.tcl \
    pd_guiprefs.tcl \
    pd_deken.tcl \
    pd_docsdir.tcl

LOCAL_TCLFILES = \
    iemgui_dynamic_strings.tcl \
    pd_dynamic_strings.tcl

# note: adding $(top_srcdir) prefix for out of tree builds
# where $(top_srcdir) and $(top_builddir) are not the same
#
# updating template.pot & the .po files are not considered per-build artifacts
# but part of the source, the per build files are the generated .msg files

FILES = $(addprefix $(top_srcdir)/tcl/, $(TCLFILES)) \
        $(addprefix $(top_srcdir)/po/, $(LOCAL_TCLFILES))

# these are the supported languages
ALL_LINGUAS = af az be bg de el en_ca eu es fr gu he hi hu it pa pt_br pt_pt sq sv vi
POFILES = $(addprefix $(top_srcdir)/po/, $(ALL_LINGUAS:=.po))
MSGFILES = $(ALL_LINGUAS:=.msg)

TEMPLATE = $(top_srcdir)/po/template.pot

libpdpodir = $(pkglibdir)/po
libpdpo_DATA = $(MSGFILES)
dist_libpdpo_DATA =

# files that are included but not built
EXTRA_DIST = $(POFILES) $(TEMPLATE) README.txt \
             $(addprefix $(top_srcdir)/po/, $(LOCAL_TCLFILES))

#########################################
##### Targets #####

.PHONY: locations

all-local: template po msg

clean-local:
	rm -f $(MSGFILES)

# refresh template.pot from the source code
template: $(TEMPLATE)

# remove string file locations and header "POT-Creation-Date" to suppress
# unnecessary changes, we really only want to update the .po files when a
# translation string changes in a tcl file
$(TEMPLATE): $(FILES)
	touch $(TEMPLATE)
	$(XGETTEXT) \
		--from-code=UTF-8 --language=Tcl --keyword=_ \
		--sort-by-file --output=$(TEMPLATE) \
		--package-name="Pure Data" --package-version=$(VERSION) \
		--copyright-holder='This file is put in the public domain' \
		--foreign-user \
		--msgid-bugs-address=pd-dev@iem.at \
		$(FILES)
	sed -i.bak -E '/^#:/d' $(TEMPLATE)
	sed -i.bak -E '/^"POT-Creation-Date/d' $(TEMPLATE)
	rm -f $(TEMPLATE).bak

# print the locations for translation strings in the .tcl source files
locations:
	$(XGETTEXT) \
		--from-code=UTF-8 --language=Tcl --keyword=_ \
		--sort-by-file --foreign-user --omit-header --output=- \
		$(FILES)

# refresh .po files from the template
po: $(POFILES)

# the .po files depend on template.pot, suppress line location changes to
# avoid unnecessary updates
$(POFILES): %.po: $(TEMPLATE)
	$(MSGMERGE) --no-location --quiet --update $@ $(TEMPLATE)
	touch $@
	rm -f $@~

# generate .msg files from the .po files,
# these are installed and loaded by the GUI at runtime
msg: $(MSGFILES)

# each .msg file depends on updates to the corresponding .po file
$(MSGFILES): %.msg: %.po
	$(MSGFMT) --check --tcl --locale=$* -d . $<

etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl

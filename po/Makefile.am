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
    AppMain.tcl \
    apple_events.tcl \
    dialog_array.tcl \
    dialog_audio.tcl \
    dialog_canvas.tcl \
    dialog_data.tcl \
    dialog_find.tcl \
    dialog_font.tcl \
    dialog_gatom.tcl \
    dialog_iemgui.tcl \
    dialog_message.tcl \
    dialog_midi.tcl \
    dialog_path.tcl \
    dialog_preferences.tcl \
    dialog_startup.tcl \
    helpbrowser.tcl \
    opt_parser.tcl \
    pd-gui.tcl \
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

EXCLUDE_FILE = $(addprefix $(top_srcdir)/po/, _exclude.txt)

# these are the supported languages
ALL_LINGUAS = \
	az \
	bg \
	de \
	el en en_ca eo es eu \
	fr \
	gu \
	he hi hu hy \
	id it \
	ja \
	ko \
	nl \
	pa pl pt_br pt_pt \
	ru \
	sq sv \
	ta tr \
	uk \
	vi \
	zh_tw \
	$(empty)

POFILES = $(addprefix $(top_srcdir)/po/, $(filter-out .po,$(ALL_LINGUAS:=.po)))
MSGFILES = $(patsubst %.po,%.msg,$(notdir $(POFILES)))

TEMPLATE = $(top_srcdir)/po/template.pot

libpdpodir = $(pkglibdir)/po
libpdpo_DATA = $(MSGFILES)
dist_libpdpo_DATA =

# files that are included but not built
EXTRA_DIST = \
    $(POFILES) $(TEMPLATE) README.txt \
    _exclude.txt \
    $(LOCAL_TCLFILES) \
    $(empty)

#########################################
##### Targets #####

all-local: template po msg

clean-local:
	rm -f $(MSGFILES)

# refresh template.pot from the source code
.PHONY: template
template: $(TEMPLATE)

# remove string file locations and header "POT-Creation-Date" to suppress
# unnecessary changes, we really only want to update the .po files when a
# translation string changes in a tcl file
$(TEMPLATE): $(FILES) $(EXCLUDE_FILE)
	touch $(TEMPLATE)
	$(XGETTEXT) \
		--from-code=UTF-8 --language=Tcl --keyword=_ \
		--sort-by-file --output=$(TEMPLATE).new \
		--package-name="Pure Data" --package-version=$(VERSION) \
		--copyright-holder='This file is put in the public domain' \
		--foreign-user \
		--no-location \
		--exclude-file=$(EXCLUDE_FILE) \
		--msgid-bugs-address=pd-dev@iem.at \
		$(FILES)
	sed -E -e '/^"POT-Creation-Date/d' -e '/^"Project-Id-Version/d' $(TEMPLATE).new > $(TEMPLATE).new.nodate
	sed -E -e '/^"POT-Creation-Date/d' -e '/^"Project-Id-Version/d' $(TEMPLATE) > $(TEMPLATE).nodate
	cmp $(TEMPLATE).new.nodate $(TEMPLATE).nodate || cat $(TEMPLATE).new > $(TEMPLATE)
	rm -f $(TEMPLATE).new $(TEMPLATE).new.nodate $(TEMPLATE).nodate

# print the locations for translation strings in the .tcl source files
.PHONY: locations
locations:
	$(XGETTEXT) \
		--from-code=UTF-8 --language=Tcl --keyword=_ \
		--sort-by-file --foreign-user --omit-header --output=- \
		$(FILES)

# refresh .po files from the template
.PHONY: po
po: $(POFILES)

# the .po files depend on template.pot, suppress line location changes to
# avoid unnecessary updates
$(POFILES): %.po: $(TEMPLATE)
	$(MSGMERGE) --no-location --quiet --update $@ $(TEMPLATE)
	touch $@
	rm -f $@~

# generate .msg files from the .po files,
# these are installed and loaded by the GUI at runtime
.PHONY: msg
msg: $(MSGFILES)

# each .msg file depends on updates to the corresponding .po file
$(MSGFILES): %.msg: %.po
	$(MSGFMT) --check --tcl --locale=$* -d . $<

.PHONY: etags
etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl


allpofiles=$(wildcard $(srcdir)/*.po)
.PHONY: unused
unused:
	@echo "UNUSED TRANSLATIONS: $(patsubst $(srcdir)/%.po,%,$(filter-out $(POFILES), $(allpofiles)))"

AUTOMAKE_OPTIONS = foreign

SUFFIXES = .po .pot .msg

# this is the only way to get gettext 0.17 with Fink
#UNAME := $(shell uname -s)
#ifeq ($(UNAME),Darwin)
if MACOSX
  PATH := /sw/lib/gettext-tools-0.17/bin:${PATH}
endif

TCLFILES = apple_events.tcl dialog_canvas.tcl dialog_gatom.tcl dialog_path.tcl pd_bindings.tcl pd_menus.tcl pdwindow.tcl scrollboxwindow.tcl AppMain.tcl dialog_data.tcl dialog_iemgui.tcl dialog_startup.tcl pd_connect.tcl pkgIndex.tcl wheredoesthisgo.tcl dialog_array.tcl dialog_find.tcl dialog_message.tcl helpbrowser.tcl pdtk_canvas.tcl pkg_mkIndex.tcl dialog_audio.tcl dialog_font.tcl dialog_midi.tcl opt_parser.tcl pd_menucommands.tcl pdtk_text.tcl scrollbox.tcl pd_guiprefs.tcl

FILES = $(addprefix ../tcl/, $(TCLFILES)) iemgui_dynamic_strings.tcl pd_dynamic_strings.tcl

# these are the supported languages, 
ALL_LINGUAS = af az be bg de el en_ca eu fr gu he hi hu it pa pt_br pt_pt sq sv vi
POFILES = $(ALL_LINGUAS:=.po)
MSGFILES = $(ALL_LINGUAS:=.msg)

TEMPLATE = template.pot

libpdpodir = $(pkglibdir)/po
libpdpo_DATA = $(MSGFILES)
dist_libpdpo_DATA =

EXTRA_DIST = $(POFILES) \
	iemgui_dynamic_strings.tcl pd_dynamic_strings.tcl \
	template.pot

# generate .msg files from the .po files
all-local: $(MSGFILES)



# refresh .po files from the template
clean-local:
	-rm -f -- $(MSGFILES)
	-rm -f -- $(POFILES:=~)

po: $(POFILES)

# refresh the template from the source code
template: $(TEMPLATE)

$(TEMPLATE): $(FILES)
	xgettext --join-existing \
		--from-code=UTF-8 --language=Tcl --keyword=_ \
		--sort-by-file --output=$(TEMPLATE) \
		--package-name="Pure Data" --package-version=0.43 \
		--copyright-holder='This file is put in the public domain' \
		--foreign-user \
		--msgid-bugs-address=pd-dev@iem.at \
		$(FILES)

# I guess officially, the .po file should depend on the template.pot, but its
# mostly annoying since it wasnts to update the template.pot and .po files any
# time a .tcl file changes
# $(POFILES): %.po: $(TEMPLATE)
$(POFILES): %.po:
	msgmerge --sort-by-file --update $< $(TEMPLATE)


%.msg: %.po
	msgfmt --check --tcl --locale=$* -d . $<


etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl

# this is the UNIX-style complicated layout dir, simple goes to $(prefix)/po
libpddir = $(prefix)/lib/pd

# this is the only way to get gettext 0.17 with Fink
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  PATH := /sw/lib/gettext-tools-0.17/bin:${PATH}
endif

# these are the files to search for localization strings
SOURCES = ../src/AppMain.tcl ../src/apple_events.tcl ../src/dialog_array.tcl \
	../src/dialog_audio.tcl ../src/dialog_canvas.tcl ../src/dialog_data.tcl \
	../src/dialog_find.tcl ../src/dialog_font.tcl ../src/dialog_gatom.tcl \
	../src/dialog_iemgui.tcl ../src/dialog_message.tcl ../src/dialog_midi.tcl \
	../src/dialog_path.tcl ../src/dialog_startup.tcl ../src/helpbrowser.tcl \
	../src/opt_parser.tcl ../src/pd-gui.tcl ../src/pd_bindings.tcl \
	../src/pd_connect.tcl ../src/pd_menucommands.tcl ../src/pd_menus.tcl \
	../src/pdtk_canvas.tcl ../src/pdtk_text.tcl ../src/pdwindow.tcl \
	../src/scrollbox.tcl ../src/scrollboxwindow.tcl ../src/wheredoesthisgo.tcl

# these are the supported languages, 
ALL_LINGUAS = af az be bg de el en_ca eu fr gu he hi hu it pa pt_br pt_pt sq sv vi
POFILES = $(ALL_LINGUAS:=.po)
MSGFILES = $(ALL_LINGUAS:=.msg)

TEMPLATE = template.pot

.SUFFIXES = .po .pot .msg
.PHONY = all po template install clean

# generate .msg files from the .po files
all: $(MSGFILES)

# refresh .po files from the template
po: $(POFILES)

install: $(MSGFILES)
	install -d $(DESTDIR)$(libpddir)/po
	install -p $(MSGFILES) $(DESTDIR)$(libpddir)/po

# refresh the template from the source code
template: $(TEMPLATE)

$(TEMPLATE): $(SOURCES)
	xgettext --join-existing \
		--from-code=UTF-8 --language=Tcl --keyword=_ \
		--sort-by-file --output=$(TEMPLATE) \
		--package-name="Pure Data" --package-version=0.43 \
		--copyright-holder='This file is put in the public domain' \
		--msgid-bugs-address=pd-dev@iem.at \
		$(SOURCES)
# fink's and MinGW's xgettext are too old for these flags, needs 0.17
#		--package-name="Pure Data" --package-version=0.43 \

# I guess officially, the .po file should depend on the template.pot, but its
# mostly annoying since it wasnts to update the template.pot and .po files any
# time a .tcl file changes
# $(POFILES): %.po: $(TEMPLATE)
$(POFILES): %.po:
	msgmerge --sort-by-file --update $< $(TEMPLATE)


%.msg: %.po
	msgfmt --check --tcl --locale=$* -d . $<


distdir:
	install -m644 -p Makefile $(distdir)
	install -m644 -p $(TEMPLATE) $(distdir)
	install -m644 -p $(POFILES) $(distdir)

distclean: clean

clean:
	-rm -f -- $(MSGFILES)
	-rm -f -- $(POFILES:=~)


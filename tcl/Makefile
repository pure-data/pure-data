# this is the UNIX-style complicated layout dir, simple goes to $(prefix)/po
libpddir = $(prefix)/lib/pd

# these are the files to search for localization strings
APPSOURCES = pd-gui.tcl
MODSOURCES = AppMain.tcl apple_events.tcl dialog_array.tcl \
	dialog_audio.tcl dialog_canvas.tcl dialog_data.tcl \
	dialog_find.tcl dialog_font.tcl dialog_gatom.tcl \
	dialog_iemgui.tcl dialog_message.tcl dialog_midi.tcl \
	dialog_path.tcl dialog_startup.tcl helpbrowser.tcl \
	opt_parser.tcl pd_bindings.tcl \
	pd_connect.tcl pd_menucommands.tcl pd_menus.tcl \
	pdtk_canvas.tcl pdtk_text.tcl pdwindow.tcl \
	scrollbox.tcl scrollboxwindow.tcl wheredoesthisgo.tcl \
	pkgIndex.tcl

SOURCES = $(APPSOURCES) $(MODSOURCES)

.SUFFIXES = .tcl
.PHONY = all install clean

# nothing todo
all: 

install: $(MSGFILES)
	install -d $(DESTDIR)$(libpddir)/tcl
	install -p $(APPSOURCES) $(DESTDIR)$(libpddir)/tcl
	install -m644 -p $(MODSOURCES) $(DESTDIR)$(libpddir)/tcl

distdir:
	install -m644 -p Makefile $(distdir)
	install -p $(APPSOURCES) $(distdir)
	install -m644 -p $(MODSOURCES) $(distdir)

maintainer-clean distclean: clean

clean:
	-rm -f -- $(SOURCES:=~)



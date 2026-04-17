#########################################
##### Files, Binaries, & Libs #####

# files that are included but not built
EXTRA_DIST = \
	     x-puredata.xml \
	     icons/48x48/puredata.png \
	     icons/512x512/puredata.png \
	     icons/puredata.svg \
	     org.puredata.pd-gui.desktop \
	     org.puredata.pd-gui.metainfo.xml \
	     $(empty)

iconsdatadir = $(datadir)/icons/hicolor
icons48dir = $(iconsdatadir)/48x48/apps
icons512dir = $(iconsdatadir)/512x512/apps
iconssvgdir = $(iconsdatadir)/scalable/apps
pixmapsdir = $(datadir)/pixmaps

desktopdir = $(datadir)/applications
appdatadir = $(datarootdir)/metainfo

if LINUX
dist_icons48_DATA = icons/48x48/puredata.png
dist_icons512_DATA = icons/512x512/puredata.png
dist_iconssvg_DATA = icons/puredata.svg
dist_pixmaps_DATA = puredata.xpm

desktop_DATA = org.puredata.pd-gui.desktop
appdata_DATA = org.puredata.pd-gui.metainfo.xml
endif

puredata.xpm: ../tcl/pd.xpm
	cp $< $@

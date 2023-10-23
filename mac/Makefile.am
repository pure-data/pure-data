#########################################
##### Files, Binaries, & Libs #####

# files that are included but not built
EXTRA_DIST = \
    README.txt \
    osx-app.sh \
    tcltk-wish.sh \
    stuff/Info.plist \
    stuff/pd.entitlements \
    stuff/wish-shell.tgz

pkgdata_DATA =

#########################################
##### Targets #####

.PHONY: app

if MACOSX
pkgdata_DATA += \
	stuff/pd.icns \
	stuff/pd-file.icns \
	$(empty)

# build an app bundle on OSX
app:
	rm -rf $(abs_top_builddir)/Pd-$(VERSION).app
	$(SH) $(top_srcdir)/mac/osx-app.sh --builddir $(abs_top_builddir) $(VERSION)
	mv $(abs_top_builddir)/mac/Pd-$(VERSION).app $(abs_top_builddir)

else

# don't bother on other platforms
app:
	echo "app bundle only supported when building on OS X"

endif

# kludge that relies on this Makefile being called *before* the src/Makefile
# since we remove empty folders in $(libdir)/pd last in the main Makefile,
# make sure multiple calls to `make uninstall` do not fail due to missing bin dir
uninstall-local:
	$(MKDIR_P) $(DESTDIR)$(pkglibdir)/bin

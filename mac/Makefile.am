#########################################
##### Files, Binaries, & Libs #####

# files that are included but not built
EXTRA_DIST = \
    README.txt \
    osx-app.sh \
    tcltk-wish.sh \
    stuff/Info.plist \
    stuff/pd-file.icns \
    stuff/pd.icns \
    stuff/wish-shell.tgz

#########################################
##### Targets #####

.PHONY: app

if MACOSX

# build an app bundle on OSX
app:
	rm -rf $(top_builddir)/Pd-$(VERSION).app
	$(SH) $(top_srcdir)/mac/osx-app.sh --builddir $(top_builddir) $(VERSION)
	mv $(top_builddir)/mac/Pd-$(VERSION).app $(top_builddir)

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

#########################################
##### Files, Binaries, & Libs #####

# files that are included but not built
EXTRA_DIST = \
    osx-app.sh \
    stuff/Info.plist \
    stuff/pd-file.icns \
    stuff/pd.icns

#########################################
##### Targets #####

.PHONY: app

if MACOSX

# build an app bundle on OSX
app:
	rm -rf $(top_builddir)/Pd-$(VERSION).app
	$(SH) $(top_srcdir)/mac/osx-app.sh $(VERSION)
	mv $(top_builddir)/mac/Pd-$(VERSION).app $(top_builddir)

else

# don't bother on other platforms
app:
	echo "app bundle only supported when building on OS X"

endif

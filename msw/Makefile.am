#########################################
##### Files, Binaries, & Libs #####

#########################################
##### Targets #####

.PHONY: app

if WINDOWS

# build a standalone application directory on Windows
app:
	rm -rf $(top_builddir)/pd-$(VERSION)
	$(SH) $(top_srcdir)/msw/msw-app.sh --sources --builddir $(top_builddir) $(VERSION)
	mv $(top_builddir)/msw/pd-$(VERSION) $(top_builddir)

else

# don't bother on other platforms
app:
	echo "only supported when building on Windows"

endif

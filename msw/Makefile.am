#########################################
##### Files, Binaries, & Libs #####

#########################################
##### Targets #####

.PHONY: install-windows

if WINDOWS

# make a standalone distro on Windows
# NOTE: all necessary files go into /pd/, the rest ends up in /pd/not-needed/ and is deleted later.
#       eventually /pd/ is renamed to /pd-VERSION/ unless VERSION is empty
install-windows:
	make -C $(top_builddir) install prefix=/pd/not-needed/ exec_prefix=/pd/
	$(top_srcdir)/msw/msw-install.sh $(DESTDIR) $(VERSION)

# build a standalone application directory on Windows
app:
	rm -rf $(top_builddir)/pd-$(VERSION)
	$(SH) $(top_srcdir)/msw/msw-app.sh --sources --builddir $(top_builddir) $(VERSION)
	mv $(top_builddir)/msw/pd-$(VERSION) $(top_builddir)

else

# don't bother on other platforms
install-windows:
	echo "only supported when building on Windows"

endif

#########################################
##### Files, Binaries, & Libs #####

#########################################
##### Targets #####

.PHONY: install-windows

if WINDOWS

# make a standalone distro on Windows
install-windows:
	make -C $(top_builddir) install prefix=$(prefix)/pd-$(VERSION)
	$(top_srcdir)/msw/msw-install.sh $(DESTDIR)$(prefix)/pd-$(VERSION)

else

# don't bother on other platforms
install-windows:
	echo "only supported when building on WINDOWS"

endif

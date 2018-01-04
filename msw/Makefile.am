#########################################
##### Files, Binaries, & Libs #####

#########################################
##### Targets #####

.PHONY: install-windows

if WINDOWS

# make a standalone distro on Windows
install-windows:
	make -C $(top_builddir) install exec_prefix=/pd/
	$(top_srcdir)/msw/msw-install.sh $(DESTDIR) $(VERSION)

else

# don't bother on other platforms
install-windows:
	echo "only supported when building on WINDOWS"

endif

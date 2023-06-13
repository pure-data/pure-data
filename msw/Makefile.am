#########################################
##### Files, Binaries, & Libs #####

#########################################
##### Targets #####

.PHONY: app

if WINDOWS

# when cross-compiling, there is typically no MINGW_PREFIX
# since we need one for the msw-app.sh script, let's calculate one as fallback
MINGW_PREFIX ?= $(patsubst %/,%,$(realpath $(dir $(wildcard $(realpath $(shell $(CC) -print-file-name=libwinpthread-1.dll))))/../))
export MINGW_PREFIX


# build a standalone application directory on Windows
app:
	rm -rf $(top_builddir)/pd-$(VERSION)
	$(SH) $(top_srcdir)/msw/msw-app.sh ${msw_strip} --sources --builddir $(top_builddir) $(VERSION)
	mv $(top_builddir)/msw/pd-$(VERSION) $(top_builddir)

else

# don't bother on other platforms
app:
	echo "only supported when building on Windows"

endif

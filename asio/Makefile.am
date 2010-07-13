# The ASIO sources are not free, so they are not included here.  You can get
# them for free (as in beer) from Steinberg:

# http://www.steinberg.net/en/company/3rd_party_developer.html

# First, download the ASIO SDK. You have to agree to Steinberg's license, then
# submit an email address, then they'll send you the download URL in an
# email. Download the asiosdk2.2.zip file to the pd/asio directory, then unzip
# it right into that folder.  You should have all of the ASIO files in
# pd/asio/ASIOSDK2 when you are done unzipping.

# this is built as a "libtool convenience library"
# http://sources.redhat.com/automake/automake.html#Libtool-Convenience-Libraries

AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = 
INCLUDES = -I./ASIOSDK2/host -I./ASIOSDK2/host/pc -I./ASIOSDK2/common

noinst_LTLIBRARIES = libasio.la

nodist_libasio_la_SOURCES = \
	ASIOSDK2/common/asio.cpp \
	ASIOSDK2/host/asiodrivers.cpp \
	ASIOSDK2/host/pc/asiolist.cpp

EXTRA_DIST = README.txt

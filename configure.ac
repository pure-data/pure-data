AC_PREREQ(2.59)
AC_INIT(pd,0.43.0)
AC_CONFIG_SRCDIR(src/m_pd.c)
AC_CONFIG_AUX_DIR([m4/config])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4/generated])

AC_CANONICAL_HOST

# you will need to install iPhone XCode on Mac OS X to use this:
PD_CHECK_IPHONE(IPHONEOS=yes, IPHONEOS=no, AC_MSG_ERROR([iPhoneOS SDK not available]))
PD_CHECK_ANDROID(ANDROID=yes, ANDROID=no , AC_MSG_ERROR([Android SDK not available]))

case $host in
*darwin*)
	if test "x${IPHONEOS}" = "xno"; then
	 portaudio=yes
	 portmidi=yes
	 MACOSX=yes

	 EXTERNAL_CFLAGS="-fPIC"
	 EXTERNAL_EXTENSION=d_fat
	else
	 portaudio=no

	 EXTERNAL_CFLAGS=
	 EXTERNAL_EXTENSION=pd_darwin
	fi

	CFLAGS="$CFLAGS -O3 -funroll-loops -fomit-frame-pointer"
	INCLUDES="$INCLUDES -I/sw/include"
	LDFLAGS="$LDFLAGS -L/sw/lib"
	EXTERNAL_LDFLAGS="-bundle -undefined dynamic_lookup"
	;;
*linux*|GNU|GNU/kFreeBSD)
# GNU and GNU/kFreeBSD are for Debian, were they are treated very similar to linux
	if test "x${host}" = "xGNU"; then
	 HURD=yes
	fi
	if test "x${ANDROID}" = "xno"; then
	 LINUX=yes
	 portaudio=yes
	 CFLAGS="$CFLAGS -O6 -funroll-loops -fomit-frame-pointer"
	fi
	EXTERNAL_CFLAGS="-fPIC"
	EXTERNAL_LDFLAGS="-Wl,--export-dynamic -shared -fPIC"
	EXTERNAL_EXTENSION=pd_linux
	;;
*mingw*)
	WINDOWS=yes
	MINGW=yes
# ASIO doesn't build yet with the autotools setup. We need to figure out how
#        to make the final linking phase use g++
#		asio=yes
	portaudio=yes
	CFLAGS="$CFLAGS -O6 -funroll-loops -fomit-frame-pointer"
# ASIO is a C++ library, so if its included, then use g++ to build
	CC=g++
	EXTERNAL_CFLAGS="-mms-bitfields"
	EXTERNAL_LDFLAGS="-s -shared -Wl,--enable-auto-import -lpd"
	EXTERNAL_EXTENSION=dll
	;;
*cygwin*)
	WINDOWS=yes
	CYGWIN=yes
	portaudio=yes
	CFLAGS="$CFLAGS -O6 -funroll-loops -fomit-frame-pointer"
	EXTERNAL_CFLAGS=
	EXTERNAL_LDFLAGS="-Wl,--export-dynamic -shared -lpd"
	EXTERNAL_EXTENSION=dll
	;;
esac

PD_CHECK_UNIVERSAL(ARCH, universal=yes, universal=no)
AM_CONDITIONAL(UNIVERSAL, test x$universal = xyes)
if test "x${universal}" = "xyes"; then
 CFLAGS="${CFLAGS} ${ARCH_CFLAGS}"
 LDFLAGS="${LDFLAGS} ${ARCH_LDFLAGS}"
fi

AM_CONDITIONAL(ANDROID, test x$ANDROID = xyes)
AM_CONDITIONAL(IPHONEOS, test x$IPHONEOS = xyes)
AM_CONDITIONAL(LINUX, test x$LINUX = xyes)
AM_CONDITIONAL(HURD, test x$HURD = xyes)
AM_CONDITIONAL(MACOSX, test x$MACOSX = xyes)
AM_CONDITIONAL(WINDOWS, test x$WINDOWS = xyes)
AM_CONDITIONAL(CYGWIN, test x$MINGW = xyes)
AM_CONDITIONAL(MINGW, test x$MINGW = xyes)

# Configure libtool
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_LN_S
#AC_PROG_GREP
#AC_PROG_SED

AC_SUBST(EXTERNTARGET)
AC_SUBST(INCLUDES)

AC_SUBST(EXTERNAL_EXTENSION)
AC_SUBST(EXTERNAL_CFLAGS)
AC_SUBST(EXTERNAL_LDFLAGS)

dnl Checks for libraries.
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIBM
# AC_CHECK_LIBM computes LIBM but does not add to LIBS, hence we add it in
# src/Makefile.am under pd_LDFLAGS as well

# Checking for `pthread_create' function in -pthread (MinGW uses unusual names)
## CHECK whether this can be replaced by AX_PTHREAD
AC_CHECK_LIB([pthread], [pthread_create],LIBS="-lpthread $LIBS",
    AC_CHECK_LIB([pthreadGC2], [pthread_create], LIBS="-lpthreadGC2 $LIBS",
        AC_CHECK_LIB([pthreadGC1], [pthread_create], LIBS="-lpthreadGC1 $LIBS",
            AC_CHECK_LIB([pthreadGC], [pthread_create], LIBS="-lpthreadGC $LIBS",
         AC_MSG_WARN([pthreads required])))))

AC_HEADER_STDC

dnl Cygwin has sys/soundcard.h but Pd isn't ready for Cygwin since it only 
dnl includes OSS for UNIX
AC_CHECK_HEADER(sys/soundcard.h, [oss=yes], [oss=no])
AM_CONDITIONAL(OSS, test x$oss = xyes)

AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [coreaudio=yes], [coreaudio=no])
dnl AC_CHECK_HEADER(Jackmp/jack.h, [jack=yes], [jack=no])

dnl portaudio/CoreAudio doesn't work with iPhone
test x$IPHONEOS = xyes && coreaudio=no

AM_CONDITIONAL(COREAUDIO, test x$coreaudio = xyes)

dnl TODO implement debug and static
#AC_ARG_ENABLE(static,   [  --enable-static         link statically],
#    static=$enableval)

#AC_ARG_ENABLE([debug],
#	[AS_HELP_STRING([--enable-debug], [use debugging support])], 
#		[debug=$enableval])
#if test x$debug = xyes; then
#    CFLAGS="$CFLAGS -g"
#fi

dnl ALSA
dnl shouldn't we use AM_PATH_ALSA from /usr/share/aclocal/alsa.m4
dnl alternatively we could use pkg-config
AC_ARG_ENABLE([alsa],
	[AS_HELP_STRING([--disable-alsa],
		[do not use ALSA audio driver])],
    [alsa=$enableval], [alsa=yes])
AS_IF([test x$alsa = xyes], [
    AC_CHECK_LIB(asound,snd_pcm_info,LIBS="$LIBS -lasound",alsa=no)])
AM_CONDITIONAL(ALSA, test x$alsa = xyes)

dnl ASIO
AC_ARG_ENABLE([asio],
	[AS_HELP_STRING([--enable-asio],
		[use ASIO audio driver])],
    [asio=$enableval])
AS_IF([test x$asio = xyes], [
    AC_CHECK_FILE([asio/ASIOSDK2/common/asio.h],[asio=yes],[asio=no])])
AM_CONDITIONAL(ASIO, test x$asio = xyes)

dnl JACK
dnl shouldn't we use pkg-config?
dnl TODO support jack xrun
AC_ARG_ENABLE([jack],
	[AS_HELP_STRING([--enable-jack],
		[use JACK audio server])],
	[jack=$enableval])
dnl on Mac OS X, use the JackOSX.com Jackmp.framework not the jack lib.
dnl this is set in src/Makefile.am based on the JACK var
AS_IF([test x$MACOSX != xyes], [
    AS_IF([test x$jack = xyes], [
        AC_CHECK_LIB(rt,shm_open,LIBS="$LIBS -lrt")
        AC_CHECK_LIB(jack,jack_set_xrun_callback,LIBS="$LIBS -ljack";jack=xrun)
        AC_CHECK_LIB(jack,jack_set_error_function,LIBS="$LIBS -ljack";jack=yes,
	              jack=no)])])
AM_CONDITIONAL(JACK, test x$jack = xyes)

dnl portaudio
AC_ARG_ENABLE([portaudio],
		[AS_HELP_STRING([--disable-portaudio],
			[do not use portaudio])],
    	[portaudio=$enableval])	
AM_CONDITIONAL(PORTAUDIO, test x$portaudio = xyes)

dnl portmidi
AC_ARG_ENABLE([portmidi],
	[AS_HELP_STRING([--enable-portmidi],
		[use portmidi])],
    [portmidi=$enableval])
AM_CONDITIONAL(PORTMIDI, test x$portmidi = xyes)
if test x$portmidi = xyes; then echo Using included portmidi; fi

dnl fftw v2
AC_ARG_ENABLE([fftw],
	[AS_HELP_STRING([--enable-fftw],
		[use FFTW package])],
    [fftw=$enableval])
if test x$fftw = xyes; then
    AC_CHECK_LIB(fftw, fftw_one, [LIBS="$LIBS -lfftw"],
        [AC_MSG_NOTICE([fftw package not found - using built-in FFT]); fftw=no])
fi
AM_CONDITIONAL(FFTW, test x$fftw = xyes)

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/param.h sys/socket.h sys/soundcard.h sys/time.h sys/timeb.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([dup2 floor getcwd gethostbyname gettimeofday memmove memset pow regcomp select socket sqrt strchr strerror strrchr strstr strtol])

# check for programs
AC_CHECK_PROG(HAVE_MSGFMT, [msgfmt], yes, no)
test x$HAVE_MSGFMT = xyes || AC_MSG_WARN([Install GNU gettext with msgfmt if you want localizations to be compiled!])
AM_CONDITIONAL(MSGFMT, test x$HAVE_MSGFMT = xyes)

# Specify what files are to be created.
AC_CONFIG_FILES([Makefile
                asio/Makefile
                man/Makefile
                portaudio/Makefile
                portmidi/Makefile
                tcl/Makefile
                po/Makefile
                src/Makefile
                extra/Makefile
		extra/bonk~/GNUmakefile
		extra/choice/GNUmakefile
		extra/expr~/GNUmakefile
		extra/fiddle~/GNUmakefile
		extra/loop~/GNUmakefile
		extra/lrshift~/GNUmakefile
		extra/pd~/GNUmakefile
		extra/pique/GNUmakefile
		extra/sigmund~/GNUmakefile
		extra/stdout/GNUmakefile
                pd.pc
                ])

AC_OUTPUT

dnl output some messages at the end

AS_IF([test x$windows = xyes], [
    AS_IF([test x$asio = xno], [
	    AC_MSG_RESULT(
ASIO SDK not found! You need to download asiosdk2.2.zip from: 
    http://www.steinberg.net/en/company/3rd_party_developer.html)])])



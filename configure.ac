AC_PREREQ(2.59)
AC_INIT(pd,0.43.0)
AC_CONFIG_SRCDIR(src/m_pd.c)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE(1.9)

case $host in
powerpc-apple-darwin* | i*86*-apple-darwin*)
		MACOSX=yes
		portaudio=yes
		portmidi=yes
		CFLAGS="$CFLAGS -fast -funroll-loops -fomit-frame-pointer"
		INCLUDES="$INCLUDES -I/sw/include"
		LDFLAGS="$LDFLAGS -L/sw/lib"
		;;
arm*darwin*)
        # you will need to install iPhone XCode on Mac OS X to use this:
		IPHONEOS=yes
        IPHONE_BASE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin
        CC=$IPHONE_BASE/gcc
        CPP=$IPHONE_BASE/cpp
		CFLAGS="$CFLAGS -miphoneos-version-min=3.0 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.0.sdk -arch armv6 -fast -funroll-loops -fomit-frame-pointer"
		INCLUDES="$INCLUDES -I/sw/include"
		LDFLAGS="$LDFLAGS -arch armv6 -L/sw/lib"
        # workaround for rpl_malloc/rpl_realloc bug in autoconf when cross-compiling
        ac_cv_func_malloc_0_nonnull=yes
        ac_cv_func_realloc_0_nonnull=yes
		;;
arm-linux)
        ANDROID=yes
        ANDROID_PATH=/home/hans/Documents/android/android-ndk-1.6_r1
        ANDROID_GCC_PATH=$ANDROID_PATH/build/prebuilt/linux-x86/arm-eabi-4.2.1/bin
        TARGET_PLATFORM=android-4
        TARGET_ARCH_ABI=arm
        SYSROOT=$ANDROID_PATH/build/platforms/$TARGET_PLATFORM/arch-$TARGET_ARCH_ABI
        CC=$ANDROID_GCC_PATH/arm-eabi-gcc
        CPP=$ANDROID_GCC_PATH/arm-eabi-cpp
        LD=$ANDROID_GCC_PATH/arm-eabi-ld
        CPPFLAGS=-I$SYSROOT/usr/include
        # TODO the API needs to be set, its currently disabled
        CONFIG_MACROS="-DANDROID -D__linux__ -DHAVE_UNISTD_H -DHAVE_LIBDL -D__BSD_VISIBLE=1"
		CFLAGS="$CONFIG_MACROS -I$SYSROOT/usr/include -march=armv5te -mtune=xscale -msoft-float -fpic -mthumb-interwork -ffunction-sections -funwind-tables -fstack-protector -fno-short-enums -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ -mthumb -O2 -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64"
        TARGET_LIBGCC=`$CC -print-libgcc-file-name`
        LDFLAGS="-nostdlib -Wl,--no-undefined -Wl,-rpath-link=$SYSROOT/usr/lib $TARGET_LIBGCC"
        # workaround for rpl_malloc/rpl_realloc bug in autoconf when cross-compiling
        ac_cv_func_malloc_0_nonnull=yes
        ac_cv_func_realloc_0_nonnull=yes
		;;
*linux*)
		LINUX=yes
		portaudio=yes
		CFLAGS="$CFLAGS -O6 -funroll-loops -fomit-frame-pointer"
		;;
*mingw*)
		WINDOWS=yes
		MINGW=yes
# ASIO doesn't build yet with the autotools setup. We need to figure out how
#        to make the final linking phase use g++
#		asio=yes
		portaudio=yes
		CFLAGS="$CFLAGS -mcpu=i586 -mtune=pentium3 -O6 -funroll-loops -fomit-frame-pointer"
# ASIO is a C++ library, so if its included, then use g++ to build
		CC=g++
		;;
*cygwin*)
		WINDOWS=yes
		CYGWIN=yes
		portaudio=yes
		CFLAGS="$CFLAGS -mcpu=i586 -mtune=pentium3 -O6 -funroll-loops -fomit-frame-pointer"
		;;
esac
AM_CONDITIONAL(ANDROID, test x$ANDROID = xyes)
AM_CONDITIONAL(IPHONEOS, test x$IPHONEOS = xyes)
AM_CONDITIONAL(LINUX, test x$LINUX = xyes)
AM_CONDITIONAL(MACOSX, test x$MACOSX = xyes)
AM_CONDITIONAL(WINDOWS, test x$WINDOWS = xyes)
AM_CONDITIONAL(CYGWIN, test x$MINGW = xyes)
AM_CONDITIONAL(MINGW, test x$MINGW = xyes)

# Configure libtool
AC_CONFIG_MACRO_DIR([m4])
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

dnl Checks for libraries.
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIBM
# AC_CHECK_LIBM computes LIBM but does not add to LIBS, hence we add it in
# src/Makefile.am under pd_LDFLAGS as well

# Checking for `pthread_create' function in -pthread (MinGW uses unusual names)
AC_CHECK_LIB([pthread], [pthread_create],LIBS="-lpthread $LIBS",
    AC_CHECK_LIB([pthreadGC2], [pthread_create], LIBS="-lpthreadGC2 $LIBS",
        AC_CHECK_LIB([pthreadGC1], [pthread_create], LIBS="-lpthreadGC1 $LIBS",
            AC_CHECK_LIB([pthreadGC], [pthread_create], LIBS="-lpthreadGC $LIBS",
         echo "WARNING! pthreads required"))))

AC_HEADER_STDC

dnl Cygwin has sys/soundcard.h but Pd isn't ready for Cygwin since it only 
dnl includes OSS for UNIX
AC_CHECK_HEADER(sys/soundcard.h, [oss=yes], [oss=no])
AM_CONDITIONAL(OSS, test x$oss = xyes)

AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [coreaudio=yes], [coreaudio=no])
dnl portaudio/CoreAudio doesn't work with iPhone
test x$IPHONEOS = xyes && coreaudio=no
AM_CONDITIONAL(COREAUDIO, test x$coreaudio = xyes)

dnl TODO implement debug and static
AC_ARG_ENABLE(static,   [  --enable-static         link statically],
    static=$enableval)

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug], [use debugging support])], 
		[debug=$enableval])
if test x$debug = xyes; then
    CFLAGS="$CFLAGS -g"
fi

AC_ARG_ENABLE([universal],
	[AS_HELP_STRING([--enable-universal],
		[build universal binary on Mac OS X])], 
    [universal=$enableval], [universal=no])
AM_CONDITIONAL(UNIVERSAL, test x$universal = xyes)
if test x$universal = xyes; then
   CFLAGS="$CFLAGS -fast -ffast-math -arch ppc7400 -arch i386 -arch ppc64 -arch x86_64 \
             -mmacosx-version-min=10.5"
   LDFLAGS="$LDFLAGS -arch ppc7400 -arch i386 -arch ppc64 -arch x86_64 -mmacosx-version-min=10.5"
fi

AC_ARG_ENABLE([alsa],
	[AS_HELP_STRING([--disable-alsa],
		[do not use ALSA audio driver])],
    [alsa=$enableval], [alsa=yes])
AS_IF([test x$alsa = xyes], [
    AC_CHECK_LIB(asound,snd_pcm_info,LIBS="$LIBS -lasound",alsa=no)])
AM_CONDITIONAL(ALSA, test x$alsa = xyes)

AC_ARG_ENABLE([asio],
	[AS_HELP_STRING([--enable-asio],
		[do not use ASIO audio driver])],
    [asio=$enableval])
AS_IF([test x$asio = xyes], [
    AC_CHECK_FILE([asio/ASIOSDK2/common/asio.h],[asio=yes],[asio=no])])
AM_CONDITIONAL(ASIO, test x$asio = xyes)

dnl TODO support jack xrun
AC_ARG_ENABLE([jack],
	[AS_HELP_STRING([--enable-jack],
		[use JACK audio server])],
	[jack=$enableval])
AS_IF([test x$jack = xyes], [
    AC_CHECK_LIB(jack,jack_set_xrun_callback,LIBS="$LIBS -ljack";jack=xrun)
    AC_CHECK_LIB(jack,jack_set_error_function,LIBS="$LIBS -ljack";jack=yes,
		jack=no)])
AM_CONDITIONAL(JACK, test x$jack = xyes)

AC_ARG_ENABLE([portaudio],
		[AS_HELP_STRING([--disable-portaudio],
			[do not use portaudio])],
    	[portaudio=$enableval])	
AM_CONDITIONAL(PORTAUDIO, test x$portaudio = xyes)

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
        [echo "fftw package not found - using built-in FFT"; fftw=no])
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
test x$HAVE_MSGFMT = xyes || echo "WARNING: Install GNU gettext with msgfmt if you want localizations to be compiled!"
AM_CONDITIONAL(MSGFMT, test x$HAVE_MSGFMT = xyes)

# Specify what files are to be created.
AC_CONFIG_FILES([Makefile
                asio/Makefile
                man/Makefile
                portaudio/Makefile
                portmidi/Makefile
                src/Makefile])

AC_OUTPUT

dnl output some messages at the end

AS_IF([test x$windows = xyes], [
    AS_IF([test x$asio = xno], [
	    AC_MSG_RESULT(
ASIO SDK not found! You need to download asiosdk2.2.zip from: 
    http://www.steinberg.net/en/company/3rd_party_developer.html)])])


#########################################
##### Prelude #####

AC_PREREQ(2.59)
AC_INIT([pd], [0.49.0])
AC_CONFIG_SRCDIR(src/m_pd.c)
AC_CONFIG_AUX_DIR([m4/config])
AC_CONFIG_MACRO_DIR([m4/generated])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CANONICAL_HOST

#########################################
##### Default values #####

# set here and not in AC_ARG_ENABLE so the values set in the OS detection
# below are not overwritten if the configure argument isn't used
portaudio=no
portmidi=no
local_portaudio=yes
local_portmidi=yes
mmio=no
asio=no
jack_framework=no
locales=yes

# wish command info, the actual value is set using the WISH define
# "wish" mirrors the default WISH define in s_inter.c
wish="wish"

# list of detected apis
audio_backends=""
midi_backends=""

# debug/release defaults
DEBUG_CFLAGS="-O0"
RELEASE_CFLAGS="-ffast-math -funroll-loops -fomit-frame-pointer -O3"

#########################################
##### OS Detection #####

# you will need to install XCode on Mac OS X to use this:
PD_CHECK_IPHONE(IPHONEOS=yes, IPHONEOS=no, AC_MSG_ERROR([iOS SDK not available]))
PD_CHECK_ANDROID(ANDROID=yes, ANDROID=no, AC_MSG_ERROR([Android SDK not available]))

case $host in
*darwin*)
    if test "x${IPHONEOS}" = "xno"; then
        MACOSX=yes
        platform="Mac OSX"
        coreaudio=yes
        portaudio=yes
        portmidi=yes
        jack_framework=yes
        EXTERNAL_CFLAGS="-fPIC"
        EXTERNAL_EXTENSION=d_fat

        # a set of search paths are used on macOS in s_inter.c
        wish="default search paths"

        # required for dlopen & weak linking for older OSX version support
        CFLAGS="-mmacosx-version-min=10.6 $CFLAGS"
    else
        platform=iOS
        locales=no
        wish="not used"
    fi

    # homebrew paths
    if test -e /usr/local ; then
        AM_CPPFLAGS="-I/usr/local/include $INCLUDES"
        LDFLAGS="-L/usr/local/lib $LDFLAGS"
    fi

    # fink paths
    if test -e /sw ; then
        AM_CPPFLAGS="-I/sw/include $INCLUDES"
        LDFLAGS="-L/sw/lib $LDFLAGS"
    fi

    # macports paths
    if test -e /opt/local ; then
        AM_CPPFLAGS="-I/opt/local/include $INCLUDES"
        LDFLAGS="-L/opt/local/lib $LDFLAGS"
    fi

    EXTERNAL_LDFLAGS="-bundle -undefined dynamic_lookup"
    ;;
*linux*|*kfreebsd*gnu*)
    # GNU/kFreeBSD are for Debian, were they are treated very similar to linux
    if test "x${ANDROID}" = "xno"; then
        LINUX=yes
        platform=Linux
        portaudio=yes
        EXTERNAL_CFLAGS="-fPIC"
        EXTERNAL_LDFLAGS="-Wl,--export-dynamic -fPIC"
        EXTERNAL_EXTENSION=pd_linux
    else
        platform=Android
    fi
    ;;
*-*-gnu*)
    HURD=yes
    platform=Hurd
    EXTERNAL_CFLAGS="-fPIC"
    EXTERNAL_LDFLAGS="-Wl,--export-dynamic -fPIC"
    EXTERNAL_EXTENSION=pd_linux
    ;;
*mingw*)
    WINDOWS=yes
    MINGW=yes
    platform=MinGW
    mmio=yes
    asio=yes
    portaudio=yes
    portmidi=yes

    # this mirrors the default windows WISH define in s_inter.c
    wish="wish85.exe"

    # externals are dynamically linked to pd.dll in their individual automake files
    EXTERNAL_CFLAGS="-mms-bitfields"
    EXTERNAL_LDFLAGS="-s -Wl,--enable-auto-import -no-undefined -lpd"
    EXTERNAL_EXTENSION=dll

    # workaround for rpl_malloc/rpl_realloc bug in autoconf when cross-compiling
    ac_cv_func_malloc_0_nonnull=yes
    ac_cv_func_realloc_0_nonnull=yes
    ;;
*cygwin*)
    WINDOWS=yes
    CYGWIN=yes
    platform=Cygwin
    mmio=yes
    asio=yes
    portaudio=yes
    portmidi=yes

    # this mirrors the default windows WISH define in s_inter.c
    wish="wish85.exe"

    # externals are dynamically linked to pd.dll in their individual automake files
    EXTERNAL_CFLAGS=
    EXTERNAL_LDFLAGS="-Wl,--export-dynamic"
    EXTERNAL_EXTENSION=dll
    ;;
*openbsd*)
    OPENBSD=yes
    platform=OpenBSD
    portaudio=yes
    EXTERNAL_CFLAGS="-D__OPENBSD__"
    EXTERNAL_LDFLAGS="-liberty"
    EXTERNAL_EXTENSION=pd_openbsd
    ;;
*)
    platform=Unknown
    ;;
esac

AM_CONDITIONAL(ANDROID, test x$ANDROID = xyes)
AM_CONDITIONAL(IPHONEOS, test x$IPHONEOS = xyes)
AM_CONDITIONAL(LINUX, test x$LINUX = xyes)
AM_CONDITIONAL(HURD, test x$HURD = xyes)
AM_CONDITIONAL(MACOSX, test x$MACOSX = xyes)
AM_CONDITIONAL(WINDOWS, test x$WINDOWS = xyes)
AM_CONDITIONAL(CYGWIN, test x$CYGWIN = xyes)
AM_CONDITIONAL(MINGW, test x$MINGW = xyes)
AM_CONDITIONAL(OPENBSD, test x$OPENBSD = xyes)

#########################################
##### Check for programs, libs, & headers #####

# Configure libtool.
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_GREP
AC_PROG_SED
AC_CHECK_TOOL([WINDRES], [windres], [:])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h \
                  limits.h \
                  malloc.h \
                  netdb.h \
                  netinet/in.h \
                  stddef.h \
                  stdlib.h \
                  string.h \
                  sys/ioctl.h \
                  sys/param.h \
                  sys/socket.h \
                  sys/soundcard.h \
                  sys/time.h \
                  sys/timeb.h \
                  unistd.h
                  ])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([dup2 \
                floor \
                getcwd \
                gethostbyname \
                gettimeofday \
                memmove \
                memset \
                pow \
                regcomp \
                select \
                socket \
                sqrt \
                strchr \
                strerror \
                strrchr \
                strstr \
                strtol
                ])

# Checks for libraries.
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIBM
# AC_CHECK_LIBM computes LIBM but does not add to LIBS, hence we add it in
# src/Makefile.am under pd_LDFLAGS as well

# Apple's CoreAudio
# not used directly, implicitly needed when using PortAudio on OSX
AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [coreaudio=yes], [coreaudio=no])

# Checking for `pthread_create' function in -pthread (MinGW uses unusual names)
## CHECK whether this can be replaced by AX_PTHREAD
AC_CHECK_LIB([pthread], [pthread_create],LIBS="-lpthread $LIBS",
    AC_CHECK_LIB([pthreadGC2], [pthread_create], LIBS="-lpthreadGC2 $LIBS",
        AC_CHECK_LIB([pthreadGC1], [pthread_create], LIBS="-lpthreadGC1 $LIBS",
            AC_CHECK_LIB([pthreadGC], [pthread_create], LIBS="-lpthreadGC $LIBS",
         AC_MSG_WARN([pthreads required])))))

# Define variables for use in Makefiles
AC_SUBST(EXTERNTARGET)
AC_SUBST(EXTERNAL_EXTENSION)
AC_SUBST(EXTERNAL_CFLAGS)
AC_SUBST(EXTERNAL_LDFLAGS)
AC_SUBST([ALSA_LIBS])
AC_SUBST([JACK_LIBS])

# pass include paths down to all Makefiles
AC_SUBST([AM_CPPFLAGS], [$AM_CPPFLAGS])

#########################################
##### Configure Options #####

##### Build with debug flags? #####
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [use debugging support])],
    [debug=$enableval], [debug=no])
if test x$debug = xyes ; then
    CFLAGS="$CFLAGS $DEBUG_CFLAGS"
else
    CFLAGS="$CFLAGS $RELEASE_CFLAGS"
fi

##### Universal/multi architecture build on OSX #####
PD_CHECK_UNIVERSAL(ARCH, [universal=yes], [universal=no])
AM_CONDITIONAL(UNIVERSAL, test x$universal = xyes)
if test x$universal = xyes ; then
    CFLAGS="$ARCH_CFLAGS $CFLAGS"
    LDFLAGS="$ARCH_LDFLAGS $LDFLAGS"
fi

##### Gettext #####
# Gettext is needed to build language localizations.
AC_ARG_ENABLE([locales],
    [AS_HELP_STRING([--disable-locales], [do not compile localizations (requires gettext)])],
    [locales=$enableval])
if test x$locales = xyes ; then
    AC_CHECK_PROG(HAVE_MSGFMT, [msgfmt], yes, no)
    if test x$HAVE_MSGFMT = xno ; then
        AC_MSG_WARN([Install GNU gettext with msgfmt if you want localizations to be compiled!])
        locales=no
    fi
fi
AM_CONDITIONAL(MSGFMT, test x$HAVE_MSGFMT = xyes)

##### OSS #####
# Cygwin has sys/soundcard.h but Pd isn't ready for Cygwin since it only
# includes OSS for UNIX
AC_ARG_ENABLE([oss],
    [AS_HELP_STRING([--disable-oss], [do not use OSS driver])],
    [oss=$enableval], [oss=yes])
if test x$oss = xyes ; then
    AC_CHECK_HEADER(sys/soundcard.h, [oss=yes], [oss=no])
fi

##### ALSA #####
# shouldn't we use AM_PATH_ALSA from /usr/share/aclocal/alsa.m4
# alternatively we could use pkg-config
AC_ARG_ENABLE([alsa],
    [AS_HELP_STRING([--disable-alsa], [do not use ALSA audio driver])],
    [alsa=$enableval], [alsa=yes])
if test x$alsa = xyes ; then
    AC_CHECK_LIB([asound], [snd_pcm_info], [ALSA_LIBS="-lasound"], [alsa=no])
fi

##### JACK FRAMEWORK #####
# on macOS, use the JackOSX.com Jackmp.framework not the jack lib by default
# this is set in src/Makefile.am based on the JACK & JACK_FRAMEWORK vars
AC_ARG_ENABLE([jack-framework],
    [AS_HELP_STRING([--disable-jack-framework], [do not weak link to Jackmp.framework on macOS])],
    [jack_framework=$enableval])
test x$MACOSX = xyes || jack_framework=no
if test x$jack_framework = xyes ; then
    AC_MSG_NOTICE([Weak linking to Jackmp.framework])
fi

##### JACK #####
AC_ARG_ENABLE([jack],
    [AS_HELP_STRING([--enable-jack], [use JACK audio server])],
    [jack=$enableval], [jack=no])
if [ test x$jack_framework != xyes ] && [ test x$jack = xyes ] ; then
    AC_CHECK_LIB([rt], [shm_open], [LIBS="$LIBS -lrt"])
    AC_CHECK_LIB([jack], [jack_set_xrun_callback], [JACK_LIBS="-ljack" ; jack=xrun])
    AC_CHECK_LIB([jack], [jack_set_error_function], [JACK_LIBS="-ljack" ; jack=yes], [jack=no])
fi

##### MMIO #####
AC_ARG_ENABLE([mmio],
    [AS_HELP_STRING([--disable-mmio], [do not use MMIO driver])],
    [mmio=$enableval])
test x$WINDOWS = xyes || mmio=no

##### ASIO #####
AC_ARG_ENABLE([asio],
    [AS_HELP_STRING([--enable-asio], [use ASIO audio driver])],
    [asio=$enableval])
test x$WINDOWS = xyes || asio=no
AS_IF([test x$asio = xyes],
    test -f "${srcdir}"/asio/ASIOSDK/common/asio.h || asio=no
    AS_IF([test x$asio = xno], AC_MSG_WARN([ASIO SDK not found! See asio/README.txt]))
)

##### PortAudio #####
AC_ARG_ENABLE([portaudio],
    [AS_HELP_STRING([--disable-portaudio], [do not use portaudio])],
    [portaudio=$enableval])
AC_ARG_WITH([local-portaudio],
    [AS_HELP_STRING([--without-local-portaudio], [do not use the portaudio included with pd])],
    [local_portaudio=$withval])
if test x$portaudio = xyes ; then
    if test x$local_portaudio = xno ; then
        # fall back to local portaudio if not foud
        AC_CHECK_LIB([portaudio], [Pa_Initialize],
            [AC_MSG_NOTICE([Using system PortAudio])], [local_portaudio=yes])
    fi
    if test x$local_portaudio = xyes ; then
        if test -d "$srcdir/portaudio" ; then
            AC_MSG_NOTICE([Using included PortAudio])
        else
            AC_MSG_WARN([PortAudio not found in pd source directory])
            portaudio=no
        fi
    fi
fi

##### PortMidi #####
AC_ARG_ENABLE([portmidi],
    [AS_HELP_STRING([--enable-portmidi], [use portmidi])],
    [portmidi=$enableval])
AC_ARG_WITH([local-portmidi],
    [AS_HELP_STRING([--without-local-portmidi], [do not use the portmidi included with pd])],
    [local_portmidi=$withval])
# don't allow portmidi if already using oss
AS_IF([test x$oss = xyes -a x$portmidi != xno],
  [AC_MSG_WARN([Cannot enable both OSS-midi and PortMidi..preferring OSS])
  portmidi=no])
if test x$portmidi = xyes ; then
    if test x$local_portmidi = xno ; then
        # fall back to local portmidi if not foud
        AC_CHECK_LIB([portmidi], [Pm_Initialize],
            [AC_MSG_NOTICE([Using system PortMidi])], [local_portmidi=yes])
    fi
    if test x$local_portmidi = xyes ; then
        if test -d "$srcdir/portmidi" ; then
            AC_MSG_NOTICE([Using included PortMidi])
        else
            AC_MSG_WARN([PortMidi not found in pd source directory])
            portmidi=no
        fi
    fi
fi

##### fftw v3 #####
AC_ARG_ENABLE([fftw],
    [AS_HELP_STRING([--enable-fftw], [use FFTW package])],
    [fftw=$enableval], [fftw=no])
if test x$fftw = xyes; then
    AC_CHECK_LIB([fftw3f], [fftwf_execute],
        [LIBS="$LIBS -lfftw3f"],
        [AC_MSG_NOTICE([fftw package not found, using built-in FFT]) ; fftw=no])
fi
AM_CONDITIONAL(FFTW, test x$fftw = xyes)

##### Wish #####
AC_ARG_WITH([wish],
    [AS_HELP_STRING([--with-wish=WISH], [which Tk Wish application to use for the Pd GUI])],
    [WISH=$withval])
AS_IF([test "x${WISH}" = "xyes" -o "x${WISH}" = "xno"],
    [AC_MSG_NOTICE([--with-wish requires an application, ignoring '${WISH}'])
    WISH=""])
AC_SUBST([WISH])

##### Configure Build From Options #####
## configure the build based on what we have found above

##### OSS #####
# Cygwin has a function OSS /dev/dsp, but not MIDI,
# and Pd is only set up to handle a single MIDI API
AS_IF([test x$WINDOW = xyes -a x$oss != xno],
  [AC_MSG_WARN([OSS not working on W32..disabling])
  oss=no])
AM_CONDITIONAL(OSS, test x$oss = xyes)
AS_IF([test x$oss = xyes], [audio_backends="OSS ${audio_backends}"])
AS_IF([test x$oss = xyes], [midi_backends="OSS ${midi_backends}"])

##### ALSA #####
AM_CONDITIONAL(ALSA, test x$alsa = xyes)
AS_IF([test x$alsa = xyes], [audio_backends="ALSA ${audio_backends}"])
AS_IF([test x$alsa = xyes], [midi_backends="ALSA ${midi_backends}"])

##### JACK #####
AM_CONDITIONAL(JACK, test x$jack = xyes)
AM_CONDITIONAL(JACK_FRAMEWORK, test x$jack_framework = xyes)
AS_IF([test x$jack = xyes], [audio_backends="JACK ${audio_backends}"])

##### MMIO #####
AM_CONDITIONAL(MMIO, test x$mmio = xyes)
AS_IF([test x$mmio = xyes], [audio_backends="MMIO ${audio_backends}"])
dnl AS_IF([test x$mmio = xyes], [midi_backends="MMIO ${midi_backends}"])

##### ASIO #####
AM_CONDITIONAL(ASIO, test x$asio = xyes)
AS_IF([test x$asio = xyes], [audio_backends="ASIO ${audio_backends}"])

##### CoreAudio #####
# portaudio doesn't work with iPhone, so don't bother with CoreAudio
AS_IF([test x$IPHONEOS = xyes -a x$coreaudio != xno],
  [AC_MSG_WARN([PortAudio not working on iPhone..disabling CoreAudio])
  coreaudio=no])
AM_CONDITIONAL(COREAUDIO, test x$coreaudio = xyes)

##### PortAudio #####
AM_CONDITIONAL(PORTAUDIO, test x$portaudio = xyes)
AM_CONDITIONAL(LOCAL_PORTAUDIO, test x$local_portaudio = xyes)
AS_IF([test x$portaudio = xyes], [audio_backends="PortAudio ${audio_backends}"])

##### PortMidi #####
AM_CONDITIONAL(PORTMIDI, test x$portmidi = xyes)
AM_CONDITIONAL(LOCAL_PORTMIDI, test x$local_portmidi = xyes)
AS_IF([test x$portmidi = xyes], [midi_backends="PortMidi ${midi_backends}"])

##### DUMMY #####
# warn if we end up trying to build without an audio or midi api
AM_CONDITIONAL(AUDIO_DUMMY, test "x$audio_backends" = "x")
AS_IF([test "x$audio_backends" = "x"],[
    AC_MSG_WARN([No audio API specified or detected])
    audio_backends="NONE!"
])
# we need the dummy MIDI backend, if there is no other backend providing the
# standard MIDI-functions (OSS and PortMidi provide them; ALSA not)
needs_midi_dummy="yes"
AS_IF([echo "${midi_backends}" | grep -wi OSS >/dev/null],[needs_midi_dummy="no"])
AS_IF([echo "${midi_backends}" | grep -wi PortMidi >/dev/null],[needs_midi_dummy="no"])

AM_CONDITIONAL(MIDI_DUMMY, test "x${needs_midi_dummy}" = "xyes")
AS_IF([test "x$midi_backends" = "x"],[
    AC_MSG_WARN([No MIDI API specified or detected])
    midi_backends="NONE!"
])

##### Wish #####
# add contextual info when adding a custom WISH path on macOS,
# other platforms use a single path so it's enough to print $wish as it is
AS_IF([test ! "x${WISH}" = "x"],[
    AS_IF([test x"$MACOSX" = x"yes"], [
        wish="prepending ${WISH} to search paths"
    ],[
        wish="${WISH}"
    ])
])

#########################################
##### Output files #####

# Specify what files are to be created.
AC_CONFIG_FILES([Makefile
                asio/Makefile
                doc/Makefile
                font/Makefile
                mac/Makefile
                man/Makefile
                msw/Makefile
                portaudio/Makefile
                portmidi/Makefile
                tcl/Makefile
                tcl/pd-gui
                po/Makefile
                src/Makefile
                extra/Makefile
                extra/bob~/GNUmakefile
                extra/bonk~/GNUmakefile
                extra/choice/GNUmakefile
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

#########################################
##### Print a summary for the user #####

AC_MSG_NOTICE([

    $PACKAGE $VERSION is now configured

    Platform:             $platform
    Debug build:          $debug
    Universal build:      $universal
    Localizations:        $locales
    Source directory:     $srcdir
    Installation prefix:  $prefix

    Compiler:             $CC
    CFLAGS:               $CFLAGS
    LDFLAGS:              $LDFLAGS
    INCLUDES:             $AM_CPPFLAGS
    LIBS:                 $LIBS

    External extension:   $EXTERNAL_EXTENSION
    External CFLAGS:      $EXTERNAL_CFLAGS
    External LDFLAGS:     $EXTERNAL_LDFLAGS

    fftw:                 $fftw
    wish(tcl/tk):         $wish
    audio APIs:           $audio_backends
    midi APIs:            $midi_backends
])

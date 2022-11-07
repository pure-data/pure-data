---
title: more on BSD
---

[back to table of contents](index.html) \
{{< green-button "back to 'installing from source'" "installing from source" "Linux & BSD">}}

### BSD

Building Pd for the various BSD variants is similar to the Linux way.
The major difference is the used package manager (and the names of the
packages), you'll want to install.

### FreeBSD

(Tested on FreeBSD-13)

Install the core build requirements:

    sudo pkg install gcc automake autoconf libtool gettext gmake

You may install one (or more) libraries (depending on your needs). It
seems that with FreeBSD-13, there are ALSA and JACK packages available:

    sudo pkg install alsa-lib jackit

Once your build system is set up, you can follow the general autotools
build steps to build Pd, but make sure to use `gmake` (aka "GNU make").
The ordinary BSD `make` will not suffice!

    ./autogen.sh
    ./configure --deken-os=FreeBSD MAKE=gmake
    gmake

    sudo gmake install

### OpenBSD

(Tested on OpenBSD-7)

Install the core build requirements:

    sudo pkg_add gcc automake autoconf libtool gettext-tools gmake

(If there are multiple versions for one or more of the packages, pick
your favourite or the newest one).

You may install one (or more) libraries (depending on your needs). It
seems that with OpenBSD-7, there are only JACK packages available:

    sudo pkg_add jack

By default, OpenBSD installs all its packages into `/usr/local/`, but
the compiler does not look for headers resp. libraries in this
directory. We can instruct autotools to automatically consider these
directories by creating a file '/usr/local/share/config.site':

    cat | sudo tee /usr/local/share/config.site>/dev/null << EOF
    CPPFLAGS="-I/usr/local/include $CPPFLAGS"
    LDFLAGS="-L/usr/local/lib $LDFLAGS"
    EOF

Also, because OpenBSD allows to coinstall multiple versions of the
autotools (with no "default"), we must specify which version we want to
use:

    export AUTOCONF_VERSION=$(ls -S /usr/local/bin/autoconf-* | sed -e 's|.*-||' | sort -n | tail -1)
    export AUTOMAKE_VERSION=$(ls -S /usr/local/bin/automake-* | sed -e 's|.*-||' | sort -n | tail -1)

Now that your build system is set up, you can follow the general
autotools build steps to build Pd, but make sure to use `gmake` (aka
"GNU make"). The ordinary BSD `make` will not suffice!

    ./autogen.sh
    ./configure --deken-os=OpenBSD --enable-jack MAKE=gmake
    gmake

    sudo gmake install

### NetBSD

(Tested on NetBSD-9)

Install the core build requirements:

    sudo pkgin install gcc automake autoconf libtool gettext-tools gmake

You may install one (or more) libraries (depending on your needs). It
seems that with NetBSD-9, there are JACK and ALSA packages available,
but the ALSA packages seem to be broken. OSS appears to be built-in.

    sudo pkgin install jack

By default, NetBSD installs all its packages into `/usr/pkg/`, but the
compiler does not look for headers resp. libraries in this directory. We
can instruct autotools to automatically consider these directories by
creating a file '/usr/pkg/share/config.site':

    cat | sudo tee /usr/pkg/share/config.site>/dev/null << EOF
    CPPFLAGS="-I/usr/pkg/include $CPPFLAGS"
    LDFLAGS="-L/usr/pkg/lib -Wl,-R/usr/pkg/lib $LDFLAGS"
    EOF

Now that your build system is set up, you can follow the general
autotools build steps to build Pd, but make sure to use `gmake` (aka
"GNU make"). The ordinary BSD `make` will not suffice!

    ./autogen.sh
    ./configure --deken-os=NetBSD --prefix=/usr/pkg --disable-alsa --enable-jack MAKE=gmake
    gmake

    sudo gmake install

{{< green-button "back to 'installing from source'" "installing from source" "Linux & BSD">}} \
[back to table of contents](index.html)

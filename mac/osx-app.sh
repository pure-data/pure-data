#! /bin/bash
#
# Creates macOS .app bundle from pd build.
#
# Make sure pd has been configured and built
# before running this.
#
# Inspired by Inkscape's osx-app.sh builder.
#
# Dan Wilcox danomatika.com
#

# stop on error
set -e

verbose=
universal=
included_wish=true
EXTERNAL_EXT=d_fat
TK=
SYS_TK=Current
WISH=
PD_VERSION=

# source dir, relative to this script
SRC=..

# build dir, relative to working directory
custom_builddir=false
BUILD=..

# PlistBuddy command for editing app bundle Info.plist from template
PLIST_BUDDY=/usr/libexec/PlistBuddy

# the app Get Info string
GETINFO="Pure Data: a free real-time computer music system"

# Help message
#----------------------------------------------------------
help() {
echo -e "
Usage: osx-app.sh [OPTIONS] [VERSION]

  Creates a Pd .app bundle for macOS using a Tk Wish.app wrapper

  Uses the included Tk 8.4 Wish.app at mac/stuff/wish-shell.tgz by default

Options:
  -h,--help           display this help message

  -v,--verbose        verbose copy prints

  -w,--wish APP       use a specific Wish.app

  -s,--system-tk VER  use a version of the Tk Wish.app installed on the system,
                      searches in /Library first then /System/Library second,
                      naming is \"8.4\", \"8.5\", \"Current\", etc

  -t,--tk VER         use a version of Wish.app with embedded Tcl/Tk
                      frameworks, downloads and builds using tcltk-wish.sh

  --universal         \"universal\" multi-arch build when using -t,--tk:
                      i386 & x86_64 (& ppc if 10.6 SDK found)

  --builddir          set pd build directory path

Arguments:

  VERSION             optional string to use in file name ie. Pd-VERSION.app
                      configure --version output used for app plist if not set

Examples:

    # create Pd.app with included Tk 8.4 Wish,
    # version string set automatically
    osx-app.sh

    # create Pd-0.47-0.app with included Tk 8.4 Wish,
    # uses specified version string
    osx-app.sh 0.47-0

    # create Pd-0.47-0.app with the system's Tk 8.4 Wish.app
    osx-app.sh --system-tk 8.4 0.47-0

    # create Pd-0.47-0.app by downloading & building Tk 8.5.19
    osx-app.sh --tk 8.5.19 0.47-0

    # same as above, but with a \"universal\" multi-arch build
    osx-app.sh --tk 8.5.19 --universal 0.47-0

    # use Wish-8.6.5.app manually built with tcltk-wish.sh
    osx-app.sh --wish Wish-8.6.5 0.47-0
"
}

# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case $1 in
        -t|--tk)
            shift 1
            if [ $# == 0 ] ; then
                echo "-t,--tk option requires a VER argument"
                exit 1
            fi
            TK=$1
            included_wish=false
            ;;
        -s|--system-tk)
            if [ $# != 0 ] ; then
                shift 1
                SYS_TK=$1
            fi
            included_wish=false
            ;;
        -w|--wish)
            if [ $# == 0 ] ; then
                echo "-w,--wish option requires an APP argument"
                exit 1
            fi
            shift 1
            WISH=${1%/} # remove trailing slash
            included_wish=false
            ;;
        --universal)
            universal=--universal
            ;;
        --builddir)
            if [ $# == 0 ] ; then
                echo "--builddir options requires a DIR argument"
                exit 1
            fi
            shift 1
            BUILD=${1%/} # remove trailing slash
            custom_builddir=true
            ;;
        -v|--verbose)
            verbose=-v
            ;;
        -h|--help)
            help
            exit 0
            ;;
        *)
            break ;;
    esac
    shift 1
done

# check for version argument and set app path in the dir the script is run from
if [ "$1" != "" ] ; then
    APP=$(pwd)/Pd-$1.app
else
    # version not specified
    APP=$(pwd)/Pd.app
fi

# Go
#----------------------------------------------------------

# make sure custom build directory is an absolute path
if [[ $custom_builddir == true ]] ; then
    if [[ "${BUILD:0:1}" != "/" ]] ; then
       BUILD=$(pwd)/$BUILD
    fi
fi

# change to the dir of this script
cd $(dirname $0)

# grab package version from configure --version output: line 1, word 3
# aka "pd configure 0.47.1" -> "0.47.1"
PD_VERSION=$(../configure --version | head -n 1 | cut -d " " -f 3)

# pd app bundle destination path
DEST=$APP/Contents/Resources

# remove old app if found
if [ -d $APP ] ; then
    rm -rf $APP
fi

# check if pd is already built
if [ ! -e "$BUILD/src/pd" ] ; then
    echo "Looks like pd hasn't been built yet. Maybe run make first?"
    exit 1
fi

if [ "$verbose" != "" ] ; then
    echo "==== Creating $APP"
fi

# extract included Wish app
if [ $included_wish == true ] ; then
    tar xzf stuff/wish-shell.tgz
    mv "Wish Shell.app" Wish.app
    WISH=Wish.app

# build Wish or use the system Wish
elif [ "$WISH" == "" ] ; then
    if [ "$TK" != "" ] ; then
        echo "Using custom $TK Wish.app"
        ./tcltk-wish.sh $universal $TK
        WISH=Wish-${TK}.app
    elif [ "$SYS_TK" != "" ] ; then
        echo "Using system $SYS_TK Wish.app"
        tk_path=/Library/Frameworks/Tk.framework/Versions
        # check /Library first, then fall back to /System/Library
        if [ ! -e $tk_path/$SYS_TK/Resources/Wish.app ] ; then
            tk_path=/System${tk_path}
            if [ ! -e $tk_path/$SYS_TK/Resources/Wish.app ] ; then
                echo "Wish.app not found"
                exit 1
            fi
        fi
        cp -R $verbose $tk_path/$SYS_TK/Resources/Wish.app .
        WISH=Wish.app
    fi

# make a local copy if using a given Wish.app
else

    # get absolute path in original location
    cd - > /dev/null # quiet
    WISH=$(cd "$(dirname "$WISH")"; pwd)/$(basename "$WISH")
    cd - > /dev/null # quiet
    if [ ! -e $WISH ] ; then
        echo "$WISH not found"
        exit 1
    fi
    echo "Using $(basename $WISH)"

    # copy
    WISH_TMP=$(basename $WISH)-tmp
    cp -R $WISH $WISH_TMP
    WISH=$WISH_TMP
fi

# sanity check
if [ ! -e $WISH ] ; then
    echo "$WISH not found"
    exit 1
fi

# change app name and rename resources
# try to handle both older "Wish Shell.app" & newer "Wish.app"
mv "$WISH" $APP
chmod -R u+w $APP
if [ -e $APP/Contents/version.plist ] ; then
    rm $APP/Contents/version.plist
fi
# older "Wish Shell.app" does not have a symlink but a real file
if [ -f $APP/Contents/MacOS/Wish\ Shell ] && \
   [ ! -L $APP/Contents/MacOS/Wish\ Shell ] ; then
    mv $APP/Contents/MacOS/Wish\ Shell $APP/Contents/MacOS/Pd
    mv $APP/Contents/Resources/Wish\ Shell.rsrc $APP/Contents/Resources/Pd.rsrc
else
    mv $APP/Contents/MacOS/Wish $APP/Contents/MacOS/Pd
    if [ -e $APP/Contents/Resources/Wish.rsrc ] ; then
        mv $APP/Contents/Resources/Wish.rsrc $APP/Contents/Resources/Pd.rsrc
    else
        mv $APP/Contents/Resources/Wish.sdef $APP/Contents/Resources/Pd.sdef
    fi
fi
rm -f $APP/Contents/MacOS/Wish
rm -f $APP/Contents/MacOS/Wish\ Shell

# prepare bundle resources
cp stuff/Info.plist $APP/Contents/
rm $APP/Contents/Resources/Wish.icns
cp stuff/pd.icns $APP/Contents/Resources/
cp stuff/pd-file.icns $APP/Contents/Resources/
cp -R ../font $APP/Contents/Resources/

INFO_PLIST=$APP/Contents/Info.plist

# set version identifiers & contextual strings in Info.plist,
# version strings can only use 0-9 and periods, so replace "-" & "test" with "."
PLIST_VERSION=${PD_VERSION/-/.}; PLIST_VERSION=${PLIST_VERSION/test/.}
$PLIST_BUDDY -c "Set:CFBundleVersion \"$PLIST_VERSION\"" $INFO_PLIST
$PLIST_BUDDY -c "Set:CFBundleShortVersionString \"$PLIST_VERSION\"" $INFO_PLIST
$PLIST_BUDDY -c "Set:CFBundleGetInfoString \"$GETINFO\"" $INFO_PLIST

# install binaries
mkdir -p $DEST/bin
cp -R $verbose $BUILD/src/pd          $DEST/bin/
cp -R $verbose $BUILD/src/pdsend      $DEST/bin/
cp -R $verbose $BUILD/src/pdreceive   $DEST/bin/
cp -R $verbose $BUILD/src/pd-watchdog $DEST/bin/

# install resources
mkdir -p $DEST/po
cp -R $verbose $SRC/doc         $DEST/
cp -R $verbose $SRC/extra       $DEST/
cp -R $verbose $SRC/tcl         $DEST/
rm -f $DEST/tcl/pd.ico $DEST/tcl/pd-gui.in
if [ ! $BUILD -ef $SRC ] ; then # are src and build dirs not the same?
    # compiled externals are in the build dir
    cp -R $verbose $BUILD/extra $DEST/
fi

# install licenses
cp $verbose $SRC/README.txt  $DEST/
cp $verbose $SRC/LICENSE.txt $DEST/

# install translations if they were built
if [ -e $BUILD/po/af.msg ] ; then
    mkdir -p $DEST/po
    cp $verbose $BUILD/po/*.msg $DEST/po/
    # add locale entries to the plist based on available .msg files
    # commented out for 0.48-1 because it seems to misbehave - open/save dialogs
    # are opening in a random language (and meanwhile Pd doesn't yet respect
    # current language setting - I don't know what's going on here.  -msp
    # LOCALES=$(find $BUILD/po -name "*.msg" -exec basename {} .msg \;)
    # $PLIST_BUDDY \
    #     -c "Add:CFBundleLocalizations array \"$PLIST_VERSION\"" $INFO_PLIST
    # for locale in $LOCALES ; do
    #     $PLIST_BUDDY \
    #         -c "Add:CFBundleLocalizations: string \"$locale\"" $INFO_PLIST
    # done
else
    echo "No localizations found. Skipping po dir..."
fi

# install headers
mkdir -p $DEST/src
cp $verbose $SRC/src/*.h $DEST/src/

# clean extra folders
cd $DEST/extra
rm -f makefile.subdir
find * -prune -type d | while read ext; do
    ext_lib=$ext.$EXTERNAL_EXT
    if [ -e $ext/.libs/$ext_lib ] ; then

        # remove any symlinks to the compiled external
        rm -f $ext/*.$EXTERNAL_EXT

        # mv compiled external into main folder
        mv $ext/.libs/*.$EXTERNAL_EXT $ext/

        # remove libtool build folders & unneeded build files
        rm -rf $ext/.libs
        rm -rf $ext/.deps
        rm -f $ext/*.c $ext/*.o $ext/*.lo $ext/*.la
        rm -f $ext/GNUmakefile* $ext/makefile*
    fi
done
cd - > /dev/null # quiet

# clean Makefiles
find $DEST -name "Makefile*" -type f -delete

# create any needed symlinks
cd $DEST
ln -s tcl Scripts
cd - > /dev/null # quiet

# finish up
touch $APP

if [ "$verbose" != "" ] ; then
    echo  "==== Finished $APP"
fi

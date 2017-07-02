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
EXTERNAL_EXT=pd_darwin
TK=
SYS_TK=Current
WISH=
PD_VERSION=

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
            if [ $# == 0 ] ; then
                echo "-t,--tk option requires a VER argument"
                exit 1
            fi
            shift 1
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

# change to the dir of this script
cd $(dirname $0)

# grab package version from configure --version output: line 1, word 3
# aka "pd configure 0.47.1" -> "0.47.1"
PD_VERSION=$(../configure --version | head -n 1 | cut -d " " -f 3)

# pd source and app bundle destination paths
SRC=..
DEST=$APP/Contents/Resources

# remove old app if found
if [ -d $APP ] ; then
    rm -rf $APP
fi

# check if pd is already built
if [ ! -e "$SRC/bin/pd" ] ; then
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
        tcltk-wish.sh $universal $TK
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
if [ -f $APP/Contents/MacOS/Wish\ Shell ] && [ ! -L $APP/Contents/MacOS/Wish\ Shell ] ; then
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

# update version identifiers in Info.plist, sanitizes version by removing - & test
PLIST_VERSION=${PD_VERSION/-/.}; PLIST_VERSION=${PLIST_VERSION/test/.}
plutil -replace CFBundleVersion -string $PLIST_VERSION $APP/Contents/Info.plist
plutil -replace CFBundleShortVersionString -string $PLIST_VERSION $APP/Contents/Info.plist
plutil -replace CFBundleGetInfoString -string "$PLIST_VERSION" $APP/Contents/Info.plist

# install binaries
mkdir -p $DEST/bin
cp -R $verbose $SRC/src/pd          $DEST/bin/
cp -R $verbose $SRC/src/pdsend      $DEST/bin/
cp -R $verbose $SRC/src/pdreceive   $DEST/bin/
cp -R $verbose $SRC/src/pd-watchdog $DEST/bin/

# install resources
cp -R $verbose $SRC/doc      $DEST/
cp -R $verbose $SRC/extra    $DEST/
cp -R $verbose $SRC/tcl      $DEST/

# install licenses
cp $verbose $SRC/README.txt  $DEST/
cp $verbose $SRC/LICENSE.txt $DEST/

# install translations if they were built
if [ -e $SRC/po/af.msg ] ; then
    mkdir -p $DEST/po
    cp $verbose $SRC/po/*.msg $DEST/po/
else
    echo "No localizations found. Skipping po dir..."
fi

# install headers
mkdir -p $DEST/include
cp $verbose $SRC/src/*.h $DEST/include/

# clean extra folders
cd $DEST/extra
find * -prune -type d | while read ext; do
    ext_lib=$ext.$EXTERNAL_EXT
    if [ -e $ext/.libs/$ext_lib ] ; then

        # remove any symlinks to the compiled external
        rm -f $ext/$ext_lib

        # mv compiled external into main folder
        mv $ext/.libs/$ext_lib $ext/

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

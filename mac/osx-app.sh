#! /bin/bash
#
# Creates Mac OS X .app bundle from pd build.
#
# Make sure pd has been configured and built
# before running this.
#
# Inspired by Inkscape's osx-app.sh builder.
#
# Dan Wilcox danomatika.com
#

WD=$(dirname $0)

verbose=
included_wish=false
TK=Current

# Help message
#----------------------------------------------------------
help() {
echo -e "
Usage: osx-app.sh [OPTIONS] VERSION

  Creates a Pd .app bundle for OS X

Options:
  -h,--help           display this help message
  -v,--verbose        verbose copy prints
  -t,--tk VER         build using a specific version of the Tk
                      Wish.app on the system (default: Current)
  -i,--included-wish  build using the included Tk 8.4 Wish.app,
                      *may not* be present (default: no)

Arguments:

  VERSION             version string to use in file name (required)

Examples:

    $ create d-0.47-0.app with the current Wish.app
    osx-app.sh 0.47-0

    # create Pd-0.47-0.app with included Wish
    osx-app.sh -i 0.47-0

    # create Pd-0.47-0.app with the system's Tk 8.4 Wish.app
    osx-app.sh -t 8.4 0.47-0
"
}

# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case $1 in
        -t|--tk)
            if [ $# == 0 ] ; then
                echo "-t,--tk option requires an argument"
                exit 1
            fi
            shift 1
            TK=$1
            ;;
        -i|--included-wish)
            included_wish=true
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

# check for required version argument
if [ "$1" == "" ] ; then
   echo "Usage: osx-app.sh [OPTIONS] VERSION"
   exit 1
fi

# Go
#----------------------------------------------------------
APP=Pd-$1.app

SRC=..
DEST=$APP/Contents/Resources

SYS_WISH_APP=/System/Library/Frameworks/Tk.framework/Versions/$TK/Resources/Wish.app

cd $WD

# remove old app if found
if [ -d $APP ] ; then
    rm -rf $APP
fi

# check if pd is already built
if [ ! -e ../bin/pd ] ; then
    echo "Doesn't look like pd has been built yet. Maybe run make first?"
    exit 1
fi

if [ "$verbose" != "" ] ; then 
    echo "==== Creating $APP"
fi

# extract Wish app & rename binary
if [ $included_wish == true ] ; then
    tar xzf stuff/wish-shell.tgz
    mv "Wish Shell.app" $APP
    mv $APP/Contents/MacOS/Wish\ Shell $APP/Contents/MacOS/Pd
    mv $APP/Contents/Resources/Wish\ Shell.rsrc $APP/Contents/Resources/Pd.rsrc
else
    echo "Using $TK system Wish.app"
    if [ -e $SYS_WISH_APP ] ; then
        cp -R $SYS_WISH_APP $APP
        if [ -e $APP/Contents/version.plist ] ; then
            rm $APP/Contents/version.plist
        fi
        mv $APP/Contents/MacOS/Wish $APP/Contents/MacOS/Pd
        if [ -e $APP/Contents/Resources/Wish.rsrc ] ; then
            mv $APP/Contents/Resources/Wish.rsrc $APP/Contents/Resources/Pd.rsrc
        else
            mv $APP/Contents/Resources/Wish.sdef $APP/Contents/Resources/Pd.sdef
        fi
    else
        echo "$TK system Wish.app not found: $SYS_WISH"
        exit 1
    fi
fi
chmod -R u+w $APP

# prepare bundle resources
cp stuff/Info.plist $APP/Contents/
rm $APP/Contents/Resources/Wish.icns
cp stuff/pd.icns $APP/Contents/Resources/
cp stuff/pd-file.icns $APP/Contents/Resources/

# install pd
cp -R $verbose $SRC/bin   $DEST/
cp -R $verbose $SRC/doc   $DEST/
cp -R $verbose $SRC/extra $DEST/
cp -R $verbose $SRC/tcl   $DEST/

mkdir -p $DEST/po
cp $verbose $SRC/po/*.po $DEST/po

# create any needed symlinks
cd $DEST
ln -s tcl Scripts
cd ../../../

# finish up
touch $APP

if [ "$verbose" != "" ] ; then 
    echo  "==== Finished $APP"
fi

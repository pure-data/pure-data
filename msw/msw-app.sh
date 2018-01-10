#! /bin/sh
#
# Creates standalone Windows application directory from Pd build.
#
# Make sure Pd has been configured and built
# before running this.
#

# stop on error
set -e

# include sources
sources=false

# strip binaries
strip=true
STRIP=${STRIP-strip}
STRIPARGS=${STRIPARGS-"--strip-unneeded -R .note -R .comment"}

# build dir, relative to working directory
custom_builddir=false
BUILD=..

# Help message
#----------------------------------------------------------
help() {
echo -e "
Usage: msw-app.sh [OPTIONS] [VERSION]

  Creates a Pd app directory for Windows.

  Uses the included Tk 8.5 in pdprototype.tgz by default

Options:
  -h,--help           display this help message

  -s,--sources        include source files in addition to headers (default: ${sources})
  -n,--no-strip       do not strip binaries (default: do strip)
  --builddir <DIR>    set Pd build directory path (default: ${BUILD})

Arguments:

  VERSION             optional string to use in dir name ie. pd-VERSION

Examples:

    # create pd directory
    msw-app.sh

    # create pd-0.47-0, uses specified version string
    msw-app.sh 0.47-0

    # create pd directory with source files
    msw-app.sh --sources
"
}

# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case $1 in
        -n|--no-strip)
            strip=false
            ;;
        --builddir)
            if [ $# = 0 ] ; then
                echo "--builddir options requires a DIR argument"
                exit 1
            fi
            shift 1
            BUILD=${1%/} # remove trailing slash
            custom_builddir=true
            ;;
        -s|--sources)
            sources=true
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
if [ "x$1" != "x" ] ; then
    APP=$(pwd)/pd-$1
else
    # version not specified
    APP=$(pwd)/pd
fi

# Go
#----------------------------------------------------------

# make sure custom build directory is an absolute path
if [ "x$custom_builddir" = "xtrue" ] ; then
    if [ "x${BUILD#/}" = "x${BUILD}" ] ; then
# BUILD isn't absolute as it doesn't start with '/'
       BUILD="$(pwd)/$BUILD"
    fi
fi

# change to the dir of this script
cd $(dirname $0)

# remove old app dir if found
if [ -d $APP ] ; then
    rm -rf $APP
fi

echo "==== Creating $(basename $APP)"

# install to app directory
make -C $BUILD install DESTDIR=$APP prefix=/

# don't need man pages
rm -rf $APP/share

# place headers together in src directory
mv $APP/include $APP/src
mv $APP/src/pd/* $APP/src
rm -rf $APP/src/pd

# move folders from lib/pd into top level directory
rm -rf $APP/lib/pd/bin
for d in $APP/lib/pd/*; do
	mv $d $APP/
done
rm -rf $APP/lib/

# install sources
if [ "x$sources" = xtrue ] ; then
	mkdir -p $APP/src
	cp -v ../src/*.c $APP/src/
	cp -v ../src/*.h $APP/src/
	for d in $APP/extra/*/; do
		s=${d%/}
		s=../extra/${s##*/}
		cp -v "${s}"/*.c "${d}"
	done
fi

# untar pdprototype.tgz
tar -xf pdprototype.tgz -C $APP/ --strip-components=1

# install info
cp -v ../README.txt  $APP/
cp -v ../LICENSE.txt $APP/

# clean up
find $APP -type f -exec chmod -x {} +
find $APP -type f '(' -iname "*.exe" -o -iname "*.com" ')' -exec chmod +x {} +
find $APP -type f '(' -iname "*.la" -o -iname "*.dll.a" -o -iname "*.am" ')' -delete
find $APP/bin -type f -not -name "*.*" -delete

# strip executables
if [ "x$strip" = xtrue ] ; then
	find "${APP}" -type f  \
		'(' -iname "*.exe" -o -iname "*.com" -o -iname "*.dll" ')' \
		-exec "${STRIP}" ${STRIPARGS} {} +
fi
# finished
touch $APP
echo  "==== Finished $(basename $APP)"

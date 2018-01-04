#! /bin/sh
#
# rearrange folders to get a working standalone Windows distro 
#
# command line args: 
# 1) full (absolute) installation path (DESTDIR)
# 2) VERSION

SCRIPTPATH=${0%/*}
DESTDIR=$1
VERSION=$2

if [ "x${DESTDIR}" = "x" ]; then
 echo "refusing to work without DESTDIR" 1>&2
 exit 1
fi

echo "installing to: $DESTDIR"

# remove unneeded cruft
rm -rf "${DESTDIR}"/usr/

# strip executables
for i in pd.exe pd.com pd.dll; do
	strip --strip-unneeded -R .note -R .comment "${DESTDIR}/pd/bin/$i"
done

# move folders from /lib/pd into top level directory
rm -rf "${DESTDIR}/pd/lib/pd/bin"
mv "${DESTDIR}"/pd/lib/pd/* "${DESTDIR}"/pd/
rm -rf "${DESTDIR}"/pd/lib/

# install sources
mkdir -p "${DESTDIR}/pd/src"
cp "${SCRIPTPATH}/../src"/*.c "${DESTDIR}/pd/src/"
cp "${SCRIPTPATH}/../src"/*.h "${DESTDIR}/pd/src/"
for d in "${DESTDIR}"/pd/extra/*/; do
	s=${d%/}
	s=${SCRIPTPATH}/../extra/${s##*/}
	cp "${s}"/*.c "${d}"
done

# untar pdprototype.tgz
tar -f ${SCRIPTPATH}/pdprototype.tgz -C "${DESTDIR}" -x
cp "${SCRIPTPATH}/../LICENSE.txt" "${DESTDIR}/pd/"

# clean up
find "${DESTDIR}" -type f -exec chmod -x {} +
find "${DESTDIR}" -type f -iname "*.exe" -exec chmod +x {} +
find "${DESTDIR}" -type f -iname "*.com" -exec chmod +x {} +
find "${DESTDIR}" -type f -name "*.la" -delete
find "${DESTDIR}" -type f -name "*.dll.a" -delete
find "${DESTDIR}/pd/bin" -type f -not -name "*.*" -delete

# put into nice VERSION dir
if [ "x${VERSION}" != "x" ]; then
 mv "${DESTDIR}"/pd "${DESTDIR}/pd-${VERSION}"
fi

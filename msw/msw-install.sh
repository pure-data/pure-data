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

# untar pdprototype.tgz
tar -f ${SCRIPTPATH}/pdprototype.tgz -C "${DESTDIR}" -x

# clean up
find "${DESTDIR}" -type f -exec chmod -x {} +
find "${DESTDIR}" -type f -iname "*.exe" -exec chmod +x {} +
find "${DESTDIR}" -type f -iname "*.com" -exec chmod +x {} +
find "${DESTDIR}" -type f -name "*.la" -delete
find "${DESTDIR}" -type f -name "*.dll.a" -delete

# put into nice VERSION dir
if [ "x${VERSION}" != "x" ]; then
 mv "${DESTDIR}"/pd "${DESTDIR}/pd-${VERSION}"
fi

#!/bin/sh

# script to call makensis (NSIS installer generator, adapted for mingw32)
# to make a Pd install/uninstall executable for MS windows.
# slightly adapted from a script by Roman Haefeli

cat > /dev/null << .

*** The README adapted from Roman: ***

The included build.sh is a shell script to create a proper Windows
installer (.exe) out of a Pure Data build tree.

Features of the resulting installer:

 * Install Pure Data to user selectable destination
   (Default 64bit OS 64bit APP: "%Program Files%\Pd")
   (Default 64bit OS 32bit APP: "%Program Files (x86)%\Pd")
   (Default 32bit OS 32bit APP: "%Program Files%\Pd")

 * Create Startmenu entries (optional)

 * Set proper file associations in Windows registry so that .pd files
   are opened with Pure Data and have proper icons in Explorer (optional)

 * Include uninstaller to cleanly wipe Pure Data from Windows (including
   Startmenu and registry entries)

Prerequisites for building the installer
----------------------------------------

* makensis, from the package mingw32-nsis

How to build installer
----------------------

Run the script and specify the directory containing a windows build of
Pure Data:

 ./build-nsi.sh <path-to-pd-dir> <pd-version-number> <arch (32 or 64)>

The scripts dynamically creates a list of all files to be included in the
installer and then runs makensis. The result is an installer named
pd-<VERSION>.windows-installer.exe.  Various parameters can be set in the
file pd.nsi.
.

# Set some defaults
SCRIPTDIR=${0%/*}
OUTDIR=/tmp
WORKDIR=$(mktemp -d || echo /tmp)
INSTALLFILE="${WORKDIR}/install_files_list.nsh"
UNINSTALLFILE="${WORKDIR}/uninstall_files_list.nsh"
LICENSEFILE="${WORKDIR}/license_page.nsh"
NSIFILE="${WORKDIR}/pd.nsi"

error() {
  echo "$@" 1>&2
}

cleanup() {
 if [ "${WORKDIR}" != "/tmp" ]; then
    rm -rf "${WORKDIR}"
 fi
 exit "$1"
}

usage() {
  cat << EOF
Helper script to build a proper Windows installer out of a
Pd build tree.

Usage:
$0 [ OPTIONS ] <path_to_pd_build> [<version> [<wish-exec-name> [<arch>]]]

 Options:

  -o,--outdir DIR      Output directory (where the generated installer binary
                       can be found after a succesfull run)

  -h,--help            Print this help

 Arguments:

  <path_to_pd_build>   input directory (containing 'bin/pd.exe')
                       such as created by 'make app'
  <version>            Pd-version to create installer for (e.g. '0.32-8')
                       DEFAULT: autodetect from <path_to_pd_build>
  <wish-exec-name>     name of the Tcl/Tk interpreter executable
                       e.g., 'wish85.exe'
                       DEFAULT: use a "bin/wish*.exe" in <path_to_pd_build>
  <ARCH>               architecture of the Pd executable.
                       valid values are '32' for x86 and '64' for x86_64
                       DEFAULT: autodetect from "bin/pd.exe" in <path_to_pd_build>
EOF
  cleanup 1
}


# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case $1 in
        -o|--outdir)
            if [  $# = 0 ]; then
                error "-o,--outdir requires a DIR argument"
                cleanup 1
            fi
            shift 1
            OUTDIR="$1"
            ;;
        -h|--help)
            usage
            ;;
        *)
            break ;;
    esac
    shift 1
done

# show usage when invoked without args
if [ $# -lt 1 ]
then
    usage
fi

PDWINDIR=$(realpath "$1")
PDVERSION="$2"
WISHNAME="$3"
PDARCH="$4"

if [ -z "${PDVERSION}" ]; then
    ver=${PDWINDIR##*/}
    if [ -n "${ver}" ] && [ "${ver}" != "${ver#*-}" ]; then
        PDVERSION="${ver#*-}"
    fi
fi

# Check validity of specified PDWINDIR
if ! [ -d "${PDWINDIR}" ]
then
  error "'${PDWINDIR}' is not a directory"
  cleanup 1
else
  for f in pd.exe pd.com pd64.exe pd64.com pd32.exe pd32.com pd.exe; do
    pd_exe="${PDWINDIR}/bin/${f}"
    [ -f "${pd_exe}" ] && break
  done
  if ! [ -f "${pd_exe}" ]; then
	  error "Could not find ${f}. Is this a Windows build?"
	  cleanup 1
  fi
fi

if [  -z "${PDVERSION}" ]; then
    error "could not auto-detect the version from ${PDWINDIR}"
    cleanup 1
fi

# autodetect wishname if none is given
if [ -z "${WISHNAME}" ]; then
	WISHNAME=$(find "${PDWINDIR}/bin" -iname "wish*.exe" -print -quit)

	if [ -n "${WISHNAME}" ]; then
		WISHNAME=$(basename "${WISHNAME}")
	fi
fi
if [ -z "${WISHNAME}" ]; then
    error "Unable to automatically determine <wish-exec-name>."
    cleanup 1
fi

# autodetect architecture if not given on the cmdline
if [  "${PDARCH}" = "" ]; then
    if file -b "${pd_exe}" | grep -E "^PE32 .* 80386 " >/dev/null; then
        PDARCH=32
    elif file -b "${pd_exe}" | grep -E "^PE32\+ .* x86-64 " >/dev/null; then
        PDARCH=64
    fi
fi
if [  "${PDARCH}" = "" ]; then
    error "Unable to automatically determine <arch>."
    cleanup 1
fi

OUTDIR=$(realpath "${OUTDIR}")

#### WRITE INSTALL LIST #########################################
find "${PDWINDIR}" \
  -mindepth 0 \
  -type d \
  -printf 'SetOutPath "$INSTDIR/%P"\n' \
  -exec \
    find {} \
    -maxdepth 1 \
    -type f \
    -printf 'File "%p"\n' \; \
    -not -name "*.o" \
  | sed 's|/|\\|g' \
  > "${INSTALLFILE}"

#### WRITE UNINSTALL FILE LIST ###################################
# clear
echo > "${UNINSTALLFILE}"

# delete files
find "${PDWINDIR}" -type f -printf 'Delete "$INSTDIR/%P"\n' \
  -not -name "*.o" \
  | sed 's|/|\\|g' \
  >> "${UNINSTALLFILE}"

# delete directories
# tac is used to reverse order so that dirs are removed in depth-first order
find "${PDWINDIR}" -type d -printf 'RMDir "$INSTDIR/%P"\n' \
  | tac \
  | sed 's|/|\\|g' \
  >> "${UNINSTALLFILE}"

#### WRITE LICENSE INCLUDE ######################################
echo "!insertmacro MUI_PAGE_LICENSE \"${PDWINDIR}/LICENSE.txt\"" \
  > "${LICENSEFILE}"

# uninstall/license/... information into pd.nsi script
cat "${SCRIPTDIR}/pd.nsi" | sed \
	-e "s|include \"/tmp/|include \"${WORKDIR}/|" \
	-e "s|OutFile \"/tmp/|OutFile \"${OUTDIR}/|" \
	> "${NSIFILE}"

# check if we have nsis compiler
nsis_version=$(makensis -version)
nsis_exit=$?
case $nsis_exit in
  0) error "Found NSIS version $nsis_version"
     ;;
  1) error "makensis returned error. What's the problem?"
     cleanup 1
     ;;
  2) error "NSIS is not found.  install, e.g., mingw32-nsis."
     cleanup 1
     ;;
  *) error "Unknown error"
     cleanup 1
     ;;
esac

# copy installer custom artwork
cp "${SCRIPTDIR}/installer-art/big.bmp" "${WORKDIR}/big.bmp"
cp "${SCRIPTDIR}/installer-art/small.bmp" "${WORKDIR}/small.bmp"
cp "${SCRIPTDIR}/../tcl/pd.ico" "${WORKDIR}/pd.ico"

# run the build
mkdir -p "${OUTDIR}"
if makensis -DPDVER="${PDVERSION}" -DWISHN="${WISHNAME}" -DARCHI="${PDARCH}" \
    -DPDEXE=$(basename "${pd_exe}") "${NSIFILE}" 1>&2
then
  error "Build successful"
  echo "${OUTDIR}/pd-${PDVERSION}.windows-installer.exe"
else
  error "Some error occurred during compilation of ${NSIFILE}"
  error "(files are not cleaned up so you can inspect them)"
  exit 1
fi
cleanup 0

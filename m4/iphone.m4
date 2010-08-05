dnl Copyright (C) 2010 IOhannes m zmölnig
dnl This file is free software; IOhannes m zmölnig
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
# AM_CHECK_IPHONE (ACTION_IF_FOUND, ACTION_IF_NOT_FOUND, [ACTION_IF_FAILED])
# enables the "--with-iphone" flag; 
# if the iPhoneSDK is detected, this will 
#  - set the CC & CPP to the cross-compiler ( resp preprocessor)
#  - as well as the apropriate CFLAGS, INCLUDES and LDFLAGS
#  - these values can also be found in IPHONE_CC, IPHONE_CPP, 
#    IPHONE_INCLUDES, IPHONE_ARCH, IPHONE_CFLAGS, IPHONE_LDFLAGS
#  it will then call ACTION_IF_FOUND
# elif ACTION_IF_FAILED is set and the iPhoneSDK is not detected even though the user
#  requested it (by specifying a path, or simply saying "--with-iphone=yes"), 
#  then ACTION_IF_FAILED is called
# else 
#  ACTION_IF_NOT_FOUND is called
#
# TODO: only run checks when architecture is right (x-compiling)
#        hc's code used "arm*darwin*"
# --------------------------------------------------------------
AC_DEFUN([PD_CHECK_IPHONE],
[
AC_REQUIRE([AC_CANONICAL_HOST])dnl

define([iphonepath_default],[/Developer/Platforms/iPhoneOS.platform/Developer])
define([iphoneversion_default],[3.0])

AC_ARG_WITH([iphone], 
	AS_HELP_STRING( [--with-iphone=</path/to/sdk>],
			[specify iPhone BaseDirectory (only used when building for iPhone) [iphonepath_default]]),
    iphonepath=$withval)
AC_ARG_WITH([iphoneversion], 
	AS_HELP_STRING( [--with-iphoneversion=<sdk-version>],
			[specify iPhone SDK version (only used when building for iPhone) [iphoneversion_default]]),
    iphone_version=$withval)


case $host in
	arm*darwin*)
	;;
	*)
	AC_MSG_NOTICE([iPhone SDK only available for arm-apple-darwin hosts, skipping tests])
	with_iphone="no"
	;;
esac

if test "x${with_iphone}" != "xno" ; then
 AC_MSG_CHECKING([iPhone SDK])

 if test "x${iphonepath}" = "xyes" || test "x${iphonepath}" = "x"; then
   iphonepath=iphonepath_default
 fi
 if test "x${iphone_version}" = "x"; then
   iphone_version=iphoneversion_default
 fi
 
 IPHONE_CC="${iphonepath}/usr/bin/gcc"
 IPHONE_CPP="${iphonepath}/usr/bin/cpp"
 IPHONE_SYSROOT="${iphonepath}/SDKs/iPhoneOS${iphone_version}.sdk"

#echo "sysroot = ${IPHONE_SYSROOT}"
#echo "cpp = ${IPHONE_CPP}"
#echo "cc = ${IPHONE_CC}"

 if test -d "${IPHONE_SYSROOT}" && 
	test -x "${IPHONE_CC}" &&
	test -x "${IPHONE_CPP}"; then
  CC="${IPHONE_CC}"
  CPP="${IPHONE_CPP}"
 
  IPHONE_ARCH="-arch armv6"
  IPHONE_CFLAGS="-miphoneos-version-min=${iphone_version} -isysroot ${IPHONE_SYSROT}"
  IPHONE_LDFLAGS=""
 
  CFLAGS="${CFLAGS} ${IPHONE_CFLAGS} ${IPHONE_ARCH}"
  LDFLAGS="${LDFLAGS} ${IPHONE_ARCH} ${LDFLAGS}"
 
 # workaround for rpl_malloc/rpl_realloc bug in autoconf when cross-compiling
  ac_cv_func_malloc_0_nonnull=yes
  ac_cv_func_realloc_0_nonnull=yes
 
  _iphone_result=yes
 
  AC_MSG_RESULT([yes])
  [$1]
 else
  _iphone_result=no
  IPHONE_CC=""
  IPHONE_CPP=""
  IPHONE_SYSROOT=""
  
  if test "x${with_iphone}" = "x"; then
    with_iphone="no"
  fi
 
  AC_MSG_RESULT([no])
  if test "x${with_iphone}" = "xno"; then
   :
   [$2]
  else
   :
   ifelse([$3], , [$2], [$3])
  fi
 fi
else
# --without-iphone
 [$2]
fi

undefine([iphonepath_default])
undefine([iphoneversion_default])
])
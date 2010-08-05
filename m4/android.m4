dnl Copyright (C) 2010 IOhannes m zmölnig
dnl This file is free software; IOhannes m zmölnig
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
# AM_CHECK_ANDROID (ACTION_IF_FOUND, ACTION_IF_NOT_FOUND, [ACTION_IF_FAILED])
# enables the "--with-android" flag; 
# if the androidSDK is detected, this will 
#  - set the CC & CPP to the cross-compiler ( resp preprocessor)
#  - as well as the apropriate CFLAGS, INCLUDES and LDFLAGS
#  - these values can also be found in ANDROID_CC, ANDROID_CPP, 
#    ANDROID_INCLUDES, ANDROID_ARCH, ANDROID_CFLAGS, ANDROID_LDFLAGS
#  it will then call ACTION_IF_FOUND
# elif ACTION_IF_FAILED is set and the androidSDK is not detected even though the user
#  requested it (by specifying a path, or simply saying "--with-android=yes"), 
#  then ACTION_IF_FAILED is called
# else 
#  ACTION_IF_NOT_FOUND is called
#
# TODO: only run checks when architecture is right (x-compiling)
#        hc's code used "arm-linux"
# --------------------------------------------------------------
AC_DEFUN([PD_CHECK_ANDROID],
[
define([androidpath_default],[${HOME}/src/android/android-ndk-1.6_r1])
define([androidversion_default],[android-4])

AC_ARG_WITH([android], 
	AS_HELP_STRING( [--with-android=</path/to/sdk>],
			[specify path to Android SDK (only used when building for Android) [androidpath_default]]),
    android_path=$withval)
AC_ARG_WITH([androidversion], 
	AS_HELP_STRING( [--with-androidversion=<sdk-version>],
			[specify Android target version (only used when building for Android) [androidversion_default]]),
    android_target_platform=$withval)


case $host in
	arm-linux)
	;;
	*)
	AC_MSG_NOTICE([Android SDK only available for arm-linux hosts, skipping tests])
	with_android="no"
	;;
esac


if test "x${with_android}" != "xno" ; then
 AC_MSG_CHECKING([android SDK])
 
 if test "x${android_path}" = "xyes"; then
   android_path=androidpath_default
 fi
 if test "x${android_target_platform}" = "x"; then
   android_target_platform=androidversion_default
 fi
 
 android_target_arch_abi=arm
 android_gcc_path="${android_path}/build/prebuilt/linux-x86/arm-eabi-4.2.1/bin"
 ANDROID_CPP="${android_gcc_path}/arm-eabi-cpp"
 ANDROID_CC="${android_gcc_path}/arm-eabi-gcc"
 ANDROID_LD="${android_gcc_path}/arm-eabi-ld"
 ANDROID_SYSROOT="${android_path}/build/platforms/${android_target_platform}/arch-${android_target_arch_abi}"
 
 if test -d "${ANDROID_SYSROOT}/usr/include" && 
 	test -x "${ANDROID_CC}" &&
 	test -x "${ANDROID_LD}" &&
 	test -x "${ANDROID_CPP}"; then
  CPP="${ANDROID_CPP}"
  CC="${ANDROID_CC}"
  LD="${ANDROID_LD}"
  android_target_libgcc=$(${ANDROID_CC} -print-libgcc-file-name)
 
 #ah, i don't like all this...
 
 #why do we have to specify _HAVE_UNISTD_H and HAVE_GL: can't we use autoconf tests?
  ANDROID_DEFS="-DANDROID -D__linux__ -D_HAVE_UNISTD_H -DHAVE_LIBGL -D__BSD_VISIBLE=
 1"
  ANDROID_INCLUDES="-I${ANDROID_SYSROOT}/usr/include"
 
 # what broken compiler is this, if we have to specify all the architecture defines manually?
  ANDROID_ARCH_CFLAGS="-march=armv5te -mtune=xscale -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__"
 
 #shan't we test for these flags?
  ANDROID_OPT_CFLAGS=" -msoft-float -fpic -mthumb-interwork -ffunction-sections -funwind-tables -fstack-protector -fno-short-enums -mthumb -fno-strict-aliasing -finline-limit=64 -O2"
 
  ANDROID_CFLAGS="${ANDROID_ARCH_CFLAGS} ${ANDROID_OPT_CFLAGS}"
  ANDROID_LDFLAGS="-nostdlib -Wl,--no-undefined -Wl,-rpath-link=${ANDROID_SYSROOT}/usr/lib ${android_target_libgcc}"
 
  INCLUDES="${INCLUDES} ${ANDROID_INCLUDES}"
  CFLAGS="${CFLAGS} ${ANDROID_CFLAGS}"
  LDFLAGS="${LDFLAGS} ${ANDROID_LDFLAGS}"
 
 # workaround for rpl_malloc/rpl_realloc bug in autoconf when cross-compiling
  ac_cv_func_malloc_0_nonnull=yes
  ac_cv_func_realloc_0_nonnull=yes
 
  AC_MSG_RESULT([yes])
  [$1]
 else
  ANDROID_CPP=""
  ANDROID_CC=""
  ANDROID_LD=""
  ANDROID_SYSROOT=""
  
  if test "x${with_android}" = "x"; then
    with_android="no"
  fi
 
  AC_MSG_RESULT([no])
  if test "x${with_android}" = "xno"; then
   :
   [$2]
  else
   :
   ifelse([$3], , [$2], [$3])
  fi
 fi
else
# --without-android
 [$2]
fi
undefine([androidpath_default])
undefine([androidversion_default])
])
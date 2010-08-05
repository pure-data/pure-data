dnl Copyright (C) 2005-2010 IOhannes m zmölnig
dnl This file is free software; IOhannes m zmölnig
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# PD_CHECK_UNIVERSAL([VARIABLE-NAME], [ACTION-IF-SUCCESS], [ACTION-IF-NO-SUCCESS])
# will enable the "--enable-universal=<ARCHS>" flag
# if <ARCH> is "yes", platform defaults are used
# the system tries to build a test program with the archs, on succes it calls ACTION-IF-SUCCESS, and ACTION-IF-NO-SUCCESS otherwise
# on success it will also add the flags to:
# [VARIABLE-NAME]_CFLAGS will hold a list of cflags to compile for all requested archs
# [VARIABLE-NAME]_LDFLAGS will hold a list of ldflags to link for all requested archs

AC_DEFUN([PD_CHECK_UNIVERSAL],
[
AC_REQUIRE([AC_CANONICAL_HOST])dnl

AC_ARG_ENABLE(universal,
       [  --enable-universal=<ARCHS>
                          build an Apple Multi Architecture Binary (MAB);
                          ARCHS is a comma-delimited list of architectures for
                          which to build; if ARCHS is omitted, then the package
                          will be built for all architectures supported by the
                          platform (e.g. "ppc,i386" for MacOS/X and Darwin); 
                          if this option is disabled or omitted entirely, then
                          the package will be built only for the target 
                          platform],
       [universal_binary=$enableval], [universal_binary=no])

if test "$universal_binary" != no; then
    AC_MSG_CHECKING([target architectures])

    # Respect TARGET_ARCHS setting from environment if available.
    if test -z "$TARGET_ARCHS"; then
     # Respect ARCH given to --enable-universal-binary if present.
     if test "$universal_binary" != yes; then
	    TARGET_ARCHS=$(echo "$universal_binary" | tr ',' ' ')
     else 
	    # Choose a default set of architectures based upon platform.
	case $host in
	*darwin*)
          TARGET_ARCHS="ppc i386"
	;;
	*)
	  TARGET_ARCHS=""
	;;
	esac
     fi
    fi
    AC_MSG_RESULT([$TARGET_ARCHS])

   # /usr/lib/arch_tool -archify_list $TARGET_ARCHS
   _pd_universal=""
   for archs in $TARGET_ARCHS 
   do
    _pd_universal="$_pd_universal -arch $archs"
   done

dnl run checks whether the compiler linker like this...
   if test "x$_pd_universal" != "x"; then
    tmp_arch_cflags="$CFLAGS"
    tmp_arch_ldflags="$LDFLAGS"
    
    CFLAGS="$CFLAGS $_pd_universal"
    LDFLAGS="$LDFLAGS $_universal"
    AC_TRY_LINK([], [return 0;], [
        if test "x$1" != "x"; then
	  $1[]_CFLAGS="$[]$1[]_CFLAGS $_pd_universal"
	  $1[]_LDFLAGS="$[]$1[]_LDFLAGS $_pd_universal"
	  AC_SUBST($1[]_CFLAGS)
	  AC_SUBST($1[]_LDFLAGS)
        fi
	[$2]
	], [
	:
	[$3]
	])

    CFLAGS="$tmp_arch_cflags"
    LDFLAGS="$tmp_arch_ldflags"
   else
    :
   [$3]
   fi
else
 :
 [$3]
fi
])# GEM_CHECK_UNIVERSAL

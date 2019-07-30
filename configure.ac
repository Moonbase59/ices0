# configure script for ices
# $Id: configure.ac 8764 2005-01-16 18:47:32Z brendan $

m4_define(ICES_MAJOR, 0)
m4_define(ICES_MINOR, 4)
m4_define(ICES_MICRO, 6)
m4_define(ICES_VERSION,
  m4_if(ICES_MICRO, 0, ICES_MAJOR.ICES_MINOR, ICES_MAJOR.ICES_MINOR.ICES_MICRO))

AC_INIT([ices], ICES_VERSION, [icecast@xiph.org])
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR([src/ices.c])

AC_CONFIG_FILES([Makefile src/Makefile src/playlist/Makefile conf/Makefile
 conf/ices.conf.dist doc/Makefile])

VERSION=ICES_VERSION

dnl -- Finally, actual autoconf stuff --

AC_ARG_PROGRAM

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

if test "$GCC" = yes
then
  CFLAGS="$CFLAGS -Wall"
fi

dnl -- System check --

AC_CANONICAL_HOST
AC_AIX
AC_ISC_POSIX
AC_MINIX

case $host in
  *hpux*)
    CFLAGS="$CFLAGS -D_HPUX_SOURCE -DHPUX"
    if test "$CC" = cc
    then
      CFLAGS="$CFLAGS -Aa"
    fi
  ;;
esac

dnl -- Compiler feature check --

AC_C_CONST
AC_C_BIGENDIAN
AC_C_INLINE

dnl -- System header check --

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([errno.h fcntl.h signal.h sys/signal.h sys/socket.h \
  sys/stat.h sys/time.h sys/types.h unistd.h])
AC_HEADER_TIME

AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL

dnl -- System function check --

AC_FUNC_STRFTIME
AC_CHECK_FUNCS([vsnprintf setsid setlinebuf])

dnl -- Build system init --

AM_INIT_AUTOMAKE([1.6 foreign])
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_ARG_WITH(docdir,
  [[  --with-docdir=DIR       put documentation in DIR [PREFIX/doc/ices]]],
  [ices_cv_docdir="$with_docdir"],
  [ices_cv_docdir='${prefix}/doc/ices'])

docdir="$ices_cv_docdir"
AC_SUBST(docdir)

AC_ARG_WITH(moddir,
  [[  --with-moddir=DIR       put playlist modules in DIR [SYSCONFDIR/modules]]],
  [ices_cv_moddir="$with_moddir"],
  [ices_cv_moddir='${sysconfdir}/modules'])

moddir="$ices_cv_moddir"
AC_SUBST(moddir)

dnl -- Required extra libraries --

XIPH_PATH_SHOUT(, [AC_MSG_ERROR([Could not find a usable libshout])])
CFLAGS="$CFLAGS $SHOUT_CPPFLAGS $SHOUT_CFLAGS"
LIBS="$LIBS $SHOUT_LIBS"

dnl -- Optional features --
AC_CACHE_SAVE

AC_ARG_WITH(xml-config,
  [  --with-xml-config=PATH  use xml-config in PATH to find libxml ])

have_xml="no"
if test "$with_xml_config" != "no"
then
  if test -n "$with_xml_config" -a "$with_xml_config" != "yes"
  then
    XMLCONFIG="$with_xml_config"
    if ! test -x "$XMLCONFIG"
    then
      AC_MSG_ERROR([$with_xml_config cannot be run])
    fi
  else
    AC_CHECK_PROGS(XMLCONFIG, [xml2-config xml-config])
  fi

  if test -n "$XMLCONFIG"
  then
    AC_DEFINE(HAVE_LIBXML, 1, [Define if you have the GNOME XML library])
    LIBS="$LIBS `$XMLCONFIG --libs`"
    CPPFLAGS="$CPPFLAGS `$XMLCONFIG --cflags`"
    ICES_OBJECTS="$ICES_OBJECTS ices_config.o"
    have_xml="yes"
  else
    AC_MSG_RESULT([Compiling without libxml support - no configfiles])
  fi
fi
AM_CONDITIONAL(USE_XML, test "$have_xml" = "yes")

AC_ARG_WITH([python],
  [[  --with-python[=PATH]    include python scripting support]],
  [enable_python="$with_python"],
  [enable_python="yes"])

have_python="no"
if test "$enable_python" != "no"
then
  XIPH_PATH_PYTHON([$enable_python])
  if test -n "$PYTHON_LIBS"
  then
    have_python="yes"
    CPPFLAGS="$CPPFLAGS $PYTHON_CPPFLAGS"
    LDFLAGS="$LDFLAGS $PYTHON_LDFLAGS"
    LIBS="$LIBS $PYTHON_LIBS"
    PLAYLIST_OBJECTS="$PLAYLIST_OBJECTS pm_python.o"
    AC_DEFINE(HAVE_LIBPYTHON, 1, [Define if you have the python library])
  fi
fi

if test "$with_python" != "no" -a "$have_python" != "yes"
then
  if test -n "$with_python"
  then
    AC_MSG_ERROR([Could not link to python])
  else
    AC_MSG_RESULT([Could not link to python, python scripting disabled])
  fi
fi

AC_ARG_WITH(perl,
  [[  --with-perl[=PATH]      include perl scripting support]])

have_perl="no"
PERL=perl
if test "$with_perl" != "no"
then
  if test -n "$with_perl" -a "$with_perl" != "yes"
  then
    PERL="$with_perl"
    if ! test -x "$PERL"
    then
      AC_MSG_ERROR([Perl not found in the specified location])
    fi
  fi

  PERLCFLAGS=`$PERL -MExtUtils::Embed -e ccopts`
  PERLLIBS=`$PERL -MExtUtils::Embed -e ldopts`

  saved_CPPFLAGS="$CPPFLAGS"
  saved_LIBS="$LIBS"
  CPPFLAGS="$CPPFLAGS $PERLCFLAGS"
  LIBS="$LIBS $PERLLIBS"

  AC_CHECK_FUNCS(perl_construct)
  if test "$ac_cv_func_perl_construct" != "no"
  then
    have_perl="yes"
    PLAYLIST_OBJECTS="$PLAYLIST_OBJECTS pm_perl.o"
    AC_DEFINE(HAVE_LIBPERL, 1, [Define if you have perl libraries])
  else
    CPPFLAGS="$saved_CPPFLAGS"
    LIBS="$saved_LIBS"
  fi
fi

if test "$with_perl" != "no" -a "$have_perl" != "yes"
then
  if test -n "$with_perl"
  then
    AC_MSG_ERROR([Could not link to perl])
  else
    AC_MSG_RESULT([Could not link to perl, perl scripting disabled])
  fi
fi

AC_ARG_WITH(lame,
  [[  --with-lame[=DIR]       enable support for reencoding with lame [in DIR]]])

have_LAME="no"
if test "$with_lame" != "no"
then
  if test -n "$with_lame" -a "$with_lame" != "yes"
  then
    CPPFLAGS="$CPPFLAGS -I$with_lame/include"
    LDFLAGS="$LDFLAGS -L$with_lame/lib"
  fi

  AC_CHECK_HEADER(lame/lame.h, [
    have_LAME="maybe",
    AC_DEFINE(HAVE_LAME_LAME_H, 1, [Use <lame/lame.h> instead of <lame.h>])
    ], [
    AC_CHECK_HEADER(lame.h, [
    have_LAME="maybe"])])

  if test "$have_LAME" != "no"
  then
    AC_CHECK_LIB(mp3lame, lame_encode_flush_nogap, [
      have_LAME="yes"
      LIBS="$LIBS -lmp3lame"
      LIBM="-lm"
      ICES_OBJECTS="$ICES_OBJECTS reencode.o"
      AC_DEFINE(HAVE_LIBLAME, 1, [Define if you have the LAME MP3 library])

      AC_CHECK_FUNCS([lame_decode_exit])
      ], [have_LAME="no"], -lm)
  fi
fi

if test "$with_lame" != "no" -a "$have_LAME" != "yes"
then
  if test -n "$with_lame"
  then
    AC_MSG_ERROR([Could not find a valid LAME library installation])
  else
    AC_MSG_RESULT([Could not find a valid LAME library, reencoding disabled])
  fi
fi

AC_ARG_WITH(vorbis,
  [[  --with-vorbis[=DIR]     support for vorbis transcoding using libvorbisfile [in DIR]]])

if test "$have_LAME" != "yes"
then
  if test -n "$with_vorbis" -a "$with_vorbis" != "no"
  then
    AC_MSG_ERROR([Vorbis cannot be enabled without LAME])
  elif test "$with_vorbis" != "no"
  then
    AC_MSG_RESULT([Vorbis is disabled because LAME is not enabled])
    with_vorbis="no"
  fi
fi

have_vorbis="no"
if test "$with_vorbis" != "no"
then
  if test -n "$with_vorbis" -a "$with_vorbis" != "yes"
  then
    CPPFLAGS="$CPPFLAGS -I$with_vorbis/include"
    LDFLAGS="$LDFLAGS -L$with_vorbis/lib"
  fi

  AC_CHECK_HEADER(vorbis/vorbisfile.h, have_vorbis="maybe")
  if test "$have_vorbis" != "no"
  then
    AC_CHECK_LIB(vorbisfile, ov_open, [
      LIBS="$LIBS -lvorbisfile -lvorbis -logg"
      AC_DEFINE(HAVE_LIBVORBISFILE, 1, [Define if you have libvorbisfile])
      ICES_OBJECTS="$ICES_OBJECTS in_vorbis.o"
      have_vorbis="yes"
    ],[have_vorbis="no"],-lvorbis -logg)
  fi
fi

if test "$with_vorbis" != "no" -a "$have_vorbis" != "yes"
then
  if test -n "$with_vorbis"
  then
    AC_MSG_ERROR([Could not find libvorbisfile])
  else
    AC_MSG_RESULT([Could not find libvorbisfile, vorbis support disabled])
  fi
fi

AC_ARG_WITH(faad,
  [[  --with-faad[=DIR]     support for MP4 (AAC) transcoding using libfaad [in DIR]]])

if test "$have_LAME" != "yes"
then
  if test -n "$with_faad" -a "$with_faad" != "no"
  then
    AC_MSG_ERROR([MP4 transcoding cannot be enabled without LAME])
  elif test "$with_faad" != "no"
  then
    AC_MSG_RESULT([MP4 transcoding is disabled because LAME is not enabled])
    with_faad="no"
  fi
fi

have_faad="no"
if test "$with_faad" != "no"
then
  if test -n "$with_faad" -a "$with_faad" != "yes"
  then
    CPPFLAGS="$CPPFLAGS -I$with_faad/include"
    LDFLAGS="$LDFLAGS -L$with_faad/lib"
  fi

  AC_CHECK_HEADER(faad.h, have_faad="maybe")
  if test "$have_faad" != "no"
  then
    AC_CHECK_LIB(mp4v2, MP4Read, [
      LIBS="$LIBS -lmp4v2 -lfaad"
      AC_DEFINE(HAVE_LIBFAAD, 1, [Define if you have libfaad])
      ICES_OBJECTS="$ICES_OBJECTS in_mp4.o"
      have_faad="yes"
    ],[have_faad="no"])
  fi
fi

if test "$with_faad" != "no" -a "$have_faad" != "yes"
then
  if test -n "$with_faad"
  then
    AC_MSG_ERROR([Could not find libfaad])
  else
    AC_MSG_RESULT([Could not find libfaad, MP4 transcoding disabled])
  fi
fi

AC_ARG_WITH(flac,
  [[  --with-flac[=DIR]     support for FLAC transcoding using libFLAC [in DIR]]])

if test "$have_LAME" != "yes"
then
  if test -n "$with_flac" -a "$with_flac" != "no"
  then
    AC_MSG_ERROR([FLAC transcoding cannot be enabled without LAME])
  elif test "$with_flac" != "no"
  then
    AC_MSG_RESULT([FLAC transcoding is disabled because LAME is not enabled])
    with_flac="no"
  fi
fi

have_flac="no"
if test "$with_flac" != "no"
then
  if test -n "$with_flac" -a "$with_flac" != "yes"
  then
    CPPFLAGS="$CPPFLAGS -I$with_flac/include"
    LDFLAGS="$LDFLAGS -L$with_flac/lib"
  fi

  AC_CHECK_HEADER([FLAC/stream_decoder.h], have_flac="maybe")
  if test "$have_flac" != "no"
  then
    AC_CHECK_LIB(FLAC, FLAC__stream_decoder_new, [
      LIBS="$LIBS -lFLAC"
      AC_DEFINE(HAVE_LIBFLAC, 1, [Define if you have libFLAC])
      ICES_OBJECTS="$ICES_OBJECTS in_flac.o"
      have_flac="yes"
    ],[have_flac="no"])
  fi
fi

if test "$with_flac" != "no" -a "$have_flac" != "yes"
then
  if test -n "$with_flac"
  then
    AC_MSG_ERROR([Could not find libFLAC])
  else
    AC_MSG_RESULT([Could not find libFLAC, FLAC transcoding disabled])
  fi
fi

dnl -- and finish up --

LIBS="$LIBS $LIBM $LIBDL"
AC_SUBST(ICES_OBJECTS)
AC_SUBST(PLAYLIST_OBJECTS)

AC_OUTPUT

AC_MSG_RESULT([Compiling with: $CPPFLAGS $CFLAGS])
AC_MSG_RESULT([Linking with: $LDFLAGS $LIBS])
AC_MSG_RESULT([Features:])
AC_MSG_RESULT([  XML     : $have_xml])
AC_MSG_RESULT([  Python  : $have_python])
AC_MSG_RESULT([  Perl    : $have_perl])
AC_MSG_RESULT([  LAME    : $have_LAME])
AC_MSG_RESULT([  Vorbis  : $have_vorbis])
AC_MSG_RESULT([  MP4     : $have_faad])
AC_MSG_RESULT([  FLAC    : $have_flac])

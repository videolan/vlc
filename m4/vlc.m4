dnl  Macros needed for VLC

dnl  Add plugins or builtins
AC_DEFUN([AX_ADD_BUILTINS], [
  BUILTINS="${BUILTINS} $1"
])

AC_DEFUN([AX_ADD_PLUGINS], [
  PLUGINS="${PLUGINS} $1"
])

dnl  Save and restore default flags
AC_DEFUN([AX_SAVE_FLAGS], [
  CPPFLAGS_save="${CPPFLAGS}"
  CFLAGS_save="${CFLAGS}"
  CXXFLAGS_save="${CXXFLAGS}"
  OBJCFLAGS_save="${OBJCFLAGS}"
  LDFLAGS_save="${LDFLAGS}"
])

AC_DEFUN([AX_RESTORE_FLAGS], [
  CPPFLAGS="${CPPFLAGS_save}"
  CFLAGS="${CFLAGS_save}"
  CXXFLAGS="${CXXFLAGS_save}"
  OBJCFLAGS="${OBJCFLAGS_save}"
  LDFLAGS="${LDFLAGS_save}"
])

dnl  Special cases: vlc, pics, plugins, save
AC_DEFUN([AX_ADD_CPPFLAGS], [
  for element in [$1]; do
    eval "CPPFLAGS_${element}="'"$'"{CPPFLAGS_${element}} $2"'"'
    am_plugins_with_cppflags="${am_plugins_with_cppflags} ${element}"
  done
])

AC_DEFUN([AX_ADD_CFLAGS], [
  for element in [$1]; do
    eval "CFLAGS_${element}="'"$'"{CFLAGS_${element}} $2"'"'
    am_plugins_with_cflags="${am_plugins_with_cflags} ${element}"
  done
])

AC_DEFUN([AX_ADD_CXXFLAGS], [
  for element in [$1]; do
    eval "CXXFLAGS_${element}="'"$'"{CXXFLAGS_${element}} $2"'"'
    am_plugins_with_cxxflags="${am_plugins_with_cxxflags} ${element}"
  done
])

AC_DEFUN([AX_ADD_OBJCFLAGS], [
  for element in [$1]; do
    eval "OBJCFLAGS_${element}="'"$'"{OBJCFLAGS_${element}} $2"'"'
    am_plugins_with_objcflags="${am_plugins_with_objcflags} ${element}"
  done
])

AC_DEFUN([AX_ADD_LDFLAGS], [
  for element in [$1]; do
    eval "LDFLAGS_${element}="'"$'"{LDFLAGS_${element}} $2"'"'
    am_plugins_with_ldflags="${am_plugins_with_ldflags} ${element}"
  done
])

AC_DEFUN([AX_OUTPUT_VLC_CONFIG_IN], [

  AC_MSG_RESULT(configure: creating ./vlc-config.in)

  rm -f vlc-config.in
  sed -ne '/#@1@#/q;p' < "${srcdir}/vlc-config.in.in" \
    | sed -e "s/@gprof@/${enable_gprof}/" \
          -e "s/@cprof@/${enable_cprof}/" \
          -e "s/@optim@/${enable_optimizations}/" \
          -e "s/@debug@/${enable_debug}/" \
          -e "s/@release@/${enable_release}/" \
          -e "s/@PLUGINS@/${PLUGINS}/" \
          -e "s/@BUILTINS@/${BUILTINS}/" \
          -e "s/@CFLAGS_TUNING@/${CFLAGS_TUNING}/" \
          -e "s/@CFLAGS_OPTIM@/${CFLAGS_OPTIM}/" \
          -e "s/@CFLAGS_OPTIM_NODEBUG@/${CFLAGS_OPTIM_NODEBUG}/" \
          -e "s/@CFLAGS_NOOPTIM@/${CFLAGS_NOOPTIM}/" \
    > vlc-config.in

  dnl  Switch/case loop
  for x in `echo ${am_plugins_with_ldflags}`
  do [
    echo "    ${x})" >> vlc-config.in
    if test -n "`eval echo '$'CPPFLAGS_${x}`"; then
      echo "      cppflags=\"\${cppflags} `eval echo '$'CPPFLAGS_${x}`\"" >> vlc-config.in
    fi
    if test -n "`eval echo '$'CFLAGS_${x}`"; then
      echo "      cflags=\"\${cflags} `eval echo '$'CFLAGS_${x}`\"" >> vlc-config.in
    fi
    if test -n "`eval echo '$'CXXFLAGS_${x}`"; then
      echo "      cxxflags=\"\${cxxflags} `eval echo '$'CXXFLAGS_${x}`\"" >> vlc-config.in
      if test "${x}" != "plugins" -a "${x}" != "builtins"; then
        echo "      linkage=\"c++\"" >> vlc-config.in
      fi
    fi
    if test -n "`eval echo '$'OBJCFLAGS_${x}`"; then
      echo "      objcflags=\"\${objcflags} `eval echo '$'OBJCFLAGS_${x}`\"" >> vlc-config.in
      if test "${x}" != "plugins" -a "${x}" != "builtins"; then
        echo "      if test \"\${linkage}\" = \"c\"; then linkage=\"objc\"; fi" >> vlc-config.in
      fi
    fi
    if test -n "`eval echo '$'LDFLAGS_${x}`"; then
      echo "      ldflags=\"\${ldflags} `eval echo '$'LDFLAGS_${x}`\"" >> vlc-config.in
    fi
    echo "    ;;" >> vlc-config.in
  ] done

  dnl  '/#@1@#/,/#@2@#/{/#@.@#/d;p}' won't work on OS X
  sed -ne '/#@1@#/,/#@2@#/p' < "${srcdir}/vlc-config.in.in" \
   | sed -e '/#@.@#/d' >> vlc-config.in

  AX_VLC_CONFIG_HELPER

  dnl  '/#@2@#/,${/#@.@#/d;p}' won't work on OS X
  sed -ne '/#@2@#/,$p' < "${srcdir}/vlc-config.in.in" \
   | sed -e '/#@.@#/d' >> vlc-config.in
])


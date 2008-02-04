# - vlc_add_compile_flag(<_target> "flags...")
# From KDELibs
# Copyright (c) 2006, Oswald Buddenhagen, <ossi@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


MACRO (vlc_add_compile_flag _target _flg)

   GET_TARGET_PROPERTY(_flags ${_target} COMPILE_FLAGS)
   if (_flags)
      set(_flags "${_flags} ${_flg}")
   else (_flags)
      set(_flags "${_flg}")
   endif (_flags)
   SET_TARGET_PROPERTIES(${_target} PROPERTIES COMPILE_FLAGS "${_flags}")

ENDMACRO (vlc_add_compile_flag)

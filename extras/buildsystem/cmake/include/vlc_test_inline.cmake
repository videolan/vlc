# This handy test is from Jack Kelly on the cmake email list.
# Here is a minimal working example. It tests the inline keyword,
# then __inline__ and then __inline. When it finds one that works,
# it will ADD_DEFINITIONS(-Dinline=${KEYWORD}) and if none work, it
# will ADD_DEFINITIONS(-Dinline=).

# Inspired from /usr/share/autoconf/autoconf/c.m4
include (CheckCSourceCompiles)
FOREACH(KEYWORD "inline" "__inline__" "__inline")
   IF(NOT DEFINED C_INLINE)
     check_c_source_compiles(
       "typedef int foo_t; static inline foo_t static_foo(){return 0;}
        foo_t foo(){return 0;} int main(int argc, char *argv[]){return 0;}"
        C_HAS_${KEYWORD})
     IF(C_HAS_${KEYWORD})
       SET(C_INLINE TRUE)
       ADD_DEFINITIONS("-Dinline=${KEYWORD}")
     ENDIF(C_HAS_${KEYWORD})
   ENDIF(NOT DEFINED C_INLINE)
ENDFOREACH(KEYWORD)
IF(NOT DEFINED C_INLINE)
   ADD_DEFINITIONS("-Dinline=")
ENDIF(NOT DEFINED C_INLINE)

flexml -SH -a skin.act skin.dtd

sed -e 's/\([SE]Tag_.*\)(void)/\1(void *pContext)/' \
    -e 's/int main().*//' skin.c > skin.c.new && mv -f skin.c.new skin.c
sed -e 's/\([SE]Tag_.*\)(void)/\1(void*)/' \
    -e 's/extern int yylex(void)/extern int yylex(void*)/' \
    -e 's/\/\* XML processor entry point. \*\//#define YY_DECL int yylex(void *pContext)/' skin.h > skin.h.new && mv -f skin.h.new skin.h


flex -oflex.c -BLs skin.l
sed -e 's/\([SE]Tag_[^()]*\)()/\1(pContext)/g' flex.c > flex.c.new && mv -f flex.c.new flex.c 

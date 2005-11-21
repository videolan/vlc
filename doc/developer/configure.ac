dnl $Id$
dnl Process this file with autoconf to produce a configure script.

AC_INIT(manual.xml.in,0.0.1)

AC_CHECK_PROGS(JADE, openjade jade, [nogood])

if test "$JADE" = nogood ; then
  AC_MSG_ERROR(You need jade to build the documentation. 
  Get it from http://openjade.sourceforge.net/)
else
  DOCS="manual.html"
fi

AC_PATH_PROG(LYNX, lynx, [nope])

if test "$LYNX" = nope ; then
  AC_MSG_WARN(Can not create text version of manual)
else 
  DOCS="$DOCS manual.txt"
fi

AC_PATH_PROG(DVIPS, dvips, [nodvips])
AC_PATH_PROG(JADETEX, jadetex, [nojadetex])

if test "$DVIPS" = nodvips -o "$JADETEX" = nojadetex ; then
  AC_MSG_WARN(Can not create PostScript version of manual)
else 
  DOCS="$DOCS manual.ps"
fi

AC_CANONICAL_SYSTEM
dnl ========================================================================
case "$host_os" in
  linux* ) 
    if test -f /etc/redhat-release ; then
      XML_DECL=/usr/share/sgml/docbook/dsssl-stylesheets-1.76/dtds/decls/xml.dcl
      HTML_SS=/usr/share/sgml/docbook/dsssl-stylesheets-1.76/html/docbook.dsl
      PRINT_SS=/usr/share/sgml/docbook/dsssl-stylesheets-1.76/print/docbook.dsl
      DOCBOOKX_DTD=/usr/share/sgml/docbook/xml-dtd-4.1.2/docbookx.dtd
    else 
      # Assume Debian
      XML_DECL=/usr/lib/sgml/declaration/xml.decl
      HTML_SS=/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl
      PRINT_SS=/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/print/docbook.dsl
      DOCBOOKX_DTS=/usr/share/sgml/docbook/dtd/xml/4.1/docbookx.dtd
    fi
    ;;	
  darwin5* | darwin6* )
    XML_DECL=/sw/share/sgml/dsssl/docbook-dsssl-nwalsh/dtds/decls/xml.dcl
    HTML_SS=/sw/share/sgml/dsssl/docbook-dsssl-nwalsh/html/docbook.dsl
    PRINT_SS=/sw/share/sgml/dsssl/docbook-dsssl-nwalsh/print/docbook.dsl
    DOCBOOKX_DTD=/sw/share/xml/dtd/docbookx/4.1.2/docbookx.dtd
    ;;	
  * ) 
    echo "Don't know $host_os. Picking up from environment."
    XML_DECL=$XML_DECL
    HTML_SS=$HTML_SS
    PRINT_SS=$PRINT_SS
esac

AC_ARG_WITH(xml_decl,
[  --with-xml-dcl=PATH    place to find xml.dcl file])
if test -n "${xml_decl}"; then
   XML_DECL=$xml_decl
fi

if test ! -f $XML_DECL ; then
  AC_MSG_ERROR(Can't find xml.dcl file. Set with --with-xml-dcl.)
fi


AC_ARG_WITH(html_ss,
[  --with-html-ss=PATH    place to find html/docbook.dsl file])
if test -n "${html_ss}"; then
   HTML_SS=$html_ss
fi

if test ! -f $HTML_SS ; then
  AC_MSG_ERROR(Can't find html/docbook.dsl file. Set with --with-html-ss.)
fi

AC_ARG_WITH(print_ss,
[  --with-print-ss=PATH    place to find print/docbook.dsl file])
if test -n "${print_ss}"; then
   PRINT_SS=$print_ss
fi

if test ! -f $PRINT_SS ; then
  AC_MSG_ERROR(Can't find print/docbook.dsl file. Set with --with-print-ss.)
fi

AC_ARG_WITH(docbook_dtd,
[  --with-dockbook-dtd=PATH    place to find docbookx.dtd file])
if test -n "${docbook_dtd}"; then
   DOCBOOKX_DTD=$docbook_dtd
fi

if test ! -f $DOCBOOKX_DTD ; then
  AC_MSG_ERROR(Can't find docbook.dsl file. Set with --with-docbook_dtd.)
fi

AC_SUBST(XML_DECL)
AC_SUBST(HTML_SS)
AC_SUBST(PRINT_SS)
AC_SUBST(DOCBOOKX_DTD)
AC_SUBST(JADE)
AC_SUBST(LYNX)
AC_SUBST(DOCS)

AC_CONFIG_FILES([manual.xml Makefile])
AC_OUTPUT

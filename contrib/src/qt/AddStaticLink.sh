#! /bin/sh

# Add a Qt plugin  in the static pkg-config configuration of a Qt module
# By default plugins are found in $PREFIX/plugins but are not seen by pkg-config.
# This is also done for qml plugins which are found in $PREFIX/qml.
#
# This could also be done in configure.ac to detect what plugins are available and where to add them

REAL_PREFIX="$1"
if [ ! `cygpath.exe -pm / || echo FAIL` = "FAIL" ]; then
    REAL_PREFIX=`cygpath.exe -pm ${REAL_PREFIX}`
fi

PREFIX=$(python3 -c "import os; print(os.path.realpath('${REAL_PREFIX}'))")
PLUGIN_PATH="$3"
PLUGIN_NAME="$4"

PC_DEST="${PREFIX}/lib/pkgconfig/${2}.pc"
PRL_SOURCE=${PREFIX}/${PLUGIN_PATH}/lib${PLUGIN_NAME}.prl

if [ ! -f $PC_DEST ]; then
    echo "destination ${PC_DEST} doesn't exists" >&2
    exit 1
fi

if [ ! -f $PRL_SOURCE ]; then
    PRL_SOURCE=${PREFIX}/${PLUGIN_PATH}/${PLUGIN_NAME}.prl
    if [ ! -f $PRL_SOURCE ]; then
        echo "source ${PRL_SOURCE} doesn't exists" >&2
        exit 1
    fi
fi

# Get the links flags necessary to use the plugin from the installed PRL file of the plugin
# replace hardcoded pathes by {libdir}
LIBS=$(sed -e "/QMAKE_PRL_LIBS/ { \
             s/QMAKE_PRL_LIBS =//; \
             s@$PREFIX/lib@\${libdir}@g; \
             s@\$\$\[QT_INSTALL_LIBS\]@\${libdir}@g;" -e "p" \
         -e "};" -e "d"  $PRL_SOURCE )

# prepend the plugin that uses the module
sed -i.bak -e "s# -l${2}# -l${PLUGIN_NAME} -l${2}#" $PC_DEST
# add the plugin static dependencies to the ones of the module
sed -i.bak -e "s#Libs.private: #Libs.private: $LIBS -L\${prefix}/${PLUGIN_PATH} #" $PC_DEST

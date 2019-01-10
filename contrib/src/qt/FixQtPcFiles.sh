#! /bin/sh

# Use the Libs.private from the prl files that don't have debug symbols and use a proper lib order
LIBS=$(sed -e '/QMAKE_PRL_LIBS/ { s/QMAKE_PRL_LIBS =//; s/\$\$\[QT_INSTALL_LIBS\]/${libdir}/g; p }; d'  $1 )
sed -i \
	-e "s#Libs.private:.*#Libs.private: $LIBS#" \
	-e "/Libs:/ { s/d / / }" $2

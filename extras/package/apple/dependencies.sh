#!/usr/bin/env sh
set -eu

MODE=$1
LTFILE=$2

extract_dependencies()
{
    #>&2 echo "PARSING FILE $1"
    filename=$1

    dependencies=`grep "dependency_libs=" "$filename"| cut -d= -f2-`
    dependencies="${dependencies%\'}"; dependencies="${dependencies#\'}"

    LIBRARY_PATH=""
    for dependency in $dependencies; do
        case $dependency in -L*)
            LIBRARY_PATH="$LIBRARY_PATH ${dependency#-L}" ;;
        esac
    done

    STATIC_DEPENDENCIES=""
    FORWARD_DEPENDENCIES=""
    for dependency in $dependencies; do
        #>&2 echo " - parsing dependency $dependency"
        case $dependency in
            -l*)
        DEPENDENCY_LIBS=""
        for path in $LIBRARY_PATH; do
            baselibpath="$path/lib${dependency#-l}"
            if [ -f "${baselibpath}.a" ]; then
                DEPENDENCY_LIBS="$DEPENDENCY_LIBS ${baselibpath}.a"; break
            elif [  -f ${baselibpath}.la ]; then
                if grep installed=yes ${baselibpath}.la > /dev/null; then
                    DEPENDENCY_LIBS="$DEPENDENCY_LIBS $(extract_dependencies ${baselibpath}.la)"
                fi
            fi
        done
        if [ -z "$DEPENDENCY_LIBS" ]; then
            FORWARD_DEPENDENCIES="$FORWARD_DEPENDENCIES $dependency"
        else
            STATIC_DEPENDENCIES="$STATIC_DEPENDENCIES $DEPENDENCY_LIBS"
        fi ;;

            *.a) STATIC_DEPENDENCIES="$STATIC_DEPENDENCIES $dependency" ;;
            *.la) if grep installed=yes $dependency > /dev/null; then
                libpath=`grep old_library $dependency | cut -d= -f2`
                libpath="${libpath%\'}"; libpath="${libpath#\'}"
                case $MODE in
                    static) STATIC_DEPENDENCIES="$STATIC_DEPENDENCIES $(dirname $dependency)/$libpath $(extract_dependencies $dependency)" ;;
                    forward) FORWARD_DEPENDENCIES="$FORWARD_DEPENDENCIES $(extract_dependencies $dependency)" ;;
                esac
            fi
        esac
    done

    case "$MODE" in
        forward) echo $FORWARD_DEPENDENCIES;;
        static) echo $STATIC_DEPENDENCIES;;
    esac
}

extract_dependencies $LTFILE

#! /bin/sh
set -e

SCRIPT_PATH="$( cd "$(dirname "$0")" ; pwd -P )"
ROOT_DIR="${SCRIPT_PATH}/../../.."

usage()
{
cat << EOF
usage: $0 [options]

Build vlc snap package

OPTIONS:
   -h            Show some help
   -p            Generate prebuilt contrib package
EOF
}

SAVE_PREBUILT=""
while getopts "hp" OPTION
do
    case $OPTION in
        p)
            SAVE_PREBUIT="yes"
        ;;
        h)
            usage
            exit 1
        ;;
    esac
done

if [ "$SAVE_PREBUIT" = "yes" ]; then
    touch ${ROOT_DIR}/snap-save-prebuilt
else
    if [ -e ${ROOT_DIR}/snap-save-prebuilt ]; then
        rm ${ROOT_DIR}/snap-save-prebuilt
    fi
fi

export SNAPCRAFT_BUILD_INFO=1

# snapcraft only support to be called from project root
cd ${ROOT_DIR}
# snapcraft need to have its yaml file in the project root directory
if [ ! -e .snapcraft.yaml ]; then
    ln -s -f extras/package/snap/snapcraft.yaml .snapcraft.yaml
fi

snapcraft pack

SCRIPTDIR="$(cd "$(dirname "$0")"; pwd)"

export VLC_SRC_DIR="$(cd "${SCRIPTDIR}/../../../"; pwd)"
export VLC_PLATFORM="tvOS"

PROJECTDIR="${VLC_SRC_DIR}/build-appletvos-arm64/build"
mkdir -p "${PROJECTDIR}"
cd "${PROJECTDIR}"

xcodegen generate --project . --spec ../../extras/package/apple/xcodegen.yml --project-root .

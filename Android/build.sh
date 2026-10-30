#!/bin/sh
# Build the APK: ./build.sh [debug|release]. Needs backend/<arch> from
# scripts/build-static.sh, the APK carries those binaries.
cd "$(dirname "$0")" || exit 1
type=${1:-debug}
case $type in
    debug) task=assembleDebug ;;
    release) task=assembleRelease ;;
    *) echo "usage: $0 [debug|release]" >&2; exit 1 ;;
esac
./gradlew "$task" && ls -l "app/build/outputs/apk/$type/app-$type.apk"

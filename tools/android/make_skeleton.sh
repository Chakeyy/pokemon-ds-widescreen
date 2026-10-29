#!/bin/bash
# Makes tools/android/skeleton.apk: the manifest, the icon and SDL's Java code, compiled.
# tools/android/apk.py adds the game to it. Only needed when one of those changes; the
# result is kept in the repository, so players need no Java or Android SDK.
#
#   make_skeleton.sh <Android SDK> <SDL2 source>
set -euo pipefail
SDK="$1"
SDL="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ -d "$SDK/build-tools" ]; then
    BT=$(ls -d "$SDK/build-tools/"* | sort -V | tail -n 1)
else
    BT="$SDK/build-tools/34.0.0"
fi

if [ -d "$SDK/platforms" ]; then
    JAR=$(ls -d "$SDK/platforms/android-"* | sort -V | tail -n 1)/android.jar
else
    JAR="$SDK/platforms/android-34/android.jar"
fi

echo "Using build-tools: $BT"
echo "Using android jar: $JAR"

W="$(mktemp -d)"

# The icon, at the launcher's sizes.
for d in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
    mkdir -p "$W/res/mipmap-${d%%:*}"
    python3 - "$HERE/../appimage/ssrecomp-setup.png" "$W/res/mipmap-${d%%:*}/ic_launcher.png" "${d##*:}" <<'PY'
import sys
from PIL import Image
Image.open(sys.argv[1]).convert('RGBA').resize((int(sys.argv[3]),) * 2, Image.LANCZOS).save(sys.argv[2], optimize=True)
PY
done
"$BT/aapt2" compile --dir "$W/res" -o "$W/res.zip"
"$BT/aapt2" link -o "$W/base.apk" -I "$JAR" --manifest "$HERE/AndroidManifest.xml" \
    --min-sdk-version 24 --target-sdk-version 34 "$W/res.zip"

# SDL's Java code + custom MainActivity.
mkdir -p "$W/classes" "$W/dex"
JAVA_FILES=$(find "$SDL/android-project/app/src/main/java" -name '*.java')
if [ -d "$HERE/java" ]; then
    JAVA_FILES="$JAVA_FILES $(find "$HERE/java" -name '*.java')"
fi

javac -nowarn --release 11 -cp "$JAR" -d "$W/classes" $JAVA_FILES 2>&1 | grep -v '^Note:' || true
"$BT/d8" --release --min-api 24 --lib "$JAR" --output "$W/dex" $(find "$W/classes" -name '*.class')
(cd "$W/dex" && zip -q -X "$W/base.apk" classes.dex)
cp "$W/base.apk" "$HERE/skeleton.apk"
echo "-> $HERE/skeleton.apk ($(stat -c %s "$HERE/skeleton.apk") bytes)"

#!/usr/bin/env bash
# Сборка RPM rdkafka-1c и rdkafka-1c-kerberos (RHEL/OL 9, x86_64) из готовых артефактов.
# Запускать из корня репозитория после ./build.sh и обновления package/RdKafka1C.zip:
#   [RELEASE=N] packaging/rpm/build-rpm.sh [путь к libRdKafka1C.so] [путь к RdKafka1C.zip]
# RELEASE (по умолчанию 1) повышать при пересборке той же версии, иначе dnf не увидит обновления.
# Результат: package/rpm/rdkafka-1c-*.x86_64.rpm и rdkafka-1c-kerberos-*.noarch.rpm
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SO="${1:-$ROOT/build/libRdKafka1C.so}"
ZIP="${2:-$ROOT/package/RdKafka1C.zip}"
SPEC="$ROOT/packaging/rpm/rdkafka-1c.spec"
OUT="$ROOT/package/rpm"
RELEASE="${RELEASE:-1}"
# Лицензии вшитых библиотек берутся из дерева сборки vcpkg
VCPKG_SHARE="$(dirname "$SO")/vcpkg_installed/x64-linux/share"

for t in rpmbuild unzip sha256sum tar; do
    command -v "$t" >/dev/null || { echo "нет $t (dnf install rpm-build unzip coreutils tar)" >&2; exit 1; }
done
[[ "$RELEASE" =~ ^[0-9]+$ ]] || { echo "RELEASE должен быть числом: $RELEASE" >&2; exit 1; }
[ -f "$SO" ]  || { echo "нет $SO — сначала ./build.sh" >&2; exit 1; }
[ -f "$ZIP" ] || { echo "нет $ZIP" >&2; exit 1; }

# Версия компоненты: из кода и из INFO.XML внутри zip, они должны совпадать
VERSION=$(sed -n 's/.*COMPONENT_VERSION = L"\([0-9.]*\)".*/\1/p' "$ROOT/src/AddInNative.h")
ZIP_VERSION=$(unzip -p "$ZIP" INFO.XML | tr -d '\r' | sed -n 's/.*version="\([0-9.]*\)".*/\1/p' | tail -1)
[ -n "$VERSION" ] || { echo "не нашёл COMPONENT_VERSION в src/AddInNative.h" >&2; exit 1; }
[ "$VERSION" = "$ZIP_VERSION" ] || { echo "версия в коде $VERSION, в INFO.XML zip $ZIP_VERSION — не совпадают" >&2; exit 1; }

# В zip должна лежать та же библиотека, что и в пакете
SO_SHA=$(sha256sum "$SO" | cut -d' ' -f1)
ZIP_SO_SHA=$(unzip -p "$ZIP" libRdKafka1C.so | sha256sum | cut -d' ' -f1)
[ "$SO_SHA" = "$ZIP_SO_SHA" ] || { echo "libRdKafka1C.so ($SO_SHA) отличается от библиотеки в $ZIP ($ZIP_SO_SHA)" >&2; exit 1; }

TOP=$(mktemp -d)
trap 'rm -rf "$TOP"' EXIT
mkdir -p "$TOP"/{SOURCES,SPECS,BUILD,RPMS,SRPMS}
cp "$SO" "$TOP/SOURCES/libRdKafka1C.so"
cp "$ZIP" "$TOP/SOURCES/RdKafka1C.zip"
cp "$ROOT/LICENSE" "$ROOT/packaging/rpm/README.rpm.md" "$ROOT/packaging/rpm/README.kerberos.md" "$TOP/SOURCES/"
cp "$SPEC" "$TOP/SPECS/"

# Лицензии вшитых библиотек: OpenSSL, librdkafka, lz4, zstd, zlib, Boost (у всех портов boost-* одна лицензия)
mkdir -p "$TOP/licenses"
for port in openssl librdkafka lz4 zstd zlib boost-json; do
    f="$VCPKG_SHARE/$port/copyright"
    [ -f "$f" ] || { echo "нет лицензии $f — нужен build/ после ./build.sh" >&2; exit 1; }
    cp "$f" "$TOP/licenses/${port%-json}.txt"
done
tar -czf "$TOP/SOURCES/third-party-licenses.tar.gz" -C "$TOP/licenses" openssl.txt librdkafka.txt lz4.txt zstd.txt zlib.txt boost.txt

rpmbuild -bb \
    --define "_topdir $TOP" \
    --define "rdk_version $VERSION" \
    --define "rdk_release $RELEASE" \
    "$TOP/SPECS/rdkafka-1c.spec"

mkdir -p "$OUT"
rm -f "$OUT"/rdkafka-1c-*.rpm
find "$TOP/RPMS" -name 'rdkafka-1c-*.rpm' -exec cp {} "$OUT/" \;
ls -l "$OUT"

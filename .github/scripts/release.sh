#!/bin/bash
# Usage: release.sh <build-dir> <out-dir>
# Packs an installation of <build-dir> as palan-<version>-<target>.tar.gz in <out-dir>, then
# unpacks it elsewhere and checks that it compiles and runs a program using std.
set -euo pipefail

build=$(realpath "$1")
mkdir -p "$2"
out=$(realpath "$2")
src=$(cd "$(dirname "$0")/../.." && pwd)
define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$src/src/common/PlnDefs.h"; }
version=$(define PALAN_VERSION)
target=$(define PALAN_TARGET)
archive="$out/palan-$version-$target.tar.gz"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

cmake --install "$build" --prefix "$tmp/stage" --strip
# The top level is the layout itself, so the archive unpacks into a prefix. Its directories are
# named, not '.', which would give the prefix itself the stage's permissions.
tar -czf "$archive" --owner=0 --group=0 --numeric-owner -C "$tmp/stage" bin lib share

mkdir "$tmp/prefix" "$tmp/home"
tar -xzf "$archive" -C "$tmp/prefix"
cat > "$tmp/uses_std.pa" <<'EOF'
cinclude <stdio.h>;
import <std/utf8>;
utf8 s = utf8("h\xC3\xA9\xFF", utf8mode.KEEP_INVALID);
printf("%ld %ld %d\n", s -> byteLen(), s -> charCount(), s.hadInvalid);
EOF
expected="4 3 1"
export PALAN_WORK="$tmp/work"
palan="$tmp/prefix/bin/palan"
check() {
	if [ "$2" != "$expected" ]; then
		echo "release check failed ($1): got '$2', expected '$expected'" >&2
		exit 1
	fi
}
check run "$("$palan" "$tmp/uses_std.pa")"
"$palan" -o "$tmp/uses_std" "$tmp/uses_std.pa"
check -o "$("$tmp/uses_std")"

needs() {
	objdump -T "$tmp/prefix/lib/palan/$version/tools/"* | grep -o "$1[0-9.]*" | sort -uV | tail -1
}
echo "requires $(needs GLIBC_) and $(needs GLIBCXX_)"
echo "$archive"

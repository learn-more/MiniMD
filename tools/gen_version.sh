#!/bin/sh
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/../src/Version.h"
TMP="$OUT.tmp"
GITVER="$(git describe --tags --always --dirty)"

# --long always yields TAG-N-gHASH (dot/hyphen-delimited), so the numeric FILEVERSION/
# PRODUCTVERSION parts for the .rc resource can be pulled out positionally.
GITLONG="$(git describe --tags --always --long)"
set -- $(echo "$GITLONG" | tr '.-' '  ')
VMAJOR="$1"
VMINOR="$2"
VPATCH="$3"
VBUILD="$4"
case "$VMAJOR" in ''|*[!0-9]*) VMAJOR=0 ;; esac
case "$VMINOR" in ''|*[!0-9]*) VMINOR=0 ;; esac
case "$VPATCH" in ''|*[!0-9]*) VPATCH=0 ;; esac
case "$VBUILD" in ''|*[!0-9]*) VBUILD=0 ;; esac

cat > "$TMP" <<EOF
#pragma once

#define MINIMD_VERSION "$GITVER"
#define MINIMD_VERSION_MAJOR $VMAJOR
#define MINIMD_VERSION_MINOR $VMINOR
#define MINIMD_VERSION_PATCH $VPATCH
#define MINIMD_VERSION_BUILD $VBUILD
EOF

if ! cmp -s "$TMP" "$OUT" 2>/dev/null; then
    mv "$TMP" "$OUT"
else
    rm "$TMP"
fi

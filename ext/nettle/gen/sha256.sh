#!/bin/sh
# Regenerate the SHA-256 assembly from the pinned Nettle source directory.
set -eu
out=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$1"

render() {
  arch=$1 os=$2
  elf=no coff=no prefix= log=no endbr=
  case $os in
    elf) elf=yes ;;
    macho) prefix=_; log=yes ;;
    coff) coff=yes ;;
  esac
  case $arch in
    x86_64) variant=sha_ni; endbr=endbr64 ;;
    arm64) variant=crypto; log=yes ;;
  esac
  src=$arch/$variant/sha256-compress-n.asm
  {
    printf '/* Generated from GNU Nettle %s by sha256.sh.\n' "$src"
    awk '/^   Copyright/ {notice=1} notice && /^.\)/ {exit} notice' "$src"
    printf '*/\n'
    m4 -Dsrcdir=. -DSYMBOL_PREFIX="$prefix" -DELF_STYLE="$elf" \
       -DCOFF_STYLE="$coff" -DW64_ABI="$coff" -DALIGN_LOG="$log" \
       -DWORDS_BIGENDIAN=no -DRODATA=.text -DTYPE_FUNCTION=%function \
       -DASM_X86_ENDBR="$endbr" m4-utils.m4 asm.m4 "$arch/machine.m4" \
       "$arch/fat/sha256-compress-n-2.asm" > "$tmp/body"
    sed -e 's/[[:space:]]*$//' -e '/^$/d' "$tmp/body"
    if [ "$elf" = yes ]; then echo '.section .note.GNU-stack,"",%progbits'; fi
  } > "$tmp/$os"
}

# Keep the common instructions once, with ABI-specific entry and exit code.
merge() { diff -D "$1" "$2" "$3" || test "$?" = 1; }
render x86_64 elf
render x86_64 coff
render x86_64 macho
merge _WIN32 "$tmp/elf" "$tmp/coff" > "$tmp/pc"
merge __APPLE__ "$tmp/pc" "$tmp/macho" > "$out/sha256-x86_64.S"
render arm64 elf
render arm64 macho
merge __APPLE__ "$tmp/elf" "$tmp/macho" > "$out/sha256-aarch64.S"

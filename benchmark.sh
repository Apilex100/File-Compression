#!/usr/bin/env bash
#
# benchmark.sh — reproducible compression + losslessness verification
# ===================================================================
# Anyone can run this to independently verify the compressor's metrics.
#
#   ./benchmark.sh                 # run on built-in, deterministic test inputs
#   ./benchmark.sh file1 file2 ... # ALSO test your own files
#
# For each input it: compiles encode/decode from ./source, compresses the file,
# decompresses the result, verifies the round-trip is BYTE-EXACT via SHA-256,
# and reports original size, compressed size, reduction %, and compression ratio.
# No network access or external data is required — the default inputs are
# generated deterministically, so the numbers reproduce on any machine.
#
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/source"

# --- choose a C++ compiler -------------------------------------------------
CXX="${CXX:-g++}"
command -v "$CXX" >/dev/null 2>&1 || CXX=c++
command -v "$CXX" >/dev/null 2>&1 || { echo "ERROR: no C++ compiler (g++/c++) found on PATH."; exit 1; }

# --- portable SHA-256 (Linux: sha256sum, macOS: shasum) --------------------
sha256() {
  if   command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
  elif command -v shasum    >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}'
  else echo "NO_SHA_TOOL"; fi
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
DATA="$WORK/data"; mkdir -p "$DATA"

echo "Compiler : $CXX"
echo "Compiling encode/decode from $SRC ..."
"$CXX" -std=c++17 -O2 -w -o "$WORK/encode" "$SRC/Encoding.cpp" || { echo "ERROR: encode build failed"; exit 1; }
"$CXX" -std=c++17 -O2 -w -o "$WORK/decode" "$SRC/Decoding.cpp" || { echo "ERROR: decode build failed"; exit 1; }
echo "Build OK."
echo

# --- build the list of inputs (copied into a scratch dir so nothing outside
#     the temp directory is ever written) -----------------------------------
NAMES=()
add_input() { cp "$2" "$DATA/$1" 2>/dev/null && NAMES+=("$1"); }

# 1) Highly repetitive / skewed alphabet — best case for Huffman.
yes "aaaaaaaaaabbbbbcccd " | head -c 500000 > "$DATA/gen_skewed.txt"; NAMES+=("gen_skewed.txt")

# 2) Natural-language English prose — a fixed pangram paragraph (contains every
#    letter incl. rare q/x/z/j + punctuation) repeated to ~900 KB. Realistic,
#    skewed letter frequencies that used to break the old 16-bit decoder.
PARA="The quick brown fox jumps over the lazy dog. Amazingly few discotheques provide jukeboxes; wizards make toxic brew for the jovial queen. Pack my box with five dozen liquor jugs. "
: > "$DATA/gen_english.txt"
i=0; while [ "$i" -lt 4000 ]; do printf '%s' "$PARA" >> "$DATA/gen_english.txt"; i=$((i+1)); done
NAMES+=("gen_english.txt")

# 3) The compressor's own source code (real mixed text), grown so the header
#    overhead is negligible.
cat "$SRC/Encoding.cpp" "$SRC/Decoding.cpp" "$SRC/compression.h" > "$DATA/unit.txt"
: > "$DATA/gen_sourcecode.txt"
i=0; while [ "$i" -lt 20 ]; do cat "$DATA/unit.txt" >> "$DATA/gen_sourcecode.txt"; i=$((i+1)); done
rm -f "$DATA/unit.txt"; NAMES+=("gen_sourcecode.txt")

# 4) Any files the user passed on the command line.
u=0
for uf in "$@"; do
  if [ -f "$uf" ]; then u=$((u+1)); add_input "user${u}_$(basename "$uf")" "$uf"
  else echo "skip (not a regular file): $uf"; fi
done
[ "$u" -gt 0 ] && echo

# --- compress -> decompress -> verify --------------------------------------
printf "%-26s %13s %15s %11s %9s  %s\n" "INPUT" "ORIGINAL(B)" "COMPRESSED(B)" "REDUCTION" "RATIO" "LOSSLESS?"
printf '%s\n' "-------------------------------------------------------------------------------------------------"
allpass=1
for name in "${NAMES[@]}"; do
  in="$DATA/$name"
  "$WORK/encode" "$in"     >/dev/null 2>&1   # produces  $in.spd
  "$WORK/decode" "$in.spd" >/dev/null 2>&1   # produces  $in.spd.txt
  if [ ! -f "$in.spd" ] || [ ! -f "$in.spd.txt" ]; then
    printf "%-26s %13s\n" "$name" "encode/decode failed"; allpass=0; continue
  fi
  o=$(wc -c < "$in"); c=$(wc -c < "$in.spd")
  red=$(awk -v o="$o" -v c="$c" 'BEGIN{printf "%.1f%%",(1-c/o)*100}')
  ratio=$(awk -v o="$o" -v c="$c" 'BEGIN{printf "%.2f:1",o/c}')
  h1=$(sha256 "$in"); h2=$(sha256 "$in.spd.txt")
  if [ "$h1" = "$h2" ]; then ll="YES (SHA-256 match)"; else ll="NO  <-- MISMATCH"; allpass=0; fi
  printf "%-26s %13d %15d %11s %9s  %s\n" "$name" "$o" "$c" "$red" "$ratio" "$ll"
done
echo
echo "REDUCTION = 1 - compressed/original   RATIO = original/compressed   (higher is better)"
echo "Small files can show a negative reduction: each distinct symbol costs a fixed"
echo "header record, so the self-describing header dominates tiny inputs."
echo
if [ "$allpass" -eq 1 ]; then
  echo "RESULT: PASS — every round-trip is byte-exact lossless (SHA-256 verified)."
  exit 0
else
  echo "RESULT: FAIL — at least one round-trip was not lossless."
  exit 2
fi

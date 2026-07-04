# File Compression using Huffman Coding

> A lossless, bit-level file compressor and decompressor written in C++, implementing classic Huffman coding from scratch.

## Overview

This project is a from-scratch implementation of **Huffman coding**, one of the foundational algorithms in lossless data compression. It replaces frequently occurring characters in a file with short binary codewords and rarely occurring characters with longer ones, producing a compressed file that is smaller than the original while being fully reversible — the original file is reconstructed **bit-for-bit** without any loss.

The project ships as two independent command-line programs:

- **`encode`** — compresses an input file into a `.spd` file.
- **`decode`** — reconstructs the original file from a `.spd` file.

What makes it interesting is that it does not rely on any compression library. The Huffman tree, the frequency-sorted data structure, the codeword generation, the serialized on-disk format, and the manual bit-packing are all implemented by hand, giving a clear, readable window into how real-world entropy coders work under the hood.

## Features

- **Lossless, reversible compression** — the round-trip `encode` then `decode` reproduces the original byte stream exactly.
- **Hand-rolled Huffman tree construction** — builds the optimal prefix-code tree from character frequencies without any external library.
- **Frequency-sorted linked list** — characters are inserted into and kept sorted by frequency in a custom singly-linked list, so the two least-frequent nodes are always at the head.
- **Manual bit-level I/O** — codewords are packed one bit at a time into bytes using a static byte buffer and bit-shifting (`writeBit`), with symmetric bit-unpacking on decode.
- **Self-describing file format** — each compressed `.spd` file embeds its own character-to-codeword mapping table plus a padding count, so `decode` needs no side channel to reconstruct the file.
- **Automatic output naming** — if no output name is given, `encode` appends `.spd` and `decode` appends `.txt` automatically.
- **Padding handling** — because compressed data rarely ends on a byte boundary, the encoder records how many padding bits were added so the decoder can discard them.
- **Basic error handling** — guards against missing input files, unopenable output files, and truncated/invalid (non-Huffman) input during decoding.
- **Standalone reference implementation** — a separate STL-based demo (`huffman_test.cpp`) illustrates the same algorithm using a `priority_queue` min-heap for comparison.

## Tech Stack

- **Language:** C++ (compiled with `g++`)
- **Standard library:** C++ standard library plus C standard I/O (`<cstdio>`/`FILE*`, `<string.h>`, `<malloc.h>`), `<iostream>`, `<fstream>`
- **Build tool:** `g++` invoked directly (no Makefile/CMake in the repository)
- **Data structures:** custom singly-linked list, binary Huffman tree, and (in the reference demo) an STL `priority_queue` min-heap

## How It Works

### The Huffman algorithm (encoder — `Encoding.cpp`)

Compression happens in two passes over the input file:

**Pass 1 — build frequencies and the tree**

1. **Count character frequencies.** `main` reads the input one byte at a time and calls `addSymbol(ch)` for each. `addSymbol` maintains a singly-linked list of `node` structs (defined in `Encoding.cpp`) sorted in **ascending** order of frequency. New characters are inserted at the head (frequency 1, the minimum); repeat characters have their frequency incremented and are re-positioned via `insert` so the list stays sorted.
2. **Construct the Huffman tree.** `makeTree()` repeatedly takes the two lowest-frequency nodes from the front of the sorted list, joins them under a new internal node (`type = INTERNAL`, symbol `'@'`) whose frequency is their sum, and re-inserts that combined node into the sorted list with `insert`. This continues until a single node remains — the `ROOT` of the Huffman tree.
3. **Assign codewords.** `genCode(ROOT, "")` walks the tree in a recursive DFS, appending `0` when descending left and `1` when descending right. Each leaf receives the accumulated bit string as its prefix-free codeword. As a side effect, `genCode` rebuilds the linked list of leaf nodes so codewords can later be looked up by character.

**Pass 2 — emit the compressed file**

4. `writeHeader(fp1)` serializes the mapping table (see the file format below).
5. The input file is read a second time; for each character, `writeCode` looks up its codeword with `getCode` and streams the bits into the output via `writeBit`. `writeBit` accumulates bits into a `static char byte` buffer using left shifts, and flushes a full byte to disk with `fwrite` once 8 bits have been collected.

### Decoding (`Decoding.cpp`)

1. Read the 1-byte `N` (unique-character count) from the header. `N == 0` is treated as `256`.
2. Read `N` `codeTable` records into a dynamically allocated `codelist` array, then read the 1-byte `padding` value.
3. Read the data one byte at a time and feed each byte to `decodeBuffer`. This function keeps a rolling 16-bit `int buffer` (large enough to hold two bytes so a codeword can straddle a byte boundary), left-shifting new bytes into it and tracking the number of valid bits with `k`. On the first call it consumes the leading padding bits.
4. `int2string` converts the buffer into a 16-character bit string, and `match` greedily compares each candidate codeword prefix against the buffer. When a codeword matches, its character is emitted, the matched bits are shifted out of the buffer, and matching restarts from the first table entry. Decoded characters are written to the output file with `fwrite`.

### On-disk file format (`.spd`)

Defined by `compression.h` and produced by `writeHeader`:

```
+--------------------------------------------------+
| N               : 1 byte  (unique char count,    |
|                            0 aliases 256)         |
+--------------------------------------------------+
| codeTable[0]    : { char x; char code[16]; }      |
| codeTable[1]    : { char x; char code[16]; }      |
| ...             : N records total                 |
+--------------------------------------------------+
| padding         : 1 byte  (# of padding bits)     |
+--------------------------------------------------+
| padding bits    : that many leading 0 bits        |
+--------------------------------------------------+
| DATA            : bit-packed Huffman codewords     |
+--------------------------------------------------+
```

Each `codeTable` record (from `compression.h`) stores a character `x` and its codeword `code` as a null-terminated ASCII bit string of up to `MAX` (16) bytes. The `padding` byte records how many zero bits were prepended so that the total bit stream aligns to whole bytes — for example, a payload of 4 bytes plus 3 bits is padded with 5 bits to reach 5 whole bytes.

### Worked example (from the source comments)

Input text `aabcbaab` yields codewords `a = "1"`, `b = "01"`, `c = "00"` and encodes as the bit stream `1 1 01 00 01 1 1 01`, preceded by the mapping table and padding count.

## Getting Started

### Prerequisites

- A C++ compiler — **`g++`** (GCC).
- A POSIX-like environment. The included `encode`/`decode` binaries are Linux x86-64 ELF executables; the source uses `<malloc.h>` and `<bits/stdc++.h>`, which are GCC/Linux conventions.

### Build

There is no Makefile in the repository, so compile the two programs directly with `g++`:

```bash
cd source

# Build the compressor
g++ Encoding.cpp -o encode

# Build the decompressor
g++ Decoding.cpp -o decode
```

(The repository already includes prebuilt Linux `encode` and `decode` binaries in `source/`.)

The reference demo can be built separately:

```bash
g++ huffman_test.cpp -o huffman_test
```

### Run

```bash
# Compress: produces <file>.spd
./encode <file-to-compress>

# Decompress: produces <file>.txt
./decode <file-to-decompress>
```

## Usage / Examples

Compress a text file:

```bash
./encode notes.txt
# -> writes notes.txt.spd
```

Decompress it again:

```bash
./decode notes.txt.spd
# -> writes notes.txt.spd.txt (the reconstructed original content)
```

Run the standalone Huffman-code demo (prints the codeword for each of `a b c d e f`):

```bash
./huffman_test
```

If the input file is missing, `encode` reports `Error, Input file does not exists`. If `decode` is given a file that was not produced by this tool, it reports that the file is not a valid Huffman-compressed file.

## Project Structure

```
File-Compression-master/
├── README.md                 # This file
├── .gitignore
└── source/
    ├── compression.h         # Shared constants, the codeTable struct, and file extensions (.spd/.txt)
    ├── Encoding.cpp          # Compressor: frequency list, Huffman tree, codeword gen, bit-packing
    ├── Decoding.cpp          # Decompressor: header parsing + rolling-buffer codeword matching
    ├── huffman_test.cpp      # Standalone STL priority_queue reference implementation
    ├── encode                # Prebuilt Linux x86-64 binary of Encoding.cpp
    └── decode                # Prebuilt Linux x86-64 binary of Decoding.cpp
```

## Possible Improvements

Drawn from the source `to-do` notes and the current design:

- Robust support for arbitrary **binary files** (e.g. JPEG, MP3), not just text.
- Grouping repeating **bit patterns** rather than single characters for better ratios.
- **Unicode** support beyond single-byte characters.
- Replacing the linear codeword lookup during decode with a tree walk for faster decoding.
- Adding a build system (Makefile/CMake) and a round-trip test harness.

## Author

**Aviral Kumar Singh** — [github.com/Apilex100](https://github.com/Apilex100)

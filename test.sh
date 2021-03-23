#!/bin/bash

OUTDIR="/tmp/mp4_test"

make -j
./gen_input.sh "$OUTDIR"
./extract_meta "$OUTDIR/input.mp4" "$OUTDIR/meta.bin"
./fragments2mp4 "$OUTDIR"/{repack.mp4,meta.bin,video.fmp4,audio1.fmp4,audio2.fmp4}

#!/bin/bash

OUTDIR="$1"
MP4_INPUT="$OUTDIR/input.mp4"

mkdir -p "$OUTDIR"

ffmpeg \
    -f lavfi -i testsrc=s=1280x720 \
    -f lavfi -i sine=f=440:b=500 \
    -f lavfi -i sine=f=700:b=760 \
    -map 0:0 -c:0 libx264 \
        -x264-params keyint=25:min-keyint=25:open-gop=0 -preset ultrafast \
    -map 1:0 -c:1 libmp3lame -b:1 128k \
    -map 2:0 -c:2 libmp3lame -b:2 320k \
    -t 60 "$MP4_INPUT" -y

function extract()
{
    STREAM_NO="$1"
    OUTPUT="$2"
    ffmpeg -i "$MP4_INPUT" -map 0:"$STREAM_NO" -c copy -f mp4 \
        -movflags frag_keyframe+empty_moov \
        -frag_duration 1 "$OUTPUT" -y
}

extract 0 "$OUTDIR"/video.fmp4
extract 1 "$OUTDIR"/audio1.fmp4
extract 2 "$OUTDIR"/audio2.fmp4

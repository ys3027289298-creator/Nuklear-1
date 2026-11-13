#!/bin/sh
# Assembles the segmented color-picker recording from the PPM frame
# sequence and the four title cards. Each segment gets a title card,
# its demo frames, and a held final frame (the inter-segment pause).
# No keyboard interaction is required; everything is pre-rendered.
set -e

cd "$(dirname "$0")"
FR=12
OUT=color_picker_demo.mp4
WORK=video_work

mkdir -p "$WORK"

# frame ranges produced by color_picker_offscreen.c
# seg1 idle:   0..29, seg2 matrix: 30..74,
# seg3 hue:    75..119, seg4 hex: 120..179
make_segment() {
    title=$1; start=$2; end=$3; out=$4
    frames=$((end - start + 1))
    title_dur=$FR            # ~1s title card
    hold_dur=$((FR * 2))     # 2s pause on the last frame
    last="frames/frame_$(printf '%04d' "$end").ppm"

    ffmpeg -hide_banner -loglevel error -y -loop 1 -t 1 -r "$FR" \
        -i "$title" -f image2 -start_number "$start" -r "$FR" \
        -i "frames/frame_%04d.ppm" -loop 1 -t 2 -r "$FR" -i "$last" \
        -filter_complex "[0:v]format=pix_fmts=rgb24[t];
                         [1:v]trim=start_frame=0:end_frame=$frames,setpts=PTS-STARTPTS[m];
                         [2:v]format=pix_fmts=rgb24[h];
                         [t][m][h]concat=n=3:v=1:a=0,format=yuv420p[v]" \
        -map "[v]" -r "$FR" -c:v libx264 -preset veryfast -crf 30 "$out"
}

make_segment frames/title_1.ppm   0  29 "$WORK/seg1.mp4"
make_segment frames/title_2.ppm  30  74 "$WORK/seg2.mp4"
make_segment frames/title_3.ppm  75 119 "$WORK/seg3.mp4"
make_segment frames/title_4.ppm 120 179 "$WORK/seg4.mp4"

: > "$WORK/list.txt"
for s in seg1 seg2 seg3 seg4; do
    echo "file '$s.mp4'" >> "$WORK/list.txt"
done

ffmpeg -hide_banner -loglevel error -y -f concat -safe 0 \
    -i "$WORK/list.txt" -c copy "$OUT"

echo "wrote $OUT"

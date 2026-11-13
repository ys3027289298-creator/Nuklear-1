# Color Picker Work — Outputs

Hex-enabled HSV color picker for Nuklear, plus its headless acceptance
tests and the segmented screen recording.

## Library change

- New public API `nk_color_pick_hex()` (declared in `nuklear.h`):
  the existing HSV matrix + hue/alpha bars plus an embedded hexadecimal
  input field drawn inside the same widget bounds.
- All edit state is caller-owned (`hex_buffer`, `hex_len`, `hex_active`,
  `nk_text_edit`). The hex string lives in a fixed buffer, so dragging and
  typing perform no heap allocation.
- The legacy `nk_color_pick()` / `nk_color_picker()` path is unchanged and
  still uses the exact same widget rectangle, so neighboring controls do
  not move.

## Files

- `color_picker_test.c` — headless acceptance harness with a counting
  allocator and simulated input. Covers:
  1. layout compatibility (identical widget/neighbor bounds)
  2. HSV matrix drag correctness and zero heap allocation
  3. hue-bar drag correctness and zero heap allocation
  4. hex typing round-trip to the exact color, zero allocation
  5. fixed-buffer overflow rejection without allocation
  6. optional `#` hex-prefix handling
- `color_picker_offscreen.c` — software-rendered (rawfb) demo that drives
  the real widget offscreen and writes one PPM frame per step.
- `make_titles.c` — renders the four segment title cards with the same
  software backend (no external artwork).
- `build_video.sh` — regenerates frames/title cards and assembles
  `color_picker_demo.mp4` (title card → segment → 2 s pause, four times).
- `color_picker_demo.mp4` — the segmented recording (480x360, ~47 KB).

## Reproduce (from the repository root)

```sh
# acceptance tests (no window system needed)
gcc -std=c89 -Wall -Wextra -pedantic -Wno-long-long -I. \
    output/color_picker_test.c -lm -o output/color_picker_test
./output/color_picker_test

# rebuild the recording end to end
cc -std=c89 -Wall -Wextra -pedantic -Wno-unused-function -I. \
    output/color_picker_offscreen.c -lm -o output/color_picker_offscreen
cc -std=c89 -Wall -Wextra -pedantic -Wno-unused-function -I. \
    output/make_titles.c -lm -o output/make_titles
mkdir -p output/frames
./output/color_picker_offscreen   # writes output/frames/frame_*.ppm
./output/make_titles              # writes output/frames/title_*.ppm
./output/build_video.sh
```

The repository demo `demo/glfw_opengl2/main.c` also exercises the new
widget inside its background color combo box.

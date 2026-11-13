# Three-zone docking layout — deliverables

This directory holds the recorded verification artifacts for the nuklear
left/center/right docking feature. Everything can be regenerated from the
repository's own build entry points; no keyboard interaction is required.

## What was implemented

- `demo/common/nk_dock.h`, `demo/common/nk_dock.c`: a self-contained docking
  module built only on the public nuklear API.
  - Left / Center / Right zones, each stacking its panes vertically.
  - Panes drag out of a zone from their header into a floating window and
    re-dock when dropped on a left/right edge strip (center drop stays
    floating). Drop-zone highlights are drawn while dragging.
  - Two draggable splitters control the left and right zone widths with
    minimum-width clamping; the hovered/dragged splitter gets a resize cursor.
  - `nk_dock_query()` reports the layout fractions and every pane's zone,
    open state and rectangles into a caller-provided fixed buffer.
  - All state is a single fixed-size struct (`struct nk_dock`). The module
    contains no heap allocation, so layout does zero allocation per frame.
- `demo/glfw_opengl3/nuklear_glfw_gl3.h`: adds `nk_glfw3_init_fixed()` so a
  context can run on a caller-owned fixed memory block. The original
  `nk_glfw3_init()` is kept and now delegates to it, so existing callers and
  floating-window behaviour are unchanged. `nuklear.h` itself is untouched.
- `demo/glfw_opengl3/dock_demo.c`: visual demo (interactive, plus unattended
  `--script` and offscreen `--record <dir>` modes). The small "assets" (an
  8x8 swatch texture and a color palette) are generated in code, so there are
  no binary assets.
- `demo/glfw_opengl3/dock_test.c`: headless verification driven entirely
  through synthetic `nk_input_*` events.
- `demo/glfw_opengl3/glew_shim/`: a tiny GLEW-compatible shim used only when
  the system lacks `libglew-dev` (this environment). With real GLEW present
  the Makefile links GLEW exactly as before.

## Build and verify (repository entry points)

```sh
cd demo/glfw_opengl3
make dock_test   # headless logic + zero-allocation test
./bin/dock_test  # prints "PASSED: <N> frame(s), 0 heap alloc(s)"
make dock_demo   # visual GLFW demo
make             # original demo, rebuilt to prove zero regression
```

The headless test runs six interaction sequences (initial layout, both
splitters, undock, re-dock, floating move) and asserts:

- correct geometry after every sequence;
- zero heap allocations overall and, per frame, an unchanged
  `ctx.memory.calls` / `ctx.memory.allocated` on the fixed block;
- floating windows keep their remembered size and follow header drags
  (legacy movable behaviour preserved through the new drag path).

## Recording

The WSLg/Wayland desktop here does not expose GL content to X11 screen
capture, so the demo renders into an offscreen framebuffer and writes PPM
frames via `glReadPixels` (`--record`), which are then encoded with ffmpeg:

```sh
./bin/dock_demo --script --record /tmp/frames
ffmpeg -framerate 60 -i /tmp/frames/%05d.ppm -c:v libx264 -pix_fmt yuv420p out.mp4
```

The script paces itself at 60 Hz, holds each segment title on screen and
pauses between segments before acting.

## Files

- `dock_demo.mp4`: full 1200x800 / 60 fps / 560-frame recording with all six
  titled segments in order.
- `segments/seg1_*.mp4` … `seg6_*.mp4`: one clip per segment, each starting
  on its title card and keeping the inter-segment pause.

# Mode9 — 3D OBJ Renderer for DIV Games Studio 2

**Mode9** is a DLL extension for [DIV Games Studio 2](https://en.wikipedia.org/wiki/DIV_Games_Studio) that adds a new rendering mode — *Mode 9* — enabling navigable 3D worlds built from standard Wavefront `.OBJ` meshes. It is the big brother of DIV2's built-in Mode 8 (sector maps): where Mode 8 uses flat `.WLD` sectors, Mode 9 uses any 3D mesh modelled in Blender or any similar tool — walls, floors, ceilings, and ramps at any angle.

> Author: **Sebastian J. Moncho Maquet**
> Date: June 2026
> Blog: [elgeneralfailure.com](https://elgeneralfailure.com)

---

## Overview

The camera is a normal DIV2 process that you place wherever you want and point in any direction. The most common setup is first-person (as in the included examples), but it works equally well as a follow camera behind a character, a top-down view, or a set of fixed cameras. On top of the 3D world, you can render characters and objects in two ways:

- **Billboard sprites** — flat 2D drawings that scale with distance and always face the camera, just like Mode 7. Good for torches, trees, items, simple enemies.
- **3D object models** — fully three-dimensional `.OBJ` meshes placed in the world (NPCs, props). You can have several models loaded at once, each drawn at multiple positions per frame. Animated models (morph-target animation from a numbered sequence of OBJ files) are supported with smooth interpolation between poses.

---

## Features

### World rendering
- Load a full 3D scenario from a Wavefront `.OBJ` file with a `.MAP` texture and a configurable scale (DIV2 units per OBJ unit).
- **Multi-material support** — up to 16 different `.MAP` textures per scene, assigned by `usemtl` group.
- **Hot world reload** — call `LOAD_WLD9()` again while the game is running to switch to a different map without stopping the renderer or losing the camera. Ideal for level transitions through doors.
- **Panoramic skybox** — a `.MAP` texture wrapped 360° horizontally, scrolling with yaw and shifting with pitch for a dome-like effect. Deactivated with `SKY9_OFF()` for interiors.
- **Ambient color and flat shading** — `SET_ENV_COLOR9()` controls the background and the shading ramp used for untextured faces.
- **Distance fog** — Doom-style colormap fog (`SET_FOG9()`), same color as the ambient light.
- **Far clipping** — `MODE9_FARCLIP()` to stop drawing beyond a set distance, combined with fog for natural fade-out.

### Camera and rendering
- Adjustable horizontal field of view (`MODE9_FOV()`; default ~71°, wide as 110°, narrow as 60°).
- Back-face culling modes: off, normal, or inverted (`MODE9_CULL()`).
- Perspective-correct texture mapping: affine-per-16px by default (fast), or exact per-pixel (`MODE9_PERSPECTIVE()`).
- Render downscaling (`MODE9_DOWNSCALE()`) for a pixelated retro look or extra performance on slow machines.
- **Split-screen** — up to 4 simultaneous cameras, each with its own viewport rect and its own `m9` struct (`MODE9_SPLIT()` / `MODE9_REGION()`).

### Billboards (Mode 7 style)
- **Automatic mode**: set `ctype = c_m9` on any process and give it world coordinates in `x, y, z` — Mode 9 draws it every frame without any extra call.
- **Manual mode**: call `MODE9_BILLBOARD()` to position a specific process, or `MODE9_PROJECT()` to project a world point to screen coordinates without touching any process.
- **Angular graphics** (`xgraph`): like Mode 7, the DLL selects the correct graphic from a table based on the angle from which the camera sees the object — perfect for enemies that face in different directions.
- **Z-buffer occlusion** — billboards hidden behind geometry are automatically culled (`MODE9_OCCLUDE()`).
- Up to 256 simultaneous billboard processes.

### 3D object models
- Load separate `.OBJ` models as props or NPCs with `LOAD_OBJ9()`. Up to 8 distinct models at once, drawn up to 32 times per frame across all models combined.
- **Background loading** — `LOAD_OBJ9()` returns a handle immediately and loads the file in the background (one file per frame), avoiding the DIV2 watchdog exception on slow media.
- **Animated models** — `LOAD_OBJ9_ANIM()` loads a numbered sequence of poses (e.g. `CAT0.OBJ` … `CAT9.OBJ`) and `DRAW_OBJ9_ANIM()` blends between them smoothly. 8–12 poses already produce a very smooth cycle.
- Each model can be drawn at any world position, height, and yaw angle on the same frame, so a single loaded model can populate an entire level.

### Collision and physics
- **Wall collision** — `advance()` and `xadvance()` slide along walls instead of going through them. Uses the camera process's `radius` field.
- **Step climbing** — `MODE9_STEP()` sets the maximum height the camera climbs automatically (stairs, ramps). Walls shorter than that height are stepped over instead of blocked.
- **NPC collision** — `MODE9_COLLIDE()` pushes any process out of walls with sliding, and rests it on the floor if `MODE9_STEP` is active. Good for enemies that share the same world.

---

## API Reference (grouped)

### Starting and stopping
| Function | Description |
|---|---|
| `START_MODE9(camera, &m9.z, region)` | Start the renderer (like `start_mode8`). `region=0` = full screen |
| `STOP_MODE9()` | Stop the renderer |
| `MODE9_SCRW()` / `MODE9_SCRH()` | Screen width / height in pixels (adapts to any resolution) |
| `MODE9_BIND(&m9.z)` | Bind the `m9` struct manually (optional; `START_MODE9` already does it) |

### Loading the 3D scene
| Function | Description |
|---|---|
| `LOAD_WLD9(obj, tex, scale)` | Load OBJ scenario + texture + scale. Hot-reloadable mid-game |
| `MODE9_SETTEX(map)` | Replace the scene's default texture at runtime |
| `MODE9_MATERIAL(n, map)` | Assign a `.MAP` texture to material slot `n` (0..15) |
| `MODE9_UNITSCALE(n)` | DIV2 units per OBJ unit (default 100) |

### Camera, FOV and clipping
| Function | Description |
|---|---|
| `MODE9_SET(height, pitch)` | Eye height and vertical angle (pitch) of the camera |
| `MODE9_FOV(degrees)` | Horizontal FOV (default ~71°; 60=zoom, 90=wide, 110=ultra-wide) |
| `MODE9_CULL(mode)` | Face culling: 0=off, 1=back faces (default), 2=inverted |
| `MODE9_FARCLIP(dist)` | Stop drawing beyond `dist` DIV2 units (0 = disabled) |

### Sky, environment and fog
| Function | Description |
|---|---|
| `LOAD_SKY9(map)` | Load a 360° panoramic sky texture |
| `SKY9_OFF()` | Disable the sky, return to flat background color |
| `SET_ENV_COLOR9(r, g, b)` | Background / ambient color (0..100 per channel) |
| `SET_FOG9(near, far)` | Distance fog, same color as ambient |
| `MODE9_FOG_OFF()` | Disable fog |

### Collisions and physics
| Function | Description |
|---|---|
| `MODE9_COLLISION(on)` | Wall collision for the camera (default on) |
| `MODE9_COLLIDE(pid)` | Push any process out of walls (sliding), returns 1 if moved |
| `MODE9_STEP(height)` | Max step height the camera auto-climbs (0 = disabled) |

### Billboards
| Function | Description |
|---|---|
| `MODE9_LOADFPG(fpg)` | Load an FPG into the DLL for multi-viewport billboard drawing |
| `MODE9_CTYPE(value)` | Change the `c_m9` constant value (default 200) |
| `MODE9_BILLBOARD(pid, wx, wy, wz, size)` | Manually position a process as a 3D billboard |
| `MODE9_PROJECT(wx, wy, wz, &sx, &sy, &sz)` | Project a world point to screen without touching a process |
| `MODE9_OCCLUDE(on)` | Z-buffer occlusion for billboards (default on) |

### 3D object models
| Function | Description |
|---|---|
| `LOAD_OBJ9(obj, tex, scale)` | Load a 3D prop/NPC model (background, non-blocking) |
| `DRAW_OBJ9(handle, x, y, z, angle)` | Draw a 3D model at a world position each frame |
| `LOAD_OBJ9_ANIM(base, tex, scale, nframes)` | Load a morph-target animation from numbered OBJ files |
| `DRAW_OBJ9_ANIM(handle, x, y, z, angle, fpos)` | Draw an animated model at a given pose (interpolated) |
| `MODE9_OBJ9_READY(handle)` | Returns 1 when background loading has finished |

### Split-screen and performance
| Function | Description |
|---|---|
| `MODE9_REGION(n, x, y, w, h)` | Define viewport region `n` (1..15); w/h≤0 clears it |
| `MODE9_SPLIT(slot, cam, m9, x, y, w, h)` | Add a split-screen viewport (up to 4) |
| `MODE9_DOWNSCALE(n)` | Render at 1/n resolution (1=native, 2/3/4=faster/pixelated) |
| `MODE9_PERSPECTIVE(on)` | 0=affine per 16px (default/fast), 1=exact per-pixel |

---

## Technical Limits

| Resource | Maximum |
|---|---|
| Scene vertices | 4 096 |
| Scene triangles | 8 192 |
| Scene materials (textures) | 16 |
| Texture size per material | 1 024 × 1 024 px |
| Simultaneous 3D models | 8 |
| 3D model draw calls per frame (all models) | 32 |
| Vertices per 3D model | 512 |
| Triangles per 3D model | 1 024 |
| Billboard processes (`c_m9`) | 256 |
| Split-screen viewports | 4 |
| Sky texture | 4 096 × 2 048 px |

> All limits are compile-time constants in `SRC/MODE9.CPP` (`MAX_TRIS`, `MAX_OBJ9_MODELS`, etc.) and can be raised by recompiling the DLL, at the cost of higher memory usage.

**Tips for working around the limits:**
- Use textures to provide detail instead of geometry (a brick wall is one quad with a good MAP, not thousands of modelled bricks).
- Split large levels into smaller maps and hot-reload them with `LOAD_WLD9()` as the player moves between areas.
- Pack multiple small textures into a single 1 024 × 1 024 atlas and use UVs to pick each face's region — one material slot does the work of many.
- Reserve 3D models for key objects; use billboards (up to 256) for everything else.
- Simplify meshes before export (Blender's *Decimate* modifier). The engine is low-resolution and a very dense model does not look better — it just runs slower.

---

## Repository Layout

```
Mode9/
├── SRC/                  # C++ source code
│   └── MODE9.CPP         # Full renderer (~1 200 lines, single file)
├── BIN/                  # Pre-compiled library
│   └── MODE9.DLL         # Ready-to-use DLL, compiled with Watcom C++ 10.6
├── EXAMPLES/             # Usage examples and assets
│   ├── PRG/              # DIV2 example programs (.PRG) — source in Spanish
│   │   ├── MODE9_01.PRG  # Stage 1: walk through a castle (keyboard + mouse)
│   │   ├── MODE9_02.PRG  # Stage 2: billboard sprites (torches, props)
│   │   ├── MODE9_03.PRG  # Stage 3: 3D object models (cat prop)
│   │   ├── MODE9_04.PRG  # Stage 4: animated 3D model
│   │   ├── MODE9_05.PRG  # Stage 5: level transition (door to interior)
│   │   └── MODE9_MU.PRG  # Bonus: split-screen multiplayer demo
│   ├── OBJ/              # 3D mesh assets (.OBJ + .mtl)
│   │   ├── CASTLE.OBJ    # Exterior castle scenario (3 materials)
│   │   ├── CASTIN.OBJ    # Interior castle scenario
│   │   ├── CAT.OBJ       # Static cat prop
│   │   ├── CAT0..9.obj   # Animated cat (10-pose morph sequence)
│   │   └── CUBE.OBJ      # Minimal test cube
│   ├── MAP/              # DIV2 texture files (.MAP)
│   │   ├── CASTLE.MAP    # Castle exterior texture
│   │   ├── CASTIN_T.MAP  # Castle interior texture
│   │   ├── STAIRS.MAP    # Staircase texture
│   │   ├── WATER.MAP     # Water texture
│   │   ├── CAT.MAP       # Cat texture
│   │   └── SKY.MAP       # 360° panoramic sky
│   └── FPG/              # DIV2 graphic packages (.FPG)
│       └── MODE9.FPG     # Billboard sprites (torches, items…)
├── HELP_MODE9_EN/        # HTML documentation — English
│   └── index.html        # Start here
├── HELP_MODE9_ES/        # HTML documentation — Spanish
│   └── index.html        # Punto de entrada
└── README.md
```

> The example `.PRG` source files are written in Spanish, by request from readers of the author's blog at [elgeneralfailure.com](https://elgeneralfailure.com).

---

## Getting Started

1. Copy `BIN/MODE9.DLL` into your DIV2 project folder (or into DIV2's `DLL/` directory).

2. Import the library and declare the `m9` struct and `c_m9` constant — the DLL cannot create DIV2 language types, so they must live in the PRG:

   ```
   PROGRAM my_game;
   IMPORT "MODE9.DLL";

   CONST
       c_m9 = 200;        // logical c_type for mode 9 billboards

   GLOBAL
       STRUCT m9          // camera struct (same fields as m8)
           z;
           camera;
           height;
           angle;
       END
   ```

3. Load the world and start the renderer in `BEGIN`:

   ```
   BEGIN
       set_mode(320200);
       set_fps(60, 0);

       LOAD_WLD9("CASTLE.OBJ", "MAP/CASTLE.MAP", 100);
       LOAD_SKY9("MAP/SKY.MAP");
       MODE9_STEP(70);              // auto-climb stairs up to 70 units

       MODE9_SET(70, 0);            // eye height=70, pitch=0 (horizon)
       START_MODE9(id, OFFSET m9.z, 0);

       LOOP
           if (key(_w)) advance(20); end
           if (key(_s)) advance(-20); end
           if (key(_left))  angle += 4000; end
           if (key(_right)) angle -= 4000; end
           if (key(_esc)) break; end
           FRAME;
       END
       STOP_MODE9();
   END
   ```

4. From there, follow the staged examples in `EXAMPLES/PRG/`: each one adds exactly one new concept on top of the previous (billboards → 3D models → animation → level transitions).

The documentation covers all functions in detail, with code examples for each:

- **English:** open `HELP_MODE9_EN/index.html`
- **Spanish:** open `HELP_MODE9_ES/index.html`

---

## Building from Source

The source in `SRC/MODE9.CPP` targets the DIV Games Studio 2 DLL API and was compiled with **Watcom C++ 10.6** (32-bit flat-model DOS DLL). To rebuild:

1. Install Watcom C++ 10.6 or [Open Watcom](https://github.com/open-watcom/open-watcom-v2).
2. Add `div.h` (DIV2 DLL SDK header) to the include path.
3. Compile as a 32-bit flat-model DLL targeting the DIV2 runtime.

The pre-built `BIN/MODE9.DLL` works out of the box — recompilation is only needed to change the mesh/object limits or the rendering internals.

---

## Requirements

- DIV Games Studio 2 running on DOS or a compatible environment (e.g. DOSBox, DOSBox-X)
- A triangulated, Y-up Wavefront `.OBJ` file for the scenario (exportable from Blender, 3ds Max, etc.)
- Textures in DIV2 `.MAP` format

---

## License

### Source code — MIT License

The **source code** of this library (the "Software") is released under the MIT
License. You are free to download, modify, reuse, and redistribute it; the only
condition when redistributing is to keep the copyright notice and attribution.

MIT License

Copyright © 2026 Sebastian J. Moncho Maquet

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### Assets — not covered by the MIT License

The MIT License above applies **only to the source code**. The bundled assets
(graphics, sprites, 3D models, textures, and characters) are **not** licensed
for reuse in other projects. Some of them resemble characters owned by third
parties and may be subject to their copyright/trademark restrictions. They are
included here solely for demonstration and preservation purposes, and must not
be redistributed or reused outside this project.

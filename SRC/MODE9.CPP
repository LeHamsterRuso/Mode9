//------------------------------------------------------------------------------
// TITLE:       MODE9.CPP
// AUTHOR:      Sebastian J. MONCHO MAQUET
// DATE:        06/06/2026
// DESCRIPTION: "Mode 9" for DIV Games Studio 2 - navigable 3D scenario (OBJ)
//
// Like mode 8 (.WLD sector maps) but the SCENARIO is an arbitrary 3D
// mesh loaded from an .OBJ file (walls, floors and ceilings at
// any slope). The camera is a DIV2 PROCESS (like in mode 8):
// it moves on the ground plane with advance()/xadvance(), turns with its angle and
// goes up/down with z. Over the 3D scenario, 2D processes are drawn in the style of
// mode 7 (sprites scaled by distance, Doom / Daggerfall style).
//
// The renderer reuses the perspective-correct textured rasterizer from
// KD3DT and adds: first-person camera transform, clipping against
// the near plane (near clipping), ambient color, distance fog
// (Doom-style colormap) and sprite projection (billboards).
//
// FUNCTIONS EXPORTED TO THE PRG (analogous to mode 8's):
//
//   LOAD_WLD9(obj,tex,scale)  -> tris loaded (0 on failure). Loads the scenario
//                               in ONE call: OBJ mesh + .MAP texture + scale
//                               (DIV2 units per OBJ unit). tex="" or "0" = no
//                               texture (flat-shaded). scale<=0 = unchanged.
//                               Like load_wld(): switch "level".
//   MODE9_SETTEX(map)         -> 1/0. Change the texture separately (optional).
//   LOAD_SKY9(map)            -> 1/0. Panoramic sky (.MAP wrapping 360 in
//                               horizontal). Replaces the flat background color;
//                               turns with the yaw and shifts with the pitch. Same
//                               palette as the game. SKY9_OFF() disables it.
//   SKY9_OFF()                -> 0. Back to the flat background (SET_ENV_COLOR9).
//   MODE9_LOADFPG(fpg)        -> num graphics. Loads an FPG IN THE DLL so that
//                               it draws the billboards (c_m9) itself in EACH
//                               viewport (needed for split-screen). Without this
//                               billboards use the c_screen path (1 view).
//   START_MODE9(camera,m9,reg)-> 1. Enables the render (signature like start_mode8).
//                               camera=id of the camera process; m9=&m9 (address
//                               of the STRUCT, see note); reg=region: 0=full
//                               screen, n=region defined with MODE9_REGION
//                               (several calls = split by regions, like mode 8's
//                               windows). Integrates the BIND: the DLL reads
//                               m9.camera/height/angle from &m9 every frame.
//   MODE9_REGION(n,x,y,w,h)   -> 1/0. Defines the rect of region n (1..15), like
//                               define_region() but for mode 9. Use it with the
//                               3rd parameter of START_MODE9. Region 0 is always
//                               the full screen. w/h<=0 clears the region.
//   MODE9_FOV(degrees)        -> 0. Horizontal field of view (def ~71). 60=zoom,
//                               90=wide, 110=ultra-wide. Affects all
//                               views. Leave it if you want the default value.
//   STOP_MODE9()              -> 0. Disables the scenario render.
//   MODE9_SCRW() / MODE9_SCRH() -> width / height of the current screen (px). To
//                               adapt mouse and viewports to any resolution.
//   MODE9_SET(height, pitch)  -> 0. Eye height and vertical angle (pitch) of
//                               the camera. Equivalent to m8.height and m8.angle.
//   MODE9_UNITSCALE(n)        -> 0. DIV2 units per OBJ unit (def 100).
//   MODE9_BIND(&m9.z)         -> 0. (optional; START_MODE9 already does it) Binds the
//                               PRG's m9 STRUCT.
//   MODE9_CULL(mode)          -> 0. 0=no culling, 1=back faces (def), 2=inverted.
//   MODE9_FARCLIP(dist)       -> 0. Don't draw beyond 'dist' DIV2 units (0=off).
//                               Affects world, OBJ and billboard polygons.
//                               With fog it also clips at fog_far.
//   SET_ENV_COLOR9(r,g,b)     -> 0. Background/ambient color, 0..100 per channel
//                               (DIV2 convention; 100=white).
//   SET_FOG9(near, far)       -> 0. Distance fog (DIV2 units). The COLOR is
//                               that of SET_ENV_COLOR9 (like mode 8's set_fog).
//   MODE9_FOG_OFF()           -> 0. Disables the fog.
//   MODE9_OCCLUDE(on)         -> 0. 1=hide billboards covered by geometry
//                               (z-buffer, def), 0=always draw them.
//   MODE9_COLLISION(on)       -> 0. 1=advance/xadvance don't go through walls
//                               (uses the camera process's 'radius' field), 0=off.
//                               ON by default: you only need to call it with 0
//                               if you want to DISABLE collisions.
//   MODE9_COLLIDE(pid)        -> 1/0. Pushes ANY process out of the
//                               walls (sliding) and rewrites its x,y. For NPCs.
//                               Returns 1 if it moved it. Uses its 'radius' (def 30).
//                               With MODE9_STEP>0 it also rests it on the floor.
//   MODE9_STEP(height)        -> 0. Stairs and ramps: max height (DIV2 units) the
//                               player climbs by itself. Walls lower than that
//                               don't block and it rests on the floor below
//                               (steps, ramps). 0=off. GRAVITY is applied by the
//                               PRG (z-=...); the DLL only RAISES z when stepping on floor.
//   MODE9_SPLIT(slot,cam,m9,x,y,w,h) -> 1. Defines a viewport (split-screen):
//                               up to 4 cameras, each with its m9 and its rect.
//   MODE9_DOWNSCALE(n)        -> 0. Render below the native resolution
//                               (1=native def, 2/3/4 = faster, more pixelated).
//   MODE9_PERSPECTIVE(on)     -> 0. Texture correction: 0=performance (affine per
//                               16px, def), 1=quality (exact 1/w per pixel).
//
//   MODE9_BILLBOARD(pid,wx,wy,wz,basesize) -> 1 visible / 0 hidden
//      Places process 'pid' as a 2D sprite at world position (wx,wy,wz)
//      (DIV2 units). Sets its x,y (screen), size (scale by distance) and
//      z (draw priority). Returns 0 and sets size=0 if it's behind the camera
//      OR if covered by geometry (z-buffer occlusion, see
//      MODE9_OCCLUDE). It's the "mode 7" way to see processes in the 3D scenario.
//      ANGULAR GRAPHICS (like mode 7): if the process has 'xgraph' = &table
//      (table[0]=N graphics, table[1..N]=codes spread over 360deg) and 'angle' =
//      object orientation, the DLL picks the graphic by the angle from which
//      the camera sees it (like start_mode7 + ctype=c_m7 + xgraph).
//
//   MODE9_PROJECT(wx,wy,wz,&sx,&sy,&sz) -> 1 visible / 0 hidden
//      Like BILLBOARD but writes the result into the pointed-to variables
//      instead of touching a process (to position sprites manually).
//
// WORLD COORDINATE SYSTEM (DIV2 units, divided by UNITSCALE -> OBJ):
//   X = process.x  (ground plane, east/west)
//   Z = process.y  (ground plane, north/south)
//   Y (height)= process.z + m9.height   (vertical axis, Y-up as in Blender/OBJ)
//   yaw  = process.angle (thousandths of a degree: pi = 180000)
//   pitch= m9.angle      (thousandths of a degree; 0 = horizon)
//
// NOTE ABOUT m9 / c_m9:
//   A DLL cannot create new STRUCTs or c_types of the DIV2 language (the
//   compiler generates them). The m9 struct (z,camera,height,angle) and the
//   c_m9 constant are declared in the PRG (see MODE9.PRG) and passed to this DLL with
//   START_MODE9 + MODE9_SET. The sprite-processes use ctype=c_screen and are
//   positioned with MODE9_BILLBOARD; they don't need a real engine c_type.
//------------------------------------------------------------------------------

#define GLOBALS
#include "div.h"
#include <math.h>
#include <string.h>

// ---- Mesh limits (scenarios larger than a single model) ------
#define MAX_VERTS   4096
#define MAX_UVS     4096
#define MAX_TRIS    8192
#define MAX_WALLS   4096      // 2D (XZ) segments for wall collision
#define MAX_FLOORS  4096      // "floor" triangles (non-vertical) for steps/ramps
#define MAX_VIEWS   4         // simultaneous viewports (split-screen)

// ---- Collision -----------------------------------------------------------
#define WALL_NY     0.5f      // |normal.y|<WALL_NY -> "wall" triangle (vertical)
#define PLAYER_BODY 1.8f      // player body height (OBJ units) downward
#define COLLIDE_DEF 30        // default collision radius (DIV2 units) if radius<=0

// ---- Flat shading palette (tris without UV) -----------------------------------
#define PAL_BASE    224       // 224..239: flat shading ramp
#define PAL_SHADES   16
#define PAL_ENV     223       // 1 entry reserved for the background color
#define AMBIENT     0.25f

// ---- Fog -----------------------------------------------------------------
#define FOG_LEVELS  16        // fog colormap levels

// ---- Projection -------------------------------------------------------------
#define NEAR_PLANE  0.06f     // near plane (OBJ units)
#define FOCAL_K     0.70f     // focal = width * FOCAL_K  (~71 degrees of FOV)
#define TEX_SEG     16        // px between perspective divisions (affine correction)

// ---- Optimization toggles (to isolate bugs) ----------------------
// 0 = disabled (classic/safe behavior). If something breaks, set to 0.
#define M9_FAST_FTOL 1        // Tier 1: float->int via "magic number" (volatile version)
#define M9_USE_MIP   1        // Tier 2: texture mipmap
#define M9_USE_GRID  1        // Tier 3: spatial culling grid
#define M9_CORNER_SLIDE 1     // Collision: don't "cap" the wall ends at the
                              // interior vertices (joints/corners) -> don't get stuck.
                              // 0 = classic behavior (radial cap at every end).
#define M9_COLLIDE_GRID 1     // Collision: spatial wall grid (only tests the
                              // ones near the player). 0 = scan all (classic).

// ---- Field offsets within a process (in ints, base = id) ------------
// (derived from _reserved[20] + _process fields in div.h)
#define P_ID      0
#define P_STATUS  4       // 0=dead, 2=alive (see LTOBJ.DEF reserved.status)
#define P_CTYPE   25
#define P_X       26
#define P_Y       27
#define P_Z       28
#define P_GRAPH   29
#define P_SIZE    31
#define P_ANGLE   32
#define P_XGRAPH  35      // pointer to angular graphics table (like mode 7)
#define P_HEIGHT  36
#define P_RADIUS  39      // process collision radius (DIV2 units)

// mode 9 "logical" ctype: a process with ctype=c_m9 is drawn as a billboard
// automatically (like c_m7 in mode 7). Default value 200 (doesn't collide
// with c_screen=0/c_scroll=1/c_m7=2); adjustable with MODE9_CTYPE. The PRG's
// c_m9 constant must hold the same value.
#define C_SCREEN  0       // DIV2 screen ctype (LTOBJ.DEF)
#define MAX_SPR   256     // max simultaneous c_m9 billboards

// ---- Types ------------------------------------------------------------------
typedef struct { float x, y, z;               } V3;
typedef struct { float u, v;                  } UV2;
typedef struct { int a,b,c; int ua,ub,uc; unsigned char flat; unsigned char mat; } TriTex;
typedef struct { float depth; int idx; int ci;} TSort;
// Vertex during clipping: position in camera space + UV
typedef struct { float cx,cy,cz,u,v;          } CVert;

// ---- Mesh data ------------------------------------------------------
static V3     mesh_v[MAX_VERTS];
static UV2    mesh_uv[MAX_UVS];
static TriTex mesh_t[MAX_TRIS];
static int n_verts=0, n_uvs=0, n_tris=0;
static int mesh_ok=0, has_uvs=0;

// ---- Walls for collision (2D footprint in XZ + vertical extent) ---------
// vi0/vi1 = vertex indices (mesh_v) of the segment ends.
// cap0/cap1 = 1 if that end generates a radial "cap" in the collision (free
// end / real corner), 0 if it's an INTERIOR vertex shared by another wall
// (joint or corner): omitting its cap avoids the player getting stuck (M9_CORNER_SLIDE).
typedef struct { float x0,z0,x1,z1, ymin,ymax; int vi0,vi1; unsigned char cap0,cap1; } M9Wall;
static M9Wall mesh_walls[MAX_WALLS];
static int    n_walls=0;
static int    wall_vc[MAX_VERTS];    // num walls touching each XZ group (for cap0/cap1)
static int    vgroup[MAX_VERTS];     // vertex -> representative of its XZ coincidence group

// ---- Floors for steps and ramps (floor-following) -----------------------
// Indices of the near-horizontal UPWARD-facing triangles (floors only, no
// ceilings/roofs: see load classifier). floor_height() finds
// the HIGHEST floor under the player (<= feet + m9_step) and raises the feet onto it:
// this climbs sloped ramps and steps without sinking into floors at a different level.
static int    mesh_floor_idx[MAX_FLOORS];
static int    n_floors=0;
// ---- Ceilings for HEAD collision ----------------------------------------
// Indices of the near-horizontal DOWNWARD-facing triangles (roofs/ceilings).
// ceil_height() finds the LOWEST ceiling above the player; the collision then
// caps the feet so the head (feet + PLAYER_BODY) can't go through it. This is
// what stops you from jumping THROUGH a ceiling when collisions are on.
static int    mesh_ceil_idx[MAX_FLOORS];
static int    n_ceils=0;
// Max auto-climbable step/ramp height (OBJ units). 0 = disabled
// (classic behavior: the DLL doesn't touch z, the PRG controls the height).
static float  m9_step=0.0f;

// ---- 3D object models (LOAD_OBJ9 / DRAW_OBJ9) --------------------------
// Let you place textured 3D meshes in the world as NPCs/props. Each
// model is an OBJ loaded with its own scale and texture. Instances are
// queued with DRAW_OBJ9 (call every frame) and rendered in render_view
// with the same z-buffer as the world mesh (correct occlusion).
#define MAX_OBJ9_MODELS  8    // distinct models
#define MAX_OBJ9_INST   32    // instances per frame
#define MAX_OBJ9V      512    // vertices per model
#define MAX_OBJ9UV    1024
#define MAX_OBJ9T     1024    // triangles per model
typedef struct {
    V3    *verts;             // div_malloc; = vframes (frame 0). Owner of the memory.
    V3    *vframes;           // nv*nframes contiguous verts; frame f at vframes+f*nv
    int    nframes;           // animation frames (1 = static model)
    UV2   *uvs;
    TriTex *tris;
    int    nv, nuv, nt;
    char  *tex; int tw, th;  // MAP texture (NULL = flat)
    char  *mip; int mw, mh;  // mipmap 1/2 (LOD/cache)
    int    ready;            // 0=loading (in background), 1=ready, -1=failed
} Obj9Model;
typedef struct {
    int   midx;              // index into obj9_models[]
    float ix, iy, iz;        // position in world space (OBJ units)
    float ysin, ycos;        // precomputed Y rotation from the DIV2 angle
    int   fa, fb;            // frames to interpolate (fa->fb)
    float fblend;            // blend 0..1 between fa and fb (0 = only fa)
} Obj9Inst;
static Obj9Model obj9_models[MAX_OBJ9_MODELS];
static int       n_obj9_models = 0;
static Obj9Inst  obj9_queue[MAX_OBJ9_INST];
static int       n_obj9_queue  = 0;

// ---- Model loading IN THE BACKGROUND (non-blocking) ----------------------
// LOAD_OBJ9/LOAD_OBJ9_ANIM reserve the slot, return the handle instantly and
// queue a "job". obj9_pump() (in background_to_buffer, 1 file per frame)
// parses it: so loading doesn't freeze the game nor trigger the watchdog (err 142)
// with many-frame animations. The model isn't drawn until ready==1; the
// DRAW_OBJ9 instances of a not-yet-ready model are ignored (it appears once loaded).
// It's not multithreaded (DOS is single-threaded): it's chunked streaming across frames.
#define OBJ9_KIND_STATIC 0
#define OBJ9_KIND_ANIM   1
typedef struct {
    int   midx;              // target model slot
    int   kind;              // OBJ9_KIND_STATIC / OBJ9_KIND_ANIM
    char  oname[64];         // OBJ (static) or anim base ("CAT" -> CATn.OBJ)
    char  tname[64];         // MAP texture ("" = flat)
    float sfac;              // scale (DIV2 units per OBJ unit)
    int   nframes;           // frames (anim)
    int   step;              // state: 0=topology/frame0, 1..nf-1=frame, nf=texture
    V3   *block;             // frame block under construction (anim)
    int   nv;                // num vertices of frame 0 (anim)
} Obj9Job;
static Obj9Job   obj9_jobs[MAX_OBJ9_MODELS];   // FIFO; at most 1 job per model
static int       n_obj9_jobs   = 0;            // queued jobs
static int       obj9_job_head = 0;            // job in progress (advances when finished)
// Transform scratch for the current model (reused per instance)
static float     obj9cv[MAX_OBJ9V][3];  // camera space
static float     obj9px[MAX_OBJ9V];     // screen subpixel X
static float     obj9py[MAX_OBJ9V];     // screen subpixel Y
static char      obj9bh[MAX_OBJ9V];     // 1 = behind the near plane

// ---- Render buffers ------------------------------------------------------
static float camv[MAX_VERTS][3];   // vertices in camera space
static int   psx[MAX_VERTS], psy[MAX_VERTS];  // projection in INTEGER screen (culling)
static float psxf[MAX_VERTS], psyf[MAX_VERTS]; // projection in SUBPIXEL screen (rasterizing)
static char  pbehind[MAX_VERTS];   // 1 if the vertex is behind the near plane
static TSort tsort[MAX_TRIS];
// Bucket sort by depth (front-to-back order in O(n))
#define NBUCKETS 256
static int   order[MAX_TRIS];      // indices into tsort[] sorted near->far
static int   bcnt[NBUCKETS+1];     // counts/prefixes of the bucket sort

// ---- Materials (multi-texture) ---------------------------------------------
// Up to MAX_MATS distinct materials (by order of 'usemtl' appearance in the
// OBJ). mat_data[0] is the default material (the one loaded with LOAD_WLD9 or
// MODE9_SETTEX). Each triangle stores its material index (TriTex.mat) and the
// rasterizer selects the correct texture; if mat_data[mat]==NULL it uses mat 0.
// MODE9_MATERIAL(idx,"MAP/x.MAP") assigns a texture to any slot.
#define MAX_MATS 16
static char *mat_data[MAX_MATS];   // texture pixels (NULL = unassigned)
static int   mat_w[MAX_MATS];
static int   mat_h[MAX_MATS];
static char *mat_mip[MAX_MATS];   // level-1 mipmap (NULL if none)
static int   matm_w[MAX_MATS];
static int   matm_h[MAX_MATS];
static int   n_mats = 1;           // num distinct materials in the current OBJ

// ---- Skybox / panoramic sky (cylindrical 360deg) ----------------------------
// Instead of filling the background with a flat color, a panoramic texture is mapped:
// the texture column comes from the camera yaw (wraps 360deg) and the row from the
// pitch (look up/down). It's the efficient equivalent of Doom/Build's "sky"
// (a 6-face cubemap would be much more expensive per pixel on a 486).
#define SKY_MAXDIM 2048
static char *sky_data = NULL;
static int   sky_w = 0, sky_h = 0, sky_on = 0;
static int   sky_coldelta[SKY_MAXDIM];   // screen column -> texture column delta
static int   sky_cd_vw = 0, sky_cd_vh = 0, sky_cd_w = 0; // cache: vw/vh/sky_w for which the tables are computed

// ---- Billboard sprites drawn by the DLL (multi-view) ----------------
// The DLL can't read the FPG graphics that DIV2 loads, so it parses the FPG
// itself (format in div.h) and stores the graphics by code. This lets it
// draw the billboards in EACH viewport (split-screen) with per-pixel occlusion.
#define MAX_FPGSPR 128
typedef struct { int code,w,h,cx,cy; char *pix; } FpgSpr;
static FpgSpr fspr[MAX_FPGSPR];
static int    n_fpgspr = 0;

// ---- Z-buffer (occlusion) ---------------------------------------------------
// 16 BITS (unsigned short): stores 1/w in fixed point (1/w * ZSCALE). Larger =
// closer. Half the memory and bandwidth of a 32-bit z-buffer
// (better cache use in DOSBox). Per-pixel comparison in integers.
// ZSCALE is chosen so that 1/NEAR_PLANE * ZSCALE fits in 16 bits (<=65535):
//   1/0.06 = 16.67  ->  ZSCALE <= 65535*0.06 = 3932  ->  we use 3900.
#define ZSCALE 3900.0f
static unsigned short *zbuf = NULL;
static int  zbuf_w = 0, zbuf_h = 0;

// ---- XZ spatial grid (per-cell triangle culling) -----------------
// The scenario is split into a GRID_DIM x GRID_DIM grid over the XZ plane.
// Each cell stores the indices of the triangles whose footprint overlaps it. When
// rendering, only the triangles of the cells inside the horizontal frustum
// + far clip are considered, instead of scanning all n_tris every frame.
#define GRID_DIM   16
#define GRID_CELLS (GRID_DIM*GRID_DIM)
static int   grid_start[GRID_CELLS+1];   // prefixes: triangles of cell c in [start[c],start[c+1])
static int  *grid_items=NULL;            // (div_malloc) triangle indices grouped by cell
static int   grid_cap=0;                  // current capacity of grid_items (in ints)
static int   grid_n=0;                    // 0 = no grid (fallback: scan everything)
static float grid_minx=0,grid_minz=0;     // grid origin (OBJ units)
static float grid_cw=1,grid_ch=1;         // cell size (OBJ units)
static float grid_invcw=1,grid_invch=1;
static float grid_cellrad=1;              // radius (half diagonal) of a cell
static int  *tstamp=NULL;                 // (div_malloc) frame stamp per triangle
static int   tstamp_cap=0;
static int   frame_stamp=0;

// ---- WALL spatial grid (collision culling) ----------------------
// Reuses the cell geometry of the render grid (grid_minx/cw/inv...).
// Each cell lists the walls whose footprint overlaps it; collision only tests
// the walls of the cells near the player, not all n_walls.
static int   wgrid_start[GRID_CELLS+1];   // prefixes per cell
static int  *wgrid_items=NULL;            // (div_malloc) wall indices per cell
static int   wgrid_cap=0;
static int   wgrid_n=0;                    // 0 = no wall grid (scan all)
static int   wcand[MAX_WALLS];            // candidates of the current collision query
static int   n_wcand=0;
static int   wstamp[MAX_WALLS];           // dedup: wall already added in this query
static int   wstamp_frame=0;

// ---- LAZY vertex transform -----------------------------
// A vertex is only transformed/projected the first time a VISIBLE triangle
// needs it (marked with vstamp == frame_stamp). So if the grid discards
// whole cells, their vertices are never touched. The camera params of the current
// view are stored in g_* so that xform_vertex() (called from consider_tri)
// uses them without passing them as arguments.
static int   vstamp[MAX_VERTS];           // frame stamp per vertex (already transformed?)
static float g_Cx,g_Cy,g_Cz,g_ysin,g_ycos,g_psin,g_pcos,g_focal,g_hw,g_hh;

// ---- Trig table (SNES Doom style) ---------------------------------
// 4096 sin/cos entries avoid the sinf/cosf calls in the rendering
// loop (~40 cycles each on a 486 -> 4-8 cycles with a table).
#define TRIG_SIZE 4096
#define TRIG_MASK (TRIG_SIZE-1)
#define TRIG_K    651.8986f   // 4096/(2*pi): rad -> index
static float sin_tab[TRIG_SIZE];
static float cos_tab[TRIG_SIZE];

// ---- Downscale / line doubler -----------------------------------------------
// 1 = native resolution (def); 2 = half the lines (double -> ~2x fill rate);
// 3 = one third; 4 = one quarter. Same trick as SNES Doom: compute 1 of every
// N scanlines and duplicate them. Use MODE9_DOWNSCALE(n) from the PRG.
static int m9_downscale = 1;

// ---- Texture perspective correction --------------------------------
// Length (px) of the affine span between two exact 1/w divisions in hspan_z.
// = TEX_SEG (16): PERFORMANCE (the texture is interpolated linearly within the span;
//   fast, but "wobbles" a bit on surfaces at steep angles).
// = 1: QUALITY (an exact 1/w per pixel -> perfect perspective, slower).
// Performance by default. Changed with MODE9_PERSPECTIVE(on).
static int m9_texseg = TEX_SEG;
// EFFECTIVE segment of the current triangle. raster_tri_z drops it to 1 (per pixel)
// automatically when the triangle has strong perspective (large depth ratio
// between its vertices: geometry crossing the near plane "entering through the
// camera", or very steep floors/ceilings), which is where the 16px affine
// correction warps the texture. The rest still uses m9_texseg (fast).
static int cur_seg = TEX_SEG;

// ---- Clip rectangle of the current viewport (for split-screen) ----------
// The z-buffer and the video buffer use the FULL width as stride; the
// drawing is limited to [clip_x0,clip_x1) x [clip_y0,clip_y1).
static int clip_x0=0, clip_x1=0, clip_y0=0, clip_y1=0;

// ---- Camera / mode state ------------------------------------------------
static int   m9_active   = 0;
static int   cull_mode   = 1;      // 0=no cull, 1=cull back faces, 2=cull front faces
static int   m9_camera   = 0;      // id of the camera process
static int   m9_height   = 0;      // eye height (DIV2 units)
static int   m9_pitch    = 0;      // vertical angle (thousandths of a degree)
static float m9_unitscale= 100.0f; // DIV2 units per OBJ unit
static int   m9_bind     = 0;      // offset in mem[] of the m9 STRUCT (0=not bound)
static int   m9_region   = 0;      // last region passed to START_MODE9 (0=full)
static int   m9_occlude  = 1;      // 1=hide billboards behind geometry (z-buffer)
static int   m9_ctype    = 200;    // value of c_m9 (automatic billboard ctype)
static int   m9_collision= 1;      // 1=advance/xadvance don't go through walls (ON by default)
static float far_clip    = 0.0f;   // max draw distance in OBJ units (0=no limit)
// Focal constant: focal = viewport_width * m9_focal_k. Relation with the horizontal
// FOV: m9_focal_k = 0.5/tan(FOV/2). Default FOCAL_K (~71 degrees).
// Changed with MODE9_FOV(degrees); affects projection AND frustum culling.
static float m9_focal_k  = FOCAL_K;

// ---- Screen regions (like define_region + start_mode8) ----------------
// The DLL can't read DIV2's region table, so it keeps its own:
// MODE9_REGION(n,x,y,w,h) registers the rectangle of region n (1..MAX_REGIONS-1)
// and START_MODE9(cam,m9,n) uses it as a viewport. Region 0 is ALWAYS the full
// screen (can't be redefined), like in start_mode8.
#define MAX_REGIONS 16
static int reg_def[MAX_REGIONS];                 // 1 = region n defined
static int reg_x[MAX_REGIONS], reg_y[MAX_REGIONS], reg_w[MAX_REGIONS], reg_h[MAX_REGIONS];

// ---- Viewports (split-screen): each view has its camera, its m9 and its rect ---
// vw==0 (or vh==0) means "full screen" (resolved when rendering).
// lastx/lastz: last valid camera position (for swept collision).
// region: number of the region this view serves (-1 = none; to reuse the slot).
typedef struct { int active,cam,bind,vx,vy,vw,vh,haslast,region; float lastx,lastz; } M9View;
static M9View views[MAX_VIEWS];

// State to convert the c_m9 processes to c_screen and restore them after
// DIV2 paints them (so they keep their WORLD coordinates in x,y,z).
typedef struct { int base,wx,wy,wz,sz,gr,ct; } M9Spr;
static M9Spr m9spr[MAX_SPR];
static int   n_m9spr = 0;

// ---- Ambient / background color ----------------------------------------------
static int env_r=0, env_g=0, env_b=0;

// ---- Fog -----------------------------------------------------------------
static char *fog_map = NULL;       // FOG_LEVELS * 256 bytes (color remap)
static int   fog_on  = 0;
static float fog_near=0.0f, fog_far=1.0f;   // in OBJ units
// Factor (FOG_LEVELS-1)/(fog_far-fog_near): converts depth -> fog level
// with a single multiply (avoids the fog_level division per pixel/segment).
static float fog_scale=0.0f;

// ---- Original palette (to restore at divend) -----------------------------
static char orig_pal[(PAL_SHADES+1) * 3];

// ---- Status message ------------------------------------------------------
static char m9_msg[40];

// ---- Pointers to the DIV2 runtime -----------------------------------------------
static void *(*g_import)(char *name) = 0;
static void  (*g_export)(char *name, void *p) = 0;

// ============================================================================
// String utilities (without libc)
// ============================================================================
static void s_cpy(char *d,const char *s){while(*s)*d++=*s++;*d=0;}
static void s_cat(char *d,const char *s){while(*d)d++;while(*s)*d++=*s++;*d=0;}
static void s_itos(int n,char *b){
    char t[12];int i=0,j=0;
    if(!n){b[0]='0';b[1]=0;return;}
    if(n<0){*b++='-';n=-n;}
    while(n){t[i++]=(char)('0'+n%10);n/=10;}
    while(i>0)b[j++]=t[--i];b[j]=0;
}

static void div2c(int off,char *buf,int max){
    int i;char *src=(char*)&mem[off];
    for(i=0;i<max-1&&src[i];i++)buf[i]=src[i];buf[i]=0;
}

static float sc_atof(const char *s){
    float v=0.0f,frac=1.0f;int neg=0;
    while(*s==' '||*s=='\t')s++;
    if(*s=='-'){neg=1;s++;}else if(*s=='+')s++;
    while(*s>='0'&&*s<='9')v=v*10.0f+(float)(*s++-'0');
    if(*s=='.'){s++;while(*s>='0'&&*s<='9'){frac*=0.1f;v+=(float)(*s++-'0')*frac;}}
    return neg?-v:v;
}

static int sc_atoi(const char *s){
    int v=0,neg=0;
    while(*s==' '||*s=='\t')s++;
    if(*s=='-'){neg=1;s++;}
    while(*s>='0'&&*s<='9')v=v*10+(*s++-'0');
    return neg?-v:v;
}

static int read_line(FILE *f,char *buf,int max){
    int c,n=0;
    while(n<max-1){c=fgetc(f);if(c==EOF){buf[n]=0;return n>0;}if(c=='\n')break;if(c!='\r')buf[n++]=(char)c;}
    buf[n]=0;return 1;
}

static char *next_tok(char *s){
    while(*s&&*s!=' '&&*s!='\t'&&*s!='\n')s++;
    while(*s==' '||*s=='\t')s++;
    return s;
}

static char *parse_face_vert(char *s,int *vi,int *uvi){
    *vi=sc_atoi(s)-1; *uvi=-1;
    while(*s&&*s!='/'&&*s!=' '&&*s!='\t'&&*s!='\n')s++;
    if(*s=='/'){
        s++;
        if(*s&&*s!='/'&&*s!=' '&&*s!='\t'&&*s!='\n')
            *uvi=sc_atoi(s)-1;
        while(*s&&*s!='/'&&*s!=' '&&*s!='\t'&&*s!='\n')s++;
        if(*s=='/'){ s++; while(*s&&*s!=' '&&*s!='\t'&&*s!='\n')s++; }
    }
    return s;
}

// ============================================================================
// Fast inverse square root (Quake's fast inverse sqrt).
// ~0.2% precision after 1 Newton iteration: enough for collision/normal.
// Avoids sqrt() calls in the collision loop (O(n_walls)/frame).
// ============================================================================
static float fast_rsqrt(float x){
    union { float f; unsigned long i; } u;
    float xh=0.5f*x;
    u.f=x; u.i=0x5f3759dfUL-(u.i>>1);
    u.f=u.f*(1.5f-xh*u.f*u.f);
    return u.f;
}

// ============================================================================
// Fast reciprocal 1/x (bit-hack + 1 Newton step, ~0.05% error). Avoids the
// FPU division (~73 cycles on a 486) in the rasterizer (1/w per segment,
// 1/dy slopes per triangle). Same order of error as the affine approximation.
// ============================================================================
static float fast_recip(float x){
    union { float f; unsigned long i; } u;
    float xx=x;
    u.f=x; u.i=0x7EF127EAUL-u.i;       // initial approximation of 1/x
    u.f=u.f*(2.0f-xx*u.f);            // 1 Newton-Raphson step
    return u.f;
}

// ============================================================================
// fast float -> int ("magic number" trick). Watcom compiles each (int)float
// as a call that saves/changes/restores the FPU control word (~50-60
// cycles); this eliminates it (round to nearest, x86 little-endian, |x|<2^31).
// ============================================================================
// Integer ceil of a float, without depending on ceilf (Watcom 10.6). For the
// subpixel rasterizing: first pixel center (x+0.5) inside an edge.
static int iceil(float a){ int i=(int)a; return (a>(float)i)?i+1:i; }

static int fast_ftol(float x){
#if M9_FAST_FTOL
    // 'volatile' FORCES storing the double in 64-bit memory (rounding
    // from the 80 bits of the x87 register) BEFORE reading the low dword. Without it
    // Watcom 10.6 kept the value in the extended register and reading the
    // union gave garbage -> crazy coordinates -> crash. (x86 little-endian.)
    volatile union { double d; int l[2]; } u;
    u.d=(double)x+6755399441055744.0;   // 2^52 + 2^51
    return u.l[0];                       // low dword = rounded integer
#else
    return (int)x;                       // classic (safe) conversion
#endif
}

// ============================================================================
// Locates a process by its id in mem[]. Returns the base offset or -1.
// ============================================================================
static int find_process(int pid){
    int first,last,step,b;
    step = *_process_size;
    if(step<=0) return -1;
    first= *_id_init_offset;
    last = *_id_end_offset;
    for(b=first; b<=last; b+=step){
        if(mem[b+P_ID]==pid) return b;
    }
    return -1;
}

// ============================================================================
// Nearest palette color (DIV2 palette, 6-bit values in char*)
// ============================================================================
static int nearest_pal(int r,int g,int b){
    int i,best=0; long bestd=0x7fffffffL;
    for(i=0;i<256;i++){
        int pr=(unsigned char)palette[i*3+0];
        int pg=(unsigned char)palette[i*3+1];
        int pb=(unsigned char)palette[i*3+2];
        long dr=pr-r,dg=pg-g,db=pb-b;
        long d=dr*dr+dg*dg+db*db;
        if(d<bestd){bestd=d;best=i;if(!d)break;}
    }
    return best;
}

// ============================================================================
// Builds the fog colormap: for each level t and each source color,
// blends toward (fr,fg,fb) and stores the nearest palette index.
// ============================================================================
static void build_fog_map(int fr,int fg,int fb){
    int lvl,c;
    if(!fog_map){
        fog_map=(char*)div_malloc((size_t)(FOG_LEVELS*256));
        if(!fog_map){fog_on=0;return;}
    }
    for(lvl=0;lvl<FOG_LEVELS;lvl++){
        float t=(float)lvl/(float)(FOG_LEVELS-1);
        for(c=0;c<256;c++){
            int cr=(unsigned char)palette[c*3+0];
            int cg=(unsigned char)palette[c*3+1];
            int cb=(unsigned char)palette[c*3+2];
            int nr=(int)((float)cr*(1.0f-t)+(float)fr*t+0.5f);
            int ng=(int)((float)cg*(1.0f-t)+(float)fg*t+0.5f);
            int nb=(int)((float)cb*(1.0f-t)+(float)fb*t+0.5f);
            fog_map[lvl*256+c]=(char)nearest_pal(nr,ng,nb);
        }
    }
}

// Fog level index for a given depth (OBJ units). Linear with the real
// DISTANCE (like mode 8): 0 at fog_near, FOG_LEVELS-1 (opaque) at fog_far.
// fog_scale = (FOG_LEVELS-1)/(fog_far-fog_near) is precomputed in set_fog9.
static int fog_level(float depth){
    float t;
    if(!fog_on) return 0;
    t=(depth-fog_near)*fog_scale;
    if(t<=0.0f) return 0;
    if(t>=(float)(FOG_LEVELS-1)) return FOG_LEVELS-1;
    return fast_ftol(t);            // fast round-to-nearest (avoids Watcom's slow (int)
                                    // ~50-60 cycles): t is already in [0,N) and >=0

}

// ============================================================================
// Unified rasterizer with Z-BUFFER (occlusion) + perspective-correct texture
// + per-pixel fog. FRONT-TO-BACK painting: the (cheap) depth test
// rejects already-covered pixels BEFORE sampling the (expensive) texture.
//   zb stores 1/w (larger = closer). tex=NULL -> flat color 'flatcol'.
// ============================================================================
static void hspan_z(char *vid,unsigned short *zb,int W,int H,int y,
                    float xlf,float uwl,float vwl,float iwl,
                    float xrf,float uwr,float vwr,float iwr,
                    char *tex,int tw,int th,int pow2,int twsh,unsigned char flatcol){
    char *p;unsigned short *zp;float tf;
    float duw,dvw,diw,cuw,cvw,ciw,dx,subx;
    int xStart,xEnd;

    if(y<clip_y0||y>=clip_y1)return;
    // Line doubler: only render 1 of every m9_downscale lines (the rest are
    // duplicated after the render, like in SNES Doom). Lines relative to the top
    // edge of the viewport so the pattern always starts on the same row.
    if(m9_downscale>1 && ((y-clip_y0)%m9_downscale)!=0)return;
    if(xlf>xrf){
        tf=xlf;xlf=xrf;xrf=tf;
        tf=uwl;uwl=uwr;uwr=tf; tf=vwl;vwl=vwr;vwr=tf; tf=iwl;iwl=iwr;iwr=tf;
    }
    dx=xrf-xlf; if(dx<1e-4f)return;
    {float invdx=fast_recip(dx);
     duw=(uwr-uwl)*invdx; dvw=(vwr-vwl)*invdx; diw=(iwr-iwl)*invdx;}

    // SUBPIXEL: the first pixel is the center one (x+0.5) >= xlf; the texture
    // coords are PRESTEPPED to that center. This way vertices don't "snap" to integers
    // (it ends the texture shimmer/swimming of moving geometry).
    xStart=iceil(xlf-0.5f); xEnd=iceil(xrf-0.5f);
    if(xStart<clip_x0)xStart=clip_x0;
    if(xEnd>clip_x1)xEnd=clip_x1;
    if(xStart>=xEnd)return;
    subx=((float)xStart+0.5f)-xlf;
    cuw=uwl+duw*subx; cvw=vwl+dvw*subx; ciw=iwl+diw*subx;

    p=vid+y*W+xStart; zp=zb+y*W+xStart;
    {
        // Depth in fixed point: constant step across the whole scanline.
        int zstep=fast_ftol(diw*ZSCALE);
        float tuf=(float)tw, tvf=(float)th;
        int twm=tw-1, thm=th-1;       // masks (valid only if pow2)
        int total=xEnd-xStart, xi=0;
        while(xi<total){
            int seg=total-xi; if(seg>cur_seg)seg=cur_seg;
            {
                // A single 1/w division at each end of the segment; the texture
                // coords are interpolated in 16.16 FIXED POINT (integers) inside.
                float w0=(ciw>1e-6f)?fast_recip(ciw):1e6f;
                float cuw_e=cuw+duw*(float)seg, cvw_e=cvw+dvw*(float)seg, ciw_e=ciw+diw*(float)seg;
                float w1=(ciw_e>1e-6f)?fast_recip(ciw_e):1e6f;
                float invseg=fast_recip((float)seg);
                float tu0=cuw*w0*tuf, tv0=cvw*w0*tvf;     // texel at the left end
                float tu1=cuw_e*w1*tuf, tv1=cvw_e*w1*tvf; // texel at the right end
                int tu=fast_ftol(tu0*65536.0f), tv=fast_ftol(tv0*65536.0f);
                int stu=fast_ftol((tu1-tu0)*invseg*65536.0f);
                int stv=fast_ftol((tv1-tv0)*invseg*65536.0f);
                int zfix=fast_ftol(ciw*ZSCALE);            // per-segment resync (no drift)
                // Fog by the real DEPTH of the segment (w0=1/ciw). Linear with
                // distance (like mode 8); 1 level per cur_seg px span.
                int lv=fog_on?fog_level(w0):0;
                char *fm=(lv>0)?&fog_map[lv*256]:0;
                int k;
                // The tex/fog/flat branches are taken OUT of the pixel loop:
                // inside only the z-test + sampling remain (no conditionals).
                // Unrolled x2: half the comparisons/control jumps of the
                // per-pixel loop (the body is short, so the overhead mattered).
                #define M9_PIX(SAMPLE) \
                    if(zfix>*zp){ *p=(SAMPLE); *zp=(unsigned short)zfix; } \
                    p++; zp++; tu+=stu; tv+=stv; zfix+=zstep;
                #define M9_SPAN(SAMPLE) \
                    { int _h=seg>>1; \
                      while(_h--){ M9_PIX(SAMPLE) M9_PIX(SAMPLE) } \
                      if(seg&1){ M9_PIX(SAMPLE) } }
                if(tex){
                    if(pow2){
                        // No clamp: AND with mask (power-of-2 texture). The
                        // row offset uses a SHIFT (twsh=log2(tw)) instead of an imul
                        // per pixel: ~10-40 fewer cycles per pixel on a 486 (bit-exact).
                        if(fm){ M9_SPAN(fm[(unsigned char)tex[(((tv>>16)&thm)<<twsh)+((tu>>16)&twm)]]) }
                        else  { M9_SPAN(tex[(((tv>>16)&thm)<<twsh)+((tu>>16)&twm)]) }
                    } else {
                        // Fallback with clamp (non power-of-2 texture).
                        for(k=0;k<seg;k++){
                            if(zfix>*zp){
                                int tx=tu>>16, ty=tv>>16;
                                char px;
                                if(tx<0)tx=0;else if(tx>=tw)tx=tw-1;
                                if(ty<0)ty=0;else if(ty>=th)ty=th-1;
                                px=tex[ty*tw+tx];
                                if(fm)px=fm[(unsigned char)px];
                                *p=px; *zp=(unsigned short)zfix;
                            }
                            p++; zp++; tu+=stu; tv+=stv; zfix+=zstep;
                        }
                    }
                } else {
                    // Flat: the color (with or without fog) is constant across the segment.
                    unsigned char pxf=fm?(unsigned char)fm[flatcol]:flatcol;
                    M9_SPAN((char)pxf)
                }
                #undef M9_SPAN
                #undef M9_PIX
                cuw=cuw_e; cvw=cvw_e; ciw=ciw_e;
            }
            xi+=seg;
        }
    }
}

// Evaluates an edge at the CENTER of scanline cy: x position + attributes.
static void edge_at(float cy,float ya,float xa,float uwa,float vwa,float iwa,
                    float gx,float gu,float gv,float gi,
                    float *ox,float *ou,float *ov,float *oi){
    float s=cy-ya; *ox=xa+gx*s; *ou=uwa+gu*s; *ov=vwa+gv*s; *oi=iwa+gi*s;
}

// Rasterizes a triangle with Z-buffer, SUBPIXEL coordinates (vertices in float).
// Sampling is at pixel centers (x+0.5, y+0.5) with prestep, so that
// vertices are NOT quantized to integers -> no texture shimmer/swimming when
// moving the geometry (the "PS1" artifact). tex=NULL -> flat color 'flatcol'.
static void raster_tri_z(char *vid,unsigned short *zb,int W,int H,
    float x0,float y0,float u0,float v0,float w0,
    float x1,float y1,float u1,float v1,float w1,
    float x2,float y2,float u2,float v2,float w2,
    char *tex,int tw,int th,unsigned char flatcol){
    int pow2,y,yA,yB,twsh=0;float tf;
    float iw0,iw1,iw2,uw0,vw0,uw1,vw1,uw2,vw2;

    // w can't drop below NEAR_PLANE: guarantees 1/w*ZSCALE <= 65535 (16-bit z).
    if(w0<NEAR_PLANE)w0=NEAR_PLANE; if(w1<NEAR_PLANE)w1=NEAR_PLANE; if(w2<NEAR_PLANE)w2=NEAR_PLANE;
    cur_seg = m9_texseg;
    pow2=(tex && (tw&(tw-1))==0 && (th&(th-1))==0);
    if(pow2){int t2=tw; while(t2>1){t2>>=1;twsh++;}}   // log2(tw) for the per-pixel shift
    // 1/w of the 3 vertices: fast reciprocal (bit-hack) instead of FPU division.
    // w>=NEAR_PLANE (clamped above), error ~0.05% (already tolerated throughout the engine).
    iw0=fast_recip(w0); iw1=fast_recip(w1); iw2=fast_recip(w2);
    uw0=u0*iw0; vw0=v0*iw0;
    uw1=u1*iw1; vw1=v1*iw1;
    uw2=u2*iw2; vw2=v2*iw2;

    // Sort the 3 vertices by Y (float), dragging x and the u/w,v/w,1/w attributes.
    #define SW(a,b) tf=(a);(a)=(b);(b)=tf;
    #define S2(ya,xa,ua,va,ia, yb,xb,ub,vb,ib) \
        if(ya>yb){SW(ya,yb) SW(xa,xb) SW(ua,ub) SW(va,vb) SW(ia,ib)}
    S2(y0,x0,uw0,vw0,iw0, y1,x1,uw1,vw1,iw1)
    S2(y1,x1,uw1,vw1,iw1, y2,x2,uw2,vw2,iw2)
    S2(y0,x0,uw0,vw0,iw0, y1,x1,uw1,vw1,iw1)
    #undef S2
    #undef SW

    if(y2-y0<1e-4f)return;

    {
    // Slopes per unit of Y (not per scanline): the edge is evaluated at the
    // CENTER of each scanline (cy=y+0.5). 1 reciprocal per edge is computed and
    // multiplied (4 mul) instead of 4 FPU divisions (~73 cycles each on a 486).
    float ril=1.0f/(y2-y0);
    float gxl=(x2-x0)*ril, gul=(uw2-uw0)*ril;   // long edge 0->2
    float gvl=(vw2-vw0)*ril, gil=(iw2-iw0)*ril;

    if(y1-y0>1e-4f){   // top half: short edge 0->1
        float ris=1.0f/(y1-y0);
        float gxs=(x1-x0)*ris, gus=(uw1-uw0)*ris;
        float gvs=(vw1-vw0)*ris, gis=(iw1-iw0)*ris;
        yA=iceil(y0-0.5f); yB=iceil(y1-0.5f);
        if(yA<clip_y0)yA=clip_y0; if(yB>clip_y1)yB=clip_y1;
        for(y=yA;y<yB;y++){
            float cy=(float)y+0.5f, xl,ul,vl,il, xs,us,vs,is;
            edge_at(cy,y0,x0,uw0,vw0,iw0,gxl,gul,gvl,gil,&xl,&ul,&vl,&il);
            edge_at(cy,y0,x0,uw0,vw0,iw0,gxs,gus,gvs,gis,&xs,&us,&vs,&is);
            hspan_z(vid,zb,W,H,y,xl,ul,vl,il,xs,us,vs,is,tex,tw,th,pow2,twsh,flatcol);
        }
    }
    if(y2-y1>1e-4f){   // bottom half: short edge 1->2 (the long one continues)
        float ris=1.0f/(y2-y1);
        float gxs=(x2-x1)*ris, gus=(uw2-uw1)*ris;
        float gvs=(vw2-vw1)*ris, gis=(iw2-iw1)*ris;
        yA=iceil(y1-0.5f); yB=iceil(y2-0.5f);
        if(yA<clip_y0)yA=clip_y0; if(yB>clip_y1)yB=clip_y1;
        for(y=yA;y<yB;y++){
            float cy=(float)y+0.5f, xl,ul,vl,il, xs,us,vs,is;
            edge_at(cy,y0,x0,uw0,vw0,iw0,gxl,gul,gvl,gil,&xl,&ul,&vl,&il);
            edge_at(cy,y1,x1,uw1,vw1,iw1,gxs,gus,gvs,gis,&xs,&us,&vs,&is);
            hspan_z(vid,zb,W,H,y,xl,ul,vl,il,xs,us,vs,is,tex,tw,th,pow2,twsh,flatcol);
        }
    }
    }
}

static unsigned char flat_color(int a,int b,int c);   // (defined below)
static int load_fpg_file(char *fname);                // (defined below)

// ============================================================================
// Spatial grid: range of cells (XZ) overlapped by a triangle's footprint.
// ============================================================================
static void tri_cellrange(int t,int *x0,int *x1,int *z0,int *z1){
    int a=mesh_t[t].a,b=mesh_t[t].b,c=mesh_t[t].c;
    float mnx=mesh_v[a].x,mxx=mnx,mnz=mesh_v[a].z,mxz=mnz,v;
    v=mesh_v[b].x; if(v<mnx)mnx=v; if(v>mxx)mxx=v;
    v=mesh_v[c].x; if(v<mnx)mnx=v; if(v>mxx)mxx=v;
    v=mesh_v[b].z; if(v<mnz)mnz=v; if(v>mxz)mxz=v;
    v=mesh_v[c].z; if(v<mnz)mnz=v; if(v>mxz)mxz=v;
    *x0=(int)((mnx-grid_minx)*grid_invcw); *x1=(int)((mxx-grid_minx)*grid_invcw);
    *z0=(int)((mnz-grid_minz)*grid_invch); *z1=(int)((mxz-grid_minz)*grid_invch);
    if(*x0<0)*x0=0; if(*x1>=GRID_DIM)*x1=GRID_DIM-1; if(*x1<*x0)*x1=*x0;
    if(*z0<0)*z0=0; if(*z1>=GRID_DIM)*z1=GRID_DIM-1; if(*z1<*z0)*z1=*z0;
}

// Builds the spatial grid of the current mesh. Returns 1 if OK, 0 if it
// couldn't (then grid_n=0 and the render scans all triangles: fallback).
static int build_grid(void){
    int i,gx,gz,t,total=0;
    float minx,minz,maxx,maxz;
    static int cur[GRID_CELLS];
    grid_n=0;
#if !M9_USE_GRID
    return 0;                            // grid disabled (toggle): scan everything
#endif
    if(n_tris<=0||n_verts<=0) return 0;
    minx=minz=1e30f; maxx=maxz=-1e30f;
    for(i=0;i<n_verts;i++){
        float x=mesh_v[i].x, z=mesh_v[i].z;
        if(x<minx)minx=x; if(x>maxx)maxx=x;
        if(z<minz)minz=z; if(z>maxz)maxz=z;
    }
    if(maxx<=minx)maxx=minx+1.0f; if(maxz<=minz)maxz=minz+1.0f;
    grid_minx=minx; grid_minz=minz;
    grid_cw=(maxx-minx)/(float)GRID_DIM; grid_ch=(maxz-minz)/(float)GRID_DIM;
    // 0.999999 so the max-edge vertex falls into cell GRID_DIM-1.
    grid_invcw=(float)GRID_DIM/(maxx-minx)*0.999999f;
    grid_invch=(float)GRID_DIM/(maxz-minz)*0.999999f;
    grid_cellrad=0.5f*(float)sqrt((double)(grid_cw*grid_cw+grid_ch*grid_ch));
    // 1st pass: count triangles per cell (in start[c+1]).
    for(i=0;i<=GRID_CELLS;i++) grid_start[i]=0;
    for(t=0;t<n_tris;t++){
        int x0,x1,z0,z1; tri_cellrange(t,&x0,&x1,&z0,&z1);
        for(gz=z0;gz<=z1;gz++) for(gx=x0;gx<=x1;gx++){ grid_start[gz*GRID_DIM+gx+1]++; total++; }
    }
    // Reserve (div_malloc) grid_items at the exact size and tstamp at n_tris. If
    // the allocation fails -> grid_n=0 and the render scans all triangles.
    if(total>grid_cap){
        if(grid_items)div_free(grid_items);
        grid_items=(int*)div_malloc((size_t)total*sizeof(int));
        if(!grid_items){grid_cap=0;return 0;}
        grid_cap=total;
    }
    if(n_tris>tstamp_cap){
        if(tstamp)div_free(tstamp);
        tstamp=(int*)div_malloc((size_t)n_tris*sizeof(int));
        if(!tstamp){tstamp_cap=0;return 0;}
        tstamp_cap=n_tris;
    }
    {int k; for(k=0;k<n_tris;k++) tstamp[k]=0;}   // no stale stamps
    for(i=1;i<=GRID_CELLS;i++) grid_start[i]+=grid_start[i-1];
    // 2nd pass: dump triangle indices into place.
    for(i=0;i<GRID_CELLS;i++) cur[i]=grid_start[i];
    for(t=0;t<n_tris;t++){
        int x0,x1,z0,z1; tri_cellrange(t,&x0,&x1,&z0,&z1);
        for(gz=z0;gz<=z1;gz++) for(gx=x0;gx<=x1;gx++){ int c=gz*GRID_DIM+gx; grid_items[cur[c]++]=t; }
    }
    grid_n=1;
    return 1;
}

// ============================================================================
// Wall spatial grid: range of cells (XZ) overlapped by a wall segment.
// Uses the SAME cell geometry as the render grid (already computed).
// ============================================================================
static void wall_cellrange(int w,int *x0,int *x1,int *z0,int *z1){
    float ax=mesh_walls[w].x0,bx=mesh_walls[w].x1;
    float az=mesh_walls[w].z0,bz=mesh_walls[w].z1;
    float mnx=ax<bx?ax:bx, mxx=ax>bx?ax:bx;
    float mnz=az<bz?az:bz, mxz=az>bz?az:bz;
    *x0=(int)((mnx-grid_minx)*grid_invcw); *x1=(int)((mxx-grid_minx)*grid_invcw);
    *z0=(int)((mnz-grid_minz)*grid_invch); *z1=(int)((mxz-grid_minz)*grid_invch);
    if(*x0<0)*x0=0; if(*x1>=GRID_DIM)*x1=GRID_DIM-1; if(*x1<*x0)*x1=*x0;
    if(*z0<0)*z0=0; if(*z1>=GRID_DIM)*z1=GRID_DIM-1; if(*z1<*z0)*z1=*z0;
}

// Builds the wall grid. Requires the render grid to be already built
// (grid_n) to reuse its cell geometry. Returns 1 if OK, 0 if not
// (then wgrid_n=0 and collision scans all walls: safe fallback).
static int build_wall_grid(void){
    int w,gx,gz,total=0;
    static int wcur[GRID_CELLS];
    wgrid_n=0;
#if !M9_COLLIDE_GRID
    return 0;
#endif
    if(n_walls<=0 || !grid_n) return 0;
    for(w=0;w<=GRID_CELLS;w++) wgrid_start[w]=0;
    for(w=0;w<n_walls;w++){
        int x0,x1,z0,z1; wall_cellrange(w,&x0,&x1,&z0,&z1);
        for(gz=z0;gz<=z1;gz++) for(gx=x0;gx<=x1;gx++){ wgrid_start[gz*GRID_DIM+gx+1]++; total++; }
    }
    if(total>wgrid_cap){
        if(wgrid_items)div_free(wgrid_items);
        wgrid_items=(int*)div_malloc((size_t)total*sizeof(int));
        if(!wgrid_items){wgrid_cap=0;return 0;}
        wgrid_cap=total;
    }
    for(w=1;w<=GRID_CELLS;w++) wgrid_start[w]+=wgrid_start[w-1];
    for(w=0;w<GRID_CELLS;w++) wcur[w]=wgrid_start[w];
    for(w=0;w<n_walls;w++){
        int x0,x1,z0,z1; wall_cellrange(w,&x0,&x1,&z0,&z1);
        for(gz=z0;gz<=z1;gz++) for(gx=x0;gx<=x1;gx++){ int c=gz*GRID_DIM+gx; wgrid_items[wcur[c]++]=w; }
    }
    {int i; for(i=0;i<MAX_WALLS;i++) wstamp[i]=0;}   // no stale stamps
    wstamp_frame=0; wgrid_n=1;
    return 1;
}

// Fills wcand[] with the walls whose footprint overlaps the given XZ rectangle
// (deduplicated). If there's no wall grid, returns ALL (fallback).
static void gather_walls(float minx,float minz,float maxx,float maxz){
    n_wcand=0;
#if M9_COLLIDE_GRID
    if(wgrid_n){
        int cx0=(int)((minx-grid_minx)*grid_invcw), cx1=(int)((maxx-grid_minx)*grid_invcw);
        int cz0=(int)((minz-grid_minz)*grid_invch), cz1=(int)((maxz-grid_minz)*grid_invch);
        int gx,gz;
        // +1 ring cell: covers a push crossing into the neighbor cell (the 2
        // passes can move the player a bit beyond the metric box).
        cx0--; cx1++; cz0--; cz1++;
        if(cx0<0)cx0=0; if(cx1>=GRID_DIM)cx1=GRID_DIM-1;
        if(cz0<0)cz0=0; if(cz1>=GRID_DIM)cz1=GRID_DIM-1;
        if(cx1<cx0||cz1<cz0) return;   // box outside the grid -> no walls nearby
        wstamp_frame++;
        for(gz=cz0;gz<=cz1;gz++) for(gx=cx0;gx<=cx1;gx++){
            int c=gz*GRID_DIM+gx, s=wgrid_start[c], e=wgrid_start[c+1], k;
            for(k=s;k<e;k++){
                int w=wgrid_items[k];
                if(wstamp[w]==wstamp_frame) continue;   // already added (another cell)
                wstamp[w]=wstamp_frame;
                wcand[n_wcand++]=w;
            }
        }
        return;
    }
#endif
    {int w; for(w=0;w<n_walls;w++) wcand[w]=w; n_wcand=n_walls;}
}

// ============================================================================
// load_obj_file: loads an OBJ mesh (with optional UVs). Returns num tris.
// ============================================================================
static int load_obj_file(char *fname){
    char line[256],*p;
    char mat_names[MAX_MATS][32];   // material names by order of appearance
    int n_mat_names=0, cur_mat=0;
    FILE *f;
    int vi[16],uvi[16],nv,i;

    n_verts=0;n_uvs=0;n_tris=0;mesh_ok=0;has_uvs=0;n_mats=1;n_floors=0;n_ceils=0;

    f=div_fopen(fname,"r");
    if(!f){
        s_cpy(m9_msg,"M9 NO FILE: ");s_cat(m9_msg,fname);
        return 0;
    }

    while(read_line(f,line,sizeof(line))){
        if(line[0]=='v'&&line[1]==' '){
            if(n_verts<MAX_VERTS){
                p=line+2;
                mesh_v[n_verts].x=sc_atof(p);p=next_tok(p);
                mesh_v[n_verts].y=sc_atof(p);p=next_tok(p);
                mesh_v[n_verts].z=sc_atof(p);
                n_verts++;
            }
        } else if(line[0]=='v'&&line[1]=='t'&&line[2]==' '){
            if(n_uvs<MAX_UVS){
                p=line+3;
                mesh_uv[n_uvs].u=sc_atof(p); p=next_tok(p);
                mesh_uv[n_uvs].v=1.0f-sc_atof(p);   // OBJ V=0 bottom, MAP row 0 top
                n_uvs++;
            }
        } else if(line[0]=='u'&&line[1]=='s'&&line[2]=='e'&&line[3]=='m'&&
                  line[4]=='t'&&line[5]=='l'&&(line[6]==' '||line[6]=='\t')){
            // usemtl <name>: register the name and get its index (0-based)
            char *mn=line+7; int j,k,found=-1,mlen=0;
            while(*mn==' '||*mn=='\t')mn++;
            while(mn[mlen]&&mn[mlen]!=' '&&mn[mlen]!='\t'&&mlen<31)mlen++;
            for(j=0;j<n_mat_names;j++){
                int eq=1;
                for(k=0;k<mlen;k++){if(mat_names[j][k]!=mn[k]){eq=0;break;}}
                if(eq&&mat_names[j][mlen]==0){found=j;break;}
            }
            if(found<0&&n_mat_names<MAX_MATS){
                for(k=0;k<mlen;k++)mat_names[n_mat_names][k]=mn[k];
                mat_names[n_mat_names][mlen]=0;
                found=n_mat_names++;
            }
            if(found>=0)cur_mat=found;
        } else if(line[0]=='f'&&line[1]==' '){
            p=line+2;nv=0;
            while(*p&&nv<16){
                while(*p==' '||*p=='\t')p++;
                if(!*p||*p=='\n')break;
                p=parse_face_vert(p,&vi[nv],&uvi[nv]);
                nv++;
            }
            for(i=1;i<nv-1&&n_tris<MAX_TRIS;i++){
                if(vi[0]>=0&&vi[0]<n_verts&&vi[i]>=0&&vi[i]<n_verts&&vi[i+1]>=0&&vi[i+1]<n_verts){
                    mesh_t[n_tris].a=vi[0]; mesh_t[n_tris].b=vi[i]; mesh_t[n_tris].c=vi[i+1];
                    mesh_t[n_tris].ua=(uvi[0]  >=0&&uvi[0]  <n_uvs)?uvi[0]  :-1;
                    mesh_t[n_tris].ub=(uvi[i]  >=0&&uvi[i]  <n_uvs)?uvi[i]  :-1;
                    mesh_t[n_tris].uc=(uvi[i+1]>=0&&uvi[i+1]<n_uvs)?uvi[i+1]:-1;
                    mesh_t[n_tris].mat=(unsigned char)cur_mat;
                    n_tris++;
                }
            }
        }
    }
    div_fclose(f);
    n_mats = (n_mat_names>0)?n_mat_names:1;

    has_uvs=(n_uvs>0);
    mesh_ok=(n_tris>0);
    // Precompute each triangle's flat-shading color ONCE (the normal
    // and the light don't change): avoids a sqrt + normalization per triangle and frame.
    for(i=0;i<n_tris;i++)
        mesh_t[i].flat=flat_color(mesh_t[i].a,mesh_t[i].b,mesh_t[i].c);

    // Precompute the 2D "footprint" (XZ plane) of the walls for collision:
    // a triangle whose normal is nearly horizontal (|ny|<WALL_NY) is a WALL;
    // its XZ projection is a segment (the pair of farthest-apart vertices).
    // Its vertical extent [ymin,ymax] is also stored to not collide with
    // roofs or lintels above the player (e.g. the gate).
    n_walls=0; n_floors=0; n_ceils=0;
    for(i=0;i<n_tris;i++){
        int a=mesh_t[i].a,b=mesh_t[i].b,c=mesh_t[i].c;
        float ux=mesh_v[b].x-mesh_v[a].x, uy=mesh_v[b].y-mesh_v[a].y, uz=mesh_v[b].z-mesh_v[a].z;
        float vx=mesh_v[c].x-mesh_v[a].x, vy=mesh_v[c].y-mesh_v[a].y, vz=mesh_v[c].z-mesh_v[a].z;
        float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        float nl=(float)sqrt((double)(nx*nx+ny*ny+nz*nz));
        if(nl<1e-6f)continue;
        if((ny<0?-ny:ny)/nl >= WALL_NY){
            // Nearly horizontal -> FLOOR or CEILING, split by normal direction
            // (Y-up). UPWARD faces (ny>0) are floors you stand on -> floor-follow.
            // DOWNWARD faces (ny<0) are ceilings/roofs -> head collision: they
            // cap how high you can jump so you can't pass THROUGH them. A ceiling
            // must NOT be a floor (or jumping would snap your feet onto it and
            // trap you on top). Neither is a wall, so they skip mesh_walls.
            if(ny>0.0f){ if(n_floors<MAX_FLOORS) mesh_floor_idx[n_floors++]=i; }
            else       { if(n_ceils <MAX_FLOORS) mesh_ceil_idx [n_ceils ++]=i; }
            continue;
        }
        if(n_walls>=MAX_WALLS) continue;            // wall (nearly vertical)
        {
            // pair of farthest-apart vertices in XZ -> segment ends
            float px[3],pz[3],py[3]; int vidx[3],j,e0=0,e1=1; float best=-1.0f;
            px[0]=mesh_v[a].x;pz[0]=mesh_v[a].z;py[0]=mesh_v[a].y;vidx[0]=a;
            px[1]=mesh_v[b].x;pz[1]=mesh_v[b].z;py[1]=mesh_v[b].y;vidx[1]=b;
            px[2]=mesh_v[c].x;pz[2]=mesh_v[c].z;py[2]=mesh_v[c].y;vidx[2]=c;
            for(j=0;j<3;j++){
                int k=(j+1)%3;
                float dxx=px[j]-px[k], dzz=pz[j]-pz[k], d=dxx*dxx+dzz*dzz;
                if(d>best){best=d;e0=j;e1=k;}
            }
            mesh_walls[n_walls].x0=px[e0]; mesh_walls[n_walls].z0=pz[e0];
            mesh_walls[n_walls].x1=px[e1]; mesh_walls[n_walls].z1=pz[e1];
            mesh_walls[n_walls].vi0=vidx[e0]; mesh_walls[n_walls].vi1=vidx[e1];
            mesh_walls[n_walls].cap0=1; mesh_walls[n_walls].cap1=1;   // by default: free end
            {float y0=py[0],y1=py[0],t; t=py[1];if(t<y0)y0=t;if(t>y1)y1=t;
             t=py[2];if(t<y0)y0=t;if(t>y1)y1=t;
             mesh_walls[n_walls].ymin=y0; mesh_walls[n_walls].ymax=y1;}
            n_walls++;
        }
    }
    // End caps: an end that coincides (in XZ) with another wall's is
    // INTERIOR (joint between segments of the same wall, corner, door jamb...).
    // Removing its radial "cap" avoids snagging when sliding; the adjacent face already
    // stops you. Only the TRULY isolated ends keep their cap.
    // IMPORTANT: grouped by XZ POSITION and not by vertex index, because the
    // same physical point (e.g. top/bottom of a vertical edge, or the wall and
    // the lintel of a door) usually has several different vertex indices. That
    // was the cause of the false blocks at the ends of the rampart gate.
#if M9_CORNER_SLIDE
    if(n_walls>0){
        int wv,k; float eps2=0.02f*0.02f;   // ~2 DIV2 units (unitscale 100): XZ coincidence
        // 1) group coincident vertices in XZ -> representative (the earliest)
        for(wv=0;wv<n_verts;wv++){
            vgroup[wv]=wv;
            for(k=0;k<wv;k++){
                float dxx=mesh_v[wv].x-mesh_v[k].x, dzz=mesh_v[wv].z-mesh_v[k].z;
                if(dxx*dxx+dzz*dzz<=eps2){ vgroup[wv]=vgroup[k]; break; }
            }
        }
        // 2) count wall ends per position GROUP
        for(wv=0;wv<n_verts;wv++) wall_vc[wv]=0;
        for(wv=0;wv<n_walls;wv++){
            wall_vc[vgroup[mesh_walls[wv].vi0]]++;
            wall_vc[vgroup[mesh_walls[wv].vi1]]++;
        }
        // 3) end in a group with >=2 walls -> interior -> no radial cap
        for(wv=0;wv<n_walls;wv++){
            if(wall_vc[vgroup[mesh_walls[wv].vi0]]>1) mesh_walls[wv].cap0=0;
            if(wall_vc[vgroup[mesh_walls[wv].vi1]]>1) mesh_walls[wv].cap1=0;
        }
    }
#endif
    // XZ spatial grid for per-cell triangle culling (Tier 3).
    build_grid();
    build_wall_grid();   // wall grid to speed up collision
    {char num[12];
     s_cpy(m9_msg,"M9 ");s_itos(n_tris,num);s_cat(m9_msg,num);
     s_cat(m9_msg,has_uvs?" tri+UV":" tri");}
    return mesh_ok?n_tris:0;
}

// ============================================================================
// build_mip_for: generates the 1-level mipmap (half width/height) for
// material m. Point sampling (palette indices: not averaged). Improves
// cache coherence when texturing distant surfaces.
// ============================================================================
static void build_mip_for(int m){
    int mw,mh,x,y;
    if(mat_mip[m]){div_free(mat_mip[m]);mat_mip[m]=NULL;matm_w[m]=matm_h[m]=0;}
#if !M9_USE_MIP
    return;
#endif
    if(!mat_data[m]||mat_w[m]<2||mat_h[m]<2) return;
    mw=mat_w[m]>>1; mh=mat_h[m]>>1;
    mat_mip[m]=(char*)div_malloc((size_t)(mw*mh));
    if(!mat_mip[m]) return;
    for(y=0;y<mh;y++){
        char *d=mat_mip[m]+(long)y*mw;
        char *s=mat_data[m]+(long)(y<<1)*mat_w[m];
        for(x=0;x<mw;x++) d[x]=s[x<<1];
    }
    matm_w[m]=mw; matm_h[m]=mh;
}

// ============================================================================
// load_material_file: loads a .MAP texture into material slot m (0..MAX_MATS-1).
// Frees that slot's previous buffer before loading (no leaks). Returns 1/0.
// ============================================================================
static int load_material_file(int m, char *fname){
    char magic[8];
    FILE *f;
    int w, h, npoints, pix_size;

    if(m<0||m>=MAX_MATS) return 0;
    if(mat_data[m]){div_free(mat_data[m]);mat_data[m]=NULL;mat_w[m]=mat_h[m]=0;}
    if(mat_mip[m]){div_free(mat_mip[m]);mat_mip[m]=NULL;matm_w[m]=matm_h[m]=0;}

    f=div_fopen(fname,"rb");
    if(!f){s_cpy(m9_msg,"TEX NO FILE");return 0;}

    if((int)fread(magic,1,8,f)!=8||magic[0]!='m'||magic[1]!='a'||magic[2]!='p'){
        s_cpy(m9_msg,"TEX no MAP");div_fclose(f);return 0;
    }
    {
        unsigned short sw=0, sh=0, snp=0;
        if((int)fread(&sw,2,1,f)!=1||(int)fread(&sh,2,1,f)!=1){
            s_cpy(m9_msg,"TEX dim err");div_fclose(f);return 0;
        }
        fseek(f,1380L,SEEK_CUR);     // code(4)+name(32)+palette(768)+ranges(576)
        if((int)fread(&snp,2,1,f)!=1){
            s_cpy(m9_msg,"TEX npts err");div_fclose(f);return 0;
        }
        w=(int)sw; h=(int)sh; npoints=(int)snp;
    }
    if(w<=0||h<=0||w>1024||h>1024){
        s_cpy(m9_msg,"TEX size err");div_fclose(f);return 0;
    }
    if(npoints>0)fseek(f,(long)npoints*4L,SEEK_CUR);

    pix_size=w*h;
    mat_data[m]=(char*)div_malloc((size_t)pix_size);
    if(!mat_data[m]){s_cpy(m9_msg,"TEX no mem");div_fclose(f);return 0;}
    if((int)fread(mat_data[m],1,(size_t)pix_size,f)!=pix_size){
        s_cpy(m9_msg,"TEX read err");div_free(mat_data[m]);mat_data[m]=NULL;
        div_fclose(f);return 0;
    }
    mat_w[m]=w; mat_h[m]=h;
    div_fclose(f);
    build_mip_for(m);                    // 1-level mipmap for LOD/cache
    {char num[12];s_cpy(m9_msg,"TEX ");s_itos(w,num);s_cat(m9_msg,num);
     s_cat(m9_msg,"x");s_itos(h,num);s_cat(m9_msg,num);}
    return 1;
}

// ============================================================================
// LOAD_WLD9(obj, texture, unitscale): loads the scenario in ONE single call
// (analogous to mode 8's load_wld but with texture and scale integrated).
//   obj       : .OBJ file of the scenario (Y-up, triangulated, with UVs).
//   texture   : .MAP texture file ("" or "0" = no texture -> flat).
//   unitscale : DIV2 units per OBJ unit (<=0 = unchanged, def 100).
// Returns num triangles loaded (0 on failure).
// ============================================================================
static void load_wld9_fn(void){
    int scale  =getparm();          // 3rd arg: unitscale
    int tex_off=getparm();          // 2nd arg: texture file
    int obj_off=getparm();          // 1st arg: OBJ file
    char fname[64], tname[64];
    int ntris, m;

    if(scale>0) m9_unitscale=(float)scale;

    div2c(obj_off,fname,sizeof(fname));
    // Free ALL the previous level's materials before loading the new one.
    for(m=0;m<MAX_MATS;m++){
        if(mat_data[m]){div_free(mat_data[m]);mat_data[m]=NULL;mat_w[m]=mat_h[m]=0;}
        if(mat_mip[m]){div_free(mat_mip[m]);mat_mip[m]=NULL;matm_w[m]=matm_h[m]=0;}
    }
    ntris=load_obj_file(fname);

    div2c(tex_off,tname,sizeof(tname));
    if(tname[0] && !(tname[0]=='0'&&tname[1]==0)){
        load_material_file(0,tname);  // default texture in slot 0
    }
    retval(ntris);
}

// MODE9_SETTEX(map): changes the texture of material 0 (default).
// Equivalent to MODE9_MATERIAL(0,"MAP/..."). Returns 1/0.
static void mode9_settex_fn(void){
    int str_off=getparm();
    char fname[64];
    div2c(str_off,fname,sizeof(fname));
    retval(load_material_file(0,fname));
}

// MODE9_MATERIAL(idx, map): assigns a .MAP texture to material idx (0..15).
// idx 0 = default material (the one loaded in LOAD_WLD9).
// Call after LOAD_WLD9, once per material you want to customize.
// If idx already had a texture, it's freed first (no leaks). Returns 1/0.
static void mode9_material_fn(void){
    int str_off=getparm();
    int idx    =getparm();
    char fname[64];
    div2c(str_off,fname,sizeof(fname));
    retval(load_material_file(idx,fname));
}

// ============================================================================
// load_sky_file: loads a panoramic .MAP texture as sky. Same format as
// textures (indices against the active DIV2 palette). Returns 1/0. The texture
// must wrap 360deg horizontally (its left and right edges must match).
// ============================================================================
static int load_sky_file(char *fname){
    char magic[8]; FILE *f; int w,h,npoints,pix_size;
    if(sky_data){div_free(sky_data);sky_data=NULL;sky_w=sky_h=0;}
    sky_on=0;
    f=div_fopen(fname,"rb");
    if(!f){s_cpy(m9_msg,"SKY NO FILE");return 0;}
    if((int)fread(magic,1,8,f)!=8||magic[0]!='m'||magic[1]!='a'||magic[2]!='p'){
        s_cpy(m9_msg,"SKY no MAP");div_fclose(f);return 0;
    }
    {unsigned short sw=0,sh=0,snp=0;
     if((int)fread(&sw,2,1,f)!=1||(int)fread(&sh,2,1,f)!=1){s_cpy(m9_msg,"SKY dim err");div_fclose(f);return 0;}
     fseek(f,1380L,SEEK_CUR);     // code(4)+name(32)+palette(768)+ranges(576)
     if((int)fread(&snp,2,1,f)!=1){s_cpy(m9_msg,"SKY npts err");div_fclose(f);return 0;}
     w=(int)sw;h=(int)sh;npoints=(int)snp;}
    if(w<=0||h<=0||w>SKY_MAXDIM*2||h>SKY_MAXDIM){s_cpy(m9_msg,"SKY size err");div_fclose(f);return 0;}
    if(npoints>0)fseek(f,(long)npoints*4L,SEEK_CUR);
    pix_size=w*h;
    sky_data=(char*)div_malloc((size_t)pix_size);
    if(!sky_data){s_cpy(m9_msg,"SKY no mem");div_fclose(f);return 0;}
    if((int)fread(sky_data,1,(size_t)pix_size,f)!=pix_size){
        s_cpy(m9_msg,"SKY read err");div_free(sky_data);sky_data=NULL;div_fclose(f);return 0;
    }
    sky_w=w;sky_h=h;sky_on=1;sky_cd_vw=0;   // invalidate column cache
    div_fclose(f);
    {char num[12];s_cpy(m9_msg,"SKY ");s_itos(w,num);s_cat(m9_msg,num);
     s_cat(m9_msg,"x");s_itos(h,num);s_cat(m9_msg,num);}
    return 1;
}

// LOAD_SKY9(map): loads a panoramic .MAP sky. Replaces the flat background color
// with the texture, oriented by the yaw (turn) and pitch (look up/down).
static void load_sky9_fn(void){
    int off=getparm(); char fname[64];
    div2c(off,fname,sizeof(fname));
    retval(load_sky_file(fname));
}
// SKY9_OFF(): back to the flat color background (SET_ENV_COLOR9).
static void sky9_off_fn(void){ sky_on=0; retval(0); }

// MODE9_LOADFPG(file): loads an FPG in the DLL to draw the billboards
// (c_m9 processes) itself in EACH viewport (needed for split-screen).
// Without this, billboards use DIV2's c_screen path (single view).
// Returns num loaded graphics. The PRG still does its own load_fpg separately.
static void mode9_loadfpg_fn(void){
    int str_off=getparm();
    char fname[64];
    div2c(str_off,fname,sizeof(fname));
    retval(load_fpg_file(fname));
}

// ============================================================================
// START_MODE9 / STOP_MODE9 / MODE9_SET / MODE9_UNITSCALE
// ============================================================================
// START_MODE9(camera, m9, region): same signature as start_mode8.
//   camera : id of the camera process (like start_mode8).
//   m9     : the m9 STRUCT to use. NOTE: in mode 8 the m8 INDEX goes here (the
//            engine knows where m8 lives); here, since m9 is a PRG struct, its
//            ADDRESS is passed: &m9 (or &m9[n] if you use a camera array). The
//            DLL reads m9.camera/height/angle from there each frame (integrates the BIND).
//   region : screen region, as in start_mode8.
//            0 = FULL SCREEN: single view, disables the rest (classic).
//            n (1..15) = the region defined with MODE9_REGION(n,x,y,w,h): creates/
//            updates a view in that rectangle WITHOUT touching the other regions,
//            so you can call START_MODE9 several times (one per region)
//            to set up a split-screen in the "several mode 8 windows" style.
//            If region n isn't defined yet, it falls back to full screen.
static void start_mode9_fn(void){
    int region=getparm();    // 3rd arg
    int m9off =getparm();    // 2nd arg: &m9
    int cam   =getparm();    // 1st arg: camera id
    int i,slot;
    m9_camera=cam;
    if(m9off) m9_bind=m9off; // binds the struct (no need for a separate MODE9_BIND)
    m9_region=region;

    if(region<=0 || region>=MAX_REGIONS || !reg_def[region]){
        // --- Full screen: single view (view 0), disable the rest -----
        views[0].active=1; views[0].cam=cam; views[0].bind=m9off; views[0].region=0;
        views[0].vx=0; views[0].vy=0; views[0].vw=0; views[0].vh=0; views[0].haslast=0;
        for(i=1;i<MAX_VIEWS;i++) views[i].active=0;
        m9_active=1;
        retval(1);
        return;
    }

    // --- Region with rectangle: reuse the slot that already served that region, or the
    //     first free slot. The other views aren't touched (split by regions). --
    slot=-1;
    for(i=0;i<MAX_VIEWS;i++) if(views[i].active && views[i].region==region){slot=i;break;}
    if(slot<0) for(i=0;i<MAX_VIEWS;i++) if(!views[i].active){slot=i;break;}
    if(slot<0){ retval(0); return; }   // no slots: no room for more regions
    views[slot].active=1; views[slot].cam=cam; views[slot].bind=m9off; views[slot].region=region;
    views[slot].vx=reg_x[region]; views[slot].vy=reg_y[region];
    views[slot].vw=reg_w[region]; views[slot].vh=reg_h[region];
    views[slot].haslast=0;
    // Slot 0 rules the c_screen billboard path (DIV2's single view).
    if(slot==0){ m9_camera=cam; if(m9off)m9_bind=m9off; }
    m9_active=1;
    retval(1);
}
// MODE9_SPLIT(slot, camera, m9, x, y, w, h): defines a viewport (split-screen).
// slot 0..MAX_VIEWS-1. Each view has its own camera and its own m9 STRUCT
// (&m9[n].z). x,y,w,h = rectangle on screen (screen units). Call several
// times (one per player). Also activates mode 9.
static void mode9_split_fn(void){
    int h=getparm(),w=getparm(),y=getparm(),x=getparm();
    int m9off=getparm(),cam=getparm(),slot=getparm();
    if(slot<0||slot>=MAX_VIEWS){retval(0);return;}
    views[slot].active=1; views[slot].cam=cam; views[slot].bind=m9off;
    views[slot].vx=x; views[slot].vy=y; views[slot].vw=w; views[slot].vh=h;
    views[slot].haslast=0; views[slot].region=-1;   // explicit rect (not by number)
    if(slot==0){ m9_camera=cam; if(m9off)m9_bind=m9off; }  // primary (billboards)
    m9_active=1;
    retval(1);
}
// MODE9_COLLISION(on): 1=advance/xadvance don't go through walls (uses the
// camera process's radius field as min distance to wall), 0=free.
// ON by default (1): call with 0 only if you want to go through walls.
static void mode9_collision_fn(void){
    m9_collision=getparm()?1:0;
    retval(0);
}
// MODE9_STEP(height): MAX step/ramp height (DIV2 units) that is climbed
// automatically. >0 enables stairs and ramps (floor-following): walls
// lower than 'height' don't block (you climb them) and the player rests on the floor
// below (steps, ramps inclined up to ~60). 0 = disabled (classic:
// the DLL doesn't touch z, the PRG controls the height). Requires MODE9_COLLISION(1).
// IMPORTANT: GRAVITY is still applied by the PRG (z = z - something each frame); the
// DLL only RAISES z when the player rests on a floor/step. Call AFTER
// LOAD_WLD9 (it uses the unit scale set there).
static void mode9_step_fn(void){
    int d=getparm();
    m9_step=(d>0)?(float)d/m9_unitscale:0.0f;
    retval(0);
}
static void stop_mode9_fn(void){
    m9_active=0;
    retval(0);
}
// MODE9_SCRW()/MODE9_SCRH(): width/height of the current screen (the DLL knows them
// from _wide/_height). So the PRG can compute mouse and viewport limits at
// ANY resolution without hardcoding 320x200. Return 320/200 if there's no mode.
static void mode9_scrw_fn(void){ retval((_wide  && *_wide  >0)?*_wide  :320); }
static void mode9_scrh_fn(void){ retval((_height&& *_height>0)?*_height:200); }
static void mode9_set_fn(void){
    m9_pitch=getparm();      // pitch (last argument)
    m9_height=getparm();     // height
    retval(0);
}
static void mode9_unitscale_fn(void){
    int n=getparm();
    if(n>0) m9_unitscale=(float)n;
    retval(0);
}
// MODE9_CULL(mode): 0=no culling, 1=discards back faces (def), 2=inverted.
// Only use 1/2 with closed-volume meshes and consistent outward normals.
static void mode9_cull_fn(void){
    int m=getparm();
    if(m>=0&&m<=2) cull_mode=m;
    retval(0);
}
// MODE9_FOV(degrees): HORIZONTAL field of view in degrees (def ~71). Typical
// values: 60 (more "tele"/zoom), 90 (wide), 110 (fisheye). The default value
// is kept if not called. Affects the projection and the frustum culling
// (not the billboards of DIV2's c_screen path, which the DLL doesn't project).
//   focal = width * 0.5/tan(FOV/2). Applies to ALL views/viewports.
static void mode9_fov_fn(void){
    int deg=getparm();
    if(deg>=20&&deg<=150){
        float r=(float)deg*(3.14159265f/180.0f)*0.5f;   // half FOV in radians
        float t=(float)tan((double)r);
        if(t>0.01f) m9_focal_k=0.5f/t;
    }
    retval(0);
}
// MODE9_REGION(n, x, y, w, h): defines the rectangle of region n (1..15), like
// DIV2's define_region() but for mode 9 (the DLL doesn't see the engine's
// regions). Then START_MODE9(cam, &m9, n) shows that camera inside the region.
// Region 0 is always the full screen and can't be redefined. Passing w<=0
// or h<=0 clears the region. Returns 1 if registered, 0 if the number is invalid.
static void mode9_region_fn(void){
    int h=getparm(),w=getparm(),y=getparm(),x=getparm(),n=getparm();
    if(n<1||n>=MAX_REGIONS){retval(0);return;}
    if(w<=0||h<=0){ reg_def[n]=0; retval(0); return; }
    reg_x[n]=x; reg_y[n]=y; reg_w[n]=w; reg_h[n]=h; reg_def[n]=1;
    retval(1);
}
// MODE9_FARCLIP(dist): don't draw beyond 'dist' (DIV2 units). Clips the
// world polygons, the OBJ models (by their origin) and the billboards (by their
// distance to camera). 0 = no limit. If fog is active, it also clips at
// fog_far (what's behind the fog is the background color, no need to
// draw it).
static void mode9_farclip_fn(void){
    int d=getparm();
    far_clip=(d>0)?(float)d/m9_unitscale:0.0f;
    retval(0);
}
// MODE9_BIND(&m9): binds the PRG's m9 STRUCT (fields z,camera,height,angle).
// The DLL reads camera/height/angle from it each frame, avoiding MODE9_SET calls.
static void mode9_bind_fn(void){
    m9_bind=getparm();
    retval(0);
}

// ============================================================================
// SET_ENV_COLOR9 / SET_FOG9 / MODE9_FOG_OFF
// ============================================================================
// DIV2 uses range 0..100 per channel; the VGA palette is 6-bit (0..63). It's
// scaled: 100 -> 63 (white). (It used to write the raw value into the palette,
// which is why 100 or 255 didn't give white: they overflowed the 6-bit range.)
static int col100to63(int v){
    if(v<0)v=0; if(v>100)v=100;
    return (v*63+50)/100;
}
static void set_env_color9_fn(void){
    int b=getparm(),g=getparm(),r=getparm();   // 0..100 per channel
    env_r=col100to63(r); env_g=col100to63(g); env_b=col100to63(b);
    palette[PAL_ENV*3+0]=(char)env_r;
    palette[PAL_ENV*3+1]=(char)env_g;
    palette[PAL_ENV*3+2]=(char)env_b;
    set_palette=1;
    // If fog is active, its color = ambient color: rebuild it.
    if(fog_on) build_fog_map(env_r,env_g,env_b);
    retval(0);
}
// SET_FOG9(near, far): distance fog, like mode 8's set_fog. The fog
// grows LINEARLY with distance: transparent up to 'near', and fully
// opaque (ambient color) at 'far'. The COLOR is that of SET_ENV_COLOR9.
// DIFFERENCE from mode 8: there near/far are 0..100 (percentage); here they are
// ABSOLUTE DISTANCES in DIV2 units (more precise), e.g. SET_FOG9(300,1400).
// As in mode 8, far ~ 2*near is usual.
static void set_fog9_fn(void){
    int ffar=getparm(),fnear=getparm();    // range in DIV2 units
    fog_near=(float)fnear/m9_unitscale;
    fog_far =(float)ffar /m9_unitscale;
    build_fog_map(env_r,env_g,env_b);      // fog color = ambient color
    fog_on=(fog_map!=NULL && fog_far>fog_near);
    // Levels/unit slope: level = (depth-fog_near)*fog_scale (see fog_level).
    fog_scale=(fog_far>fog_near)?((float)(FOG_LEVELS-1)/(fog_far-fog_near)):0.0f;
    retval(0);
}
static void mode9_fog_off_fn(void){
    fog_on=0;
    retval(0);
}

// ============================================================================
// Reads the camera (process) and fills position/orientation. Returns 1 if OK.
// ============================================================================
static int get_camera(int cam,int bind,
                      float *cx,float *cy,float *cz,float *yaw,float *pitch,
                      int *outbase){
    int eyeh=m9_height, pitch_i=m9_pitch, base;
    // If there's a bound m9 STRUCT, read camera/height/angle from it (consecutive
    // fields: z=+0, camera=+1, height=+2, angle=+3). Avoids MODE9_SET.
    if(bind){
        cam     =mem[bind+1];
        eyeh    =mem[bind+2];
        pitch_i =mem[bind+3];
    }
    base=find_process(cam);
    if(outbase)*outbase=base;
    if(base<0) return 0;
    *cx   =(float)mem[base+P_X]/m9_unitscale;
    *cz   =(float)mem[base+P_Y]/m9_unitscale;     // process.y -> world Z
    *cy   =(float)(mem[base+P_Z]+eyeh)/m9_unitscale;   // eye height
    *yaw  =(float)mem[base+P_ANGLE]*(3.14159265f/180000.0f);
    *pitch=(float)pitch_i*(3.14159265f/180000.0f);
    return 1;
}

// ============================================================================
// floor_height: floor height under the point (px,pz) in OBJ units. Walks the
// floor triangles, does the point-in-triangle test in XZ (barycentric
// coordinates) and interpolates the plane's Y. Returns the HIGHEST floor whose
// height doesn't exceed ceil_y (so in a 2-story building it doesn't raise you to the ceiling).
// Returns 1 and writes *fy if it finds a floor; 0 if there's no floor under the point.
// ============================================================================
static int floor_height(float px,float pz,float ceil_y,float *fy){
    int i,found=0; float best=-1e30f;
    for(i=0;i<n_floors;i++){
        int t=mesh_floor_idx[i];
        int a=mesh_t[t].a,b=mesh_t[t].b,c=mesh_t[t].c;
        float x0=mesh_v[a].x,z0=mesh_v[a].z,y0=mesh_v[a].y;
        float x1=mesh_v[b].x,z1=mesh_v[b].z,y1=mesh_v[b].y;
        float x2=mesh_v[c].x,z2=mesh_v[c].z,y2=mesh_v[c].y;
        float d=(z1-z2)*(x0-x2)+(x2-x1)*(z0-z2);    // 2*area in XZ (!=0 = not vertical)
        float l0,l1,l2,y,inv;
        if(d<1e-6f&&d>-1e-6f)continue;
        inv=1.0f/d;
        l0=((z1-z2)*(px-x2)+(x2-x1)*(pz-z2))*inv;
        l1=((z2-z0)*(px-x2)+(x0-x2)*(pz-z2))*inv;
        l2=1.0f-l0-l1;
        if(l0<-0.001f||l1<-0.001f||l2<-0.001f)continue;   // (px,pz) outside the triangle
        y=l0*y0+l1*y1+l2*y2;
        if(y<=ceil_y+0.05f && y>best){best=y;found=1;}
    }
    if(found)*fy=best;
    return found;
}

// ============================================================================
// ceil_height: height of the LOWEST ceiling/roof strictly above 'above_y' at
// the point (px,pz), in OBJ units. Same XZ point-in-triangle test as
// floor_height but over the DOWNWARD-facing triangles, keeping the SMALLEST y.
// Used for HEAD collision: the caller caps the feet so feet+PLAYER_BODY stays
// under this ceiling, so you can't jump through a roof when collisions are on.
// Returns 1 and writes *cy if it finds a ceiling overhead; 0 otherwise.
// ============================================================================
static int ceil_height(float px,float pz,float above_y,float *cy){
    int i,found=0; float best=1e30f;
    for(i=0;i<n_ceils;i++){
        int t=mesh_ceil_idx[i];
        int a=mesh_t[t].a,b=mesh_t[t].b,c=mesh_t[t].c;
        float x0=mesh_v[a].x,z0=mesh_v[a].z,y0=mesh_v[a].y;
        float x1=mesh_v[b].x,z1=mesh_v[b].z,y1=mesh_v[b].y;
        float x2=mesh_v[c].x,z2=mesh_v[c].z,y2=mesh_v[c].y;
        float d=(z1-z2)*(x0-x2)+(x2-x1)*(z0-z2);
        float l0,l1,l2,y,inv;
        if(d<1e-6f&&d>-1e-6f)continue;
        inv=1.0f/d;
        l0=((z1-z2)*(px-x2)+(x2-x1)*(pz-z2))*inv;
        l1=((z2-z0)*(px-x2)+(x0-x2)*(pz-z2))*inv;
        l2=1.0f-l0-l1;
        if(l0<-0.001f||l1<-0.001f||l2<-0.001f)continue;   // (px,pz) outside the triangle
        y=l0*y0+l1*y1+l2*y2;
        if(y>above_y && y<best){best=y;found=1;}          // lowest ceiling overhead
    }
    if(found)*cy=best;
    return found;
}

// ============================================================================
// 2D collision (XZ plane) of a circle (camera, radius r in OBJ units) against the
// wall segments. Pushes the camera out of the walls that overlap with
// its height (sliding: only the perpendicular component is corrected, so it
// "slides" along the wall). Modifies *cx,*cz. Walls whose top is
// <= 'step' above the feet do NOT block: they're steps the
// floor-follow climbs (so you can enter a staircase).
// ============================================================================
static void collide_xz(float *cx,float *cz,float cy,float r,float feet,float step){
    float ylo=cy-PLAYER_BODY, yhi=cy+0.3f;   // player's vertical strip (OBJ)
    float r2=r*r, reach=r*2.0f+0.01f;        // reach: covers the 2 passes + push
    int pass,wi;
    // Only tests the nearby walls (grid). reach=2r guarantees including every
    // wall within <=r of the player after both passes' pushes (see gather_walls).
    gather_walls(*cx-reach,*cz-reach,*cx+reach,*cz+reach);
    for(pass=0;pass<2;pass++){               // 2 passes: resolves corners
        for(wi=0;wi<n_wcand;wi++){
            int w=wcand[wi];
            float ax,az,bx,bz,dx,dz,len2,t,px,pz,ox,oz,d2;
            int atEnd;
            if(mesh_walls[w].ymax<ylo || mesh_walls[w].ymin>yhi) continue;
            if(step>0.0f && mesh_walls[w].ymax<=feet+step) continue;  // step: doesn't block
            ax=mesh_walls[w].x0; az=mesh_walls[w].z0;
            bx=mesh_walls[w].x1; bz=mesh_walls[w].z1;
            dx=bx-ax; dz=bz-az; len2=dx*dx+dz*dz;
            t = (len2>1e-9f) ? (((*cx-ax)*dx+(*cz-az)*dz)/len2) : 0.0f;
            atEnd=0;
            if(t<0.0f){t=0.0f; atEnd=1;} else if(t>1.0f){t=1.0f; atEnd=2;}
            // The nearest point falls on an END (radial cap). If that end
            // is an interior vertex (cap=0), skip it: the adjacent wall already stops you and
            // this way you don't snag at joints/corners (see M9_CORNER_SLIDE).
            if(atEnd==1 && !mesh_walls[w].cap0) continue;
            if(atEnd==2 && !mesh_walls[w].cap1) continue;
            px=ax+dx*t; pz=az+dz*t;          // nearest point of the segment
            ox=*cx-px; oz=*cz-pz; d2=ox*ox+oz*oz;
            if(d2<r2){
                if(d2>1e-10f){
                    float inv=fast_rsqrt(d2); float push=(r-1.0f/inv)*inv;
                    *cx+=ox*push; *cz+=oz*push;
                } else {                      // right above: push along the normal
                    float nx=-dz, nz2=dx, nl2=nx*nx+nz2*nz2;
                    if(nl2>1e-10f){ float inv=fast_rsqrt(nl2); *cx+=nx*inv*r; *cz+=nz2*inv*r; }
                }
            }
        }
    }
}

// SWEPT collision: avoids "tunneling" when the camera moves
// faster than its radius. If the movement segment (lx,lz)->(cx,cz) crosses
// a wall's plane at distance < r, it stops the camera at distance r from the
// plane on the side it came from. collide_xz then refines the sliding.
static void collide_swept(float lx,float lz,float *cx,float *cz,float cy,float r,
                          float feet,float step){
    float ylo=cy-PLAYER_BODY, yhi=cy+0.3f;
    float mnx=lx<*cx?lx:*cx, mxx=lx>*cx?lx:*cx;     // AABB of the movement segment
    float mnz=lz<*cz?lz:*cz, mxz=lz>*cz?lz:*cz;
    float m=r+0.01f;
    int wi;
    gather_walls(mnx-m,mnz-m,mxx+m,mxz+m);          // only walls the sweep crosses
    for(wi=0;wi<n_wcand;wi++){
        int w=wcand[wi];
        float ax,az,ex,ez,el2,nx,nz,nl,dl,dc,t,ix,iz,st,target,denom;
        if(mesh_walls[w].ymax<ylo||mesh_walls[w].ymin>yhi)continue;
        if(step>0.0f && mesh_walls[w].ymax<=feet+step)continue;   // step: doesn't block
        ax=mesh_walls[w].x0; az=mesh_walls[w].z0;
        ex=mesh_walls[w].x1-ax; ez=mesh_walls[w].z1-az; el2=ex*ex+ez*ez;
        if(el2<1e-9f)continue;
        nx=-ez; nz=ex; {float nl2=nx*nx+nz*nz; if(nl2<1e-12f)continue;
        {float inv=fast_rsqrt(nl2); nx*=inv; nz*=inv;}}
        dl=(lx-ax)*nx+(lz-az)*nz;        // perpendicular distance of the origin
        dc=(*cx-ax)*nx+(*cz-az)*nz;      // perpendicular distance of the destination
        // does it cross the radius-r "band" of the plane coming from outside?
        if(dl>=r && dc<r)      target= r;
        else if(dl<=-r && dc>-r) target=-r;
        else continue;
        denom=dl-dc; if(denom>-1e-6f&&denom<1e-6f)continue;
        t=(dl-target)/denom; if(t<0.0f)t=0.0f; else if(t>1.0f)t=1.0f;
        ix=lx+(*cx-lx)*t; iz=lz+(*cz-lz)*t;
        st=((ix-ax)*ex+(iz-az)*ez)/el2;  // does the crossing fall inside the segment?
        // Inside the segment: always stop. Beyond an end: only if that
        // end is a REAL corner (cap=1); if it's interior (joint/jamb, cap=0)
        // it doesn't stop, to avoid false blocks at the gate edges.
        {
            int ok;
            if(st>=0.0f&&st<=1.0f) ok=1;
            else if(st<0.0f)       ok=(mesh_walls[w].cap0 && st>=-0.15f);
            else                   ok=(mesh_walls[w].cap1 && st<= 1.15f);
            if(ok){ *cx=ix; *cz=iz; }
        }
    }
}

// ============================================================================
// MODE9_COLLIDE(pid): pushes ANY process out of the walls (sliding),
// not just the camera. Reads its x,y (ground), z (height) and the process's radius; corrects
// x,y and rewrites them. Returns 1 if it moved it (it was inside a wall), 0 if not.
// It's pure sliding (no anti-tunneling sweep): meant for NPCs at
// normal speed; for something very fast, call it several times or reduce its step.
// z (P_Z) is treated as the FEET height; the body rises PLAYER_BODY.
// ============================================================================
static void mode9_collide_fn(void){
    int pid=getparm();
    int base=find_process(pid);
    float cx,cz,cy,r,ox,oz;
    if(base<0 || n_walls<=0){ retval(0); return; }
    cx=(float)mem[base+P_X]/m9_unitscale;
    cz=(float)mem[base+P_Y]/m9_unitscale;
    cy=(float)mem[base+P_Z]/m9_unitscale + PLAYER_BODY;   // vertical band of the body
    r =(float)mem[base+P_RADIUS]/m9_unitscale;
    if(r<=0.0f) r=(float)COLLIDE_DEF/m9_unitscale;
    ox=cx; oz=cz;
    {
        float feet=(float)mem[base+P_Z]/m9_unitscale;
        collide_xz(&cx,&cz,cy,r,feet,m9_step);
        // Steps/ramps: raise the process's feet to the floor below it (<= m9_step).
        if(m9_step>0.0f && n_floors>0){
            float fy;
            if(floor_height(cx,cz,feet+m9_step,&fy) && feet<fy){
                float fz=fy*m9_unitscale;
                mem[base+P_Z]=(int)(fz>=0?fz+0.5f:fz-0.5f);
            }
        }
    }
    {float fx=cx*m9_unitscale, fz=cz*m9_unitscale;
     mem[base+P_X]=(int)(fx>=0?fx+0.5f:fx-0.5f);
     mem[base+P_Y]=(int)(fz>=0?fz+0.5f:fz-0.5f);}
    retval((ox!=cx||oz!=cz)?1:0);
}

// ============================================================================
// Billboard sprites: FPG parsing + scaled draw (blit) with occlusion.
// ============================================================================
static void free_sprites(void){
    int i;
    for(i=0;i<n_fpgspr;i++){ if(fspr[i].pix){div_free(fspr[i].pix);fspr[i].pix=NULL;} }
    n_fpgspr=0;
}
// load_fpg_file: parses a DIV2 .FPG (FPGHEADER/FPGBODY format from div.h) and
// stores each graphic (code, w, h, control point, pixels). Returns num
// graphics. Enables billboard drawing by the DLL (multi-view).
static int load_fpg_file(char *fname){
    FILE *f; char magic[8]; int count=0;
    free_sprites();
    f=div_fopen(fname,"rb");
    if(!f){s_cpy(m9_msg,"FPG NO FILE");return 0;}
    if((int)fread(magic,1,8,f)!=8||magic[0]!='f'||magic[1]!='p'||magic[2]!='g'){
        s_cpy(m9_msg,"FPG bad");div_fclose(f);return 0;
    }
    fseek(f,768L+576L,SEEK_CUR);          // skip palette(768)+colorbars(16*36)
    while(n_fpgspr<MAX_FPGSPR){
        int code,length,wide,height,npoints,psz,cx,cy;
        char desc[32],fn2[12];
        unsigned short pt[2];
        if((int)fread(&code,4,1,f)!=1) break;
        if((int)fread(&length,4,1,f)!=1) break;
        if((int)fread(desc,1,32,f)!=32) break;
        if((int)fread(fn2,1,12,f)!=12) break;
        if((int)fread(&wide,4,1,f)!=1) break;
        if((int)fread(&height,4,1,f)!=1) break;
        if((int)fread(&npoints,4,1,f)!=1) break;
        if(wide<=0||height<=0||wide>2048||height>2048||npoints<0) break;
        cx=wide/2; cy=height/2;
        if(npoints>0){
            if((int)fread(pt,2,2,f)!=2) break;
            if((short)pt[0]>=0&&(short)pt[1]>=0){cx=(short)pt[0];cy=(short)pt[1];}
            if(npoints>1) fseek(f,(long)(npoints-1)*4L,SEEK_CUR);
        }
        psz=wide*height;
        {char *pix=(char*)div_malloc((size_t)psz);
         if(!pix) break;
         if((int)fread(pix,1,(size_t)psz,f)!=psz){div_free(pix);break;}
         fspr[n_fpgspr].code=code; fspr[n_fpgspr].w=wide; fspr[n_fpgspr].h=height;
         fspr[n_fpgspr].cx=cx; fspr[n_fpgspr].cy=cy; fspr[n_fpgspr].pix=pix;
         n_fpgspr++; count++;}
    }
    div_fclose(f);
    {char num[12];s_cpy(m9_msg,"FPG ");s_itos(count,num);s_cat(m9_msg,num);s_cat(m9_msg," gr");}
    return count;
}
static int find_sprite(int code){
    int i; for(i=0;i<n_fpgspr;i++) if(fspr[i].code==code) return i; return -1;
}
// Draws sprite 'idx' centered on its control point at (scx,scy) of
// screen, scaled to sizepct%, clipped to the viewport (clip_*), with color 0
// transparent and z-buffer test/write (zfix) for per-pixel occlusion.
// fogmap: fog color remap for THIS sprite (by its distance), or
// NULL if there's no fog. Applied to the pixel before writing it (like the
// geometry), so distant billboards blend with the fog instead of popping.
static void blit_sprite(int idx,int scx,int scy,int sizepct,int zfix,char *vid,int W,char *fogmap){
    FpgSpr *s=&fspr[idx];
    int dw=s->w*sizepct/100, dh=s->h*sizepct/100;
    int x0,y0,sxstep,systep,cx0,cy0,cx1,cy1,su0,sv,x,y;
    if(dw<1||dh<1||dw>4000||dh>4000)return;
    x0=scx-(s->cx*sizepct/100); y0=scy-(s->cy*sizepct/100);
    sxstep=(s->w<<16)/dw; systep=(s->h<<16)/dh;
    cx0=x0; cy0=y0; cx1=x0+dw; cy1=y0+dh; su0=0; sv=0;
    if(cx0<clip_x0){su0=(clip_x0-x0)*sxstep; cx0=clip_x0;}
    if(cy0<clip_y0){sv =(clip_y0-y0)*systep; cy0=clip_y0;}
    if(cx1>clip_x1)cx1=clip_x1;
    if(cy1>clip_y1)cy1=clip_y1;
    if(cx0>=cx1||cy0>=cy1)return;
    for(y=cy0;y<cy1;y++){
        char *prow=vid + (long)y*W;
        unsigned short *zrow=zbuf + (long)y*W;
        char *srow=s->pix + (long)(sv>>16)*s->w;
        int su=su0;
        if(fogmap){
            for(x=cx0;x<cx1;x++){
                char px=srow[su>>16];
                if(px!=0 && zfix>(int)zrow[x]){ prow[x]=fogmap[(unsigned char)px]; zrow[x]=(unsigned short)zfix; }
                su+=sxstep;
            }
        } else {
            for(x=cx0;x<cx1;x++){
                char px=srow[su>>16];
                if(px!=0 && zfix>(int)zrow[x]){ prow[x]=px; zrow[x]=(unsigned short)zfix; }
                su+=sxstep;
            }
        }
        sv+=systep;
    }
}

// Transforms a world point (OBJ units) to camera space. Returns cx,cy,cz.
static void world_to_cam(float Px,float Py,float Pz,
                         float Cx,float Cy,float Cz,
                         float ysin,float ycos,float psin,float pcos,
                         float *ocx,float *ocy,float *ocz){
    float dx=Px-Cx, dy=Py-Cy, dz=Pz-Cz;
    // Yaw around Y. Camera X axis = F x up (same handedness as
    // mode 8: the horizontal turn is NOT mirrored).
    float rx = dx*ysin + dz*ycos;
    float rz = dx*ycos + dz*(-ysin);
    float ry = dy;
    // Pitch around the camera's X axis
    float y2 = ry*pcos - rz*psin;
    float z2 = ry*psin + rz*pcos;
    *ocx=rx; *ocy=y2; *ocz=z2;
}

// ============================================================================
// do_render: core of mode 9 (first-person camera + near clip + fog)
// ============================================================================
// Flat-shading color of a triangle (normal in OBJ units against the light).
static unsigned char flat_color(int a,int b,int c){
    float ex1=mesh_v[b].x-mesh_v[a].x,ey1=mesh_v[b].y-mesh_v[a].y,ez1=mesh_v[b].z-mesh_v[a].z;
    float ex2=mesh_v[c].x-mesh_v[a].x,ey2=mesh_v[c].y-mesh_v[a].y,ez2=mesh_v[c].z-mesh_v[a].z;
    float nx=ey1*ez2-ez1*ey2,ny=ez1*ex2-ex1*ez2,nz=ex1*ey2-ey1*ex2;
    float nlen=(float)sqrt((double)(nx*nx+ny*ny+nz*nz));
    float sh; int ci;
    if(nlen>1e-6f){nx/=nlen;ny/=nlen;nz/=nlen;}
    sh=nx*0.4082f+ny*0.8165f+nz*(-0.4082f);
    if(sh<0.0f)sh=-sh;
    sh=AMBIENT+(1.0f-AMBIENT)*sh;
    ci=(int)(sh*(float)(PAL_SHADES-1)+0.5f);
    if(ci<0)ci=0;if(ci>=PAL_SHADES)ci=PAL_SHADES-1;
    return (unsigned char)(PAL_BASE+ci);
}

// flat_color_v: like flat_color but receives V3* instead of global indices.
// Used by load_obj9_model to precompute each triangle's flat-shading color
// model triangle (the tri's normal against the same light as the world).
static unsigned char flat_color_v(V3 *va,V3 *vb,V3 *vc){
    float ex1=vb->x-va->x,ey1=vb->y-va->y,ez1=vb->z-va->z;
    float ex2=vc->x-va->x,ey2=vc->y-va->y,ez2=vc->z-va->z;
    float nx=ey1*ez2-ez1*ey2,ny=ez1*ex2-ex1*ez2,nz=ex1*ey2-ey1*ex2;
    float nlen=(float)sqrt((double)(nx*nx+ny*ny+nz*nz));
    float sh; int ci;
    if(nlen>1e-6f){nx/=nlen;ny/=nlen;nz/=nlen;}
    sh=nx*0.4082f+ny*0.8165f+nz*(-0.4082f);
    if(sh<0.0f)sh=-sh;
    sh=AMBIENT+(1.0f-AMBIENT)*sh;
    ci=(int)(sh*(float)(PAL_SHADES-1)+0.5f);
    if(ci<0)ci=0;if(ci>=PAL_SHADES)ci=PAL_SHADES-1;
    return (unsigned char)(PAL_BASE+ci);
}

// build_frame_name: builds "<base><n>.OBJ" (e.g. base="CAT",n=3 -> "CAT3.OBJ").
// Supports multi-digit n in case >10 frames are used in the future.
static void build_frame_name(char *dst,const char *base,int n){
    char tmp[12]; int k=0;
    strcpy(dst,base);
    if(n<=0){ tmp[k++]='0'; }
    else { int t=n; char rev[12]; int r=0;
           while(t>0){rev[r++]=(char)('0'+(t%10));t/=10;}
           while(r>0)tmp[k++]=rev[--r]; }
    tmp[k]=0;
    strcat(dst,tmp);
    strcat(dst,".OBJ");
}

// load_obj9_verts_only: parses ONLY the 'v' positions of an OBJ into dst (scaled
// by sfac). For animation frames >0, which share topology (vt/f) with
// frame 0. Returns the number of vertices read.
static int load_obj9_verts_only(char *fname,V3 *dst,int maxv,float sfac){
    FILE *f; int nv=0; char line[256];
    f=div_fopen(fname,"r");
    if(!f) return 0;
    while(read_line(f,line,sizeof(line))){
        char *p=line;
        while(*p==' '||*p=='\t')p++;
        if(p[0]=='v'&&p[1]==' '){
            float x,y,z; p+=2;
            x=sc_atof(p);p=next_tok(p);y=sc_atof(p);p=next_tok(p);z=sc_atof(p);
            if(nv<maxv){dst[nv].x=x*sfac;dst[nv].y=y*sfac;dst[nv].z=z*sfac;nv++;}
        }
    }
    div_fclose(f);
    return nv;
}

// load_obj9_model: parses an OBJ into an Obj9Model. sfac = scale factor
// (model_unitscale / m9_unitscale) to convert OBJ verts -> world space.
// Uses div_malloc for the temporaries and for the model's final arrays.
static int load_obj9_model(Obj9Model *mdl,char *fname,float sfac){
    FILE *f;
    V3    *tv;
    UV2   *tuv;
    TriTex *ttr;
    int nv=0,nuv=0,nt=0,i,has_uv=0;
    char line[256];
    f=div_fopen(fname,"r");
    if(!f) return 0;
    tv =(V3*   )div_malloc(MAX_OBJ9V *sizeof(V3));
    tuv=(UV2*  )div_malloc(MAX_OBJ9UV*sizeof(UV2));
    ttr=(TriTex*)div_malloc(MAX_OBJ9T *sizeof(TriTex));
    if(!tv||!tuv||!ttr){
        if(tv)div_free(tv);if(tuv)div_free(tuv);if(ttr)div_free(ttr);
        div_fclose(f);return 0;
    }
    while(read_line(f,line,sizeof(line))){
        char *p=line;
        while(*p==' '||*p=='\t')p++;
        if(p[0]=='v'&&p[1]==' '){
            float x,y,z; p+=2;
            x=sc_atof(p);p=next_tok(p);y=sc_atof(p);p=next_tok(p);z=sc_atof(p);
            if(nv<MAX_OBJ9V){tv[nv].x=x*sfac;tv[nv].y=y*sfac;tv[nv].z=z*sfac;nv++;}
        } else if(p[0]=='v'&&p[1]=='t'){
            float u,v; p+=3;
            u=sc_atof(p);p=next_tok(p);v=sc_atof(p);
            if(nuv<MAX_OBJ9UV){tuv[nuv].u=u;tuv[nuv].v=1.0f-v;nuv++;has_uv=1;}
        } else if(p[0]=='f'&&p[1]==' '){
            int vi[8],uvi[8],nf=0; p+=2;
            while(*p&&*p!='\n'&&nf<8){
                while(*p==' '||*p=='\t')p++;
                if(!*p||*p=='\n')break;
                p=parse_face_vert(p,&vi[nf],&uvi[nf]);
                nf++;
            }
            for(i=1;i<nf-1&&nt<MAX_OBJ9T;i++){
                int ia=vi[0],ib=vi[i],ic=vi[i+1];
                if(ia<0||ia>=nv||ib<0||ib>=nv||ic<0||ic>=nv) continue;
                ttr[nt].a=ia;ttr[nt].b=ib;ttr[nt].c=ic;
                ttr[nt].ua=(uvi[0]  >=0&&uvi[0]  <nuv)?uvi[0]  :-1;
                ttr[nt].ub=(uvi[i]  >=0&&uvi[i]  <nuv)?uvi[i]  :-1;
                ttr[nt].uc=(uvi[i+1]>=0&&uvi[i+1]<nuv)?uvi[i+1]:-1;
                ttr[nt].mat=0;
                ttr[nt].flat=flat_color_v(&tv[ia],&tv[ib],&tv[ic]);
                nt++;
            }
        }
    }
    div_fclose(f);
    if(nv==0||nt==0){div_free(tv);div_free(tuv);div_free(ttr);return 0;}
    mdl->verts=(V3*   )div_malloc((size_t)(nv *sizeof(V3)));
    mdl->uvs  =(UV2*  )div_malloc((size_t)((nuv>0?nuv:1)*sizeof(UV2)));
    mdl->tris =(TriTex*)div_malloc((size_t)(nt *sizeof(TriTex)));
    if(!mdl->verts||!mdl->uvs||!mdl->tris){
        if(mdl->verts)div_free(mdl->verts);
        if(mdl->uvs)  div_free(mdl->uvs);
        if(mdl->tris) div_free(mdl->tris);
        mdl->verts=NULL;mdl->uvs=NULL;mdl->tris=NULL;
        div_free(tv);div_free(tuv);div_free(ttr);return 0;
    }
    for(i=0;i<nv;i++) mdl->verts[i]=tv[i];
    for(i=0;i<nuv;i++)mdl->uvs[i]  =tuv[i];
    for(i=0;i<nt;i++) mdl->tris[i] =ttr[i];
    mdl->nv=nv;mdl->nuv=(has_uv?nuv:0);mdl->nt=nt;
    mdl->vframes=mdl->verts;mdl->nframes=1;   // static by default
    div_free(tv);div_free(tuv);div_free(ttr);
    return nt;
}

// Transforms and projects a vertex to camera space ONLY if not already done this
// frame (vstamp). Uses the camera params of the current view (g_*). Fills
// camv/psx/psy/pbehind as before, for the loop that scanned all vertices.
static void xform_vertex(int i){
    float dx,dy,dz,rx,rz,ocx,ocy,ocz;
    if(vstamp[i]==frame_stamp) return;
    vstamp[i]=frame_stamp;
    dx=mesh_v[i].x-g_Cx; dy=mesh_v[i].y-g_Cy; dz=mesh_v[i].z-g_Cz;
    rx=dx*g_ysin+dz*g_ycos;
    rz=dx*g_ycos-dz*g_ysin;
    ocx=rx;
    ocy=dy*g_pcos-rz*g_psin;
    ocz=dy*g_psin+rz*g_pcos;
    camv[i][0]=ocx;camv[i][1]=ocy;camv[i][2]=ocz;
    if(ocz>=NEAR_PLANE){
        float inv=fast_recip(ocz);
        float fx=g_hw+ocx*g_focal*inv, fy=g_hh-ocy*g_focal*inv;
        psxf[i]=fx; psyf[i]=fy;                 // subpixel (for the rasterizing)
        psx[i]=fast_ftol(fx); psy[i]=fast_ftol(fy); // integer (for culling)
        pbehind[i]=0;
    } else pbehind[i]=1;
}

// Culls a triangle (near cull + far clip + backface + frustum) and, if it passes,
// adds it to tsort[] for the bucket sort. Transforms its vertices
// lazily (xform_vertex). Common to the grid path and the fallback.
static void consider_tri(int i,float fcull,int *pndraw){
    int a=mesh_t[i].a,b=mesh_t[i].b,c=mesh_t[i].c;
    float za,zb,zc,depth,zmin; int nd;
    xform_vertex(a); xform_vertex(b); xform_vertex(c);
    za=camv[a][2]; zb=camv[b][2]; zc=camv[c][2];
    if(za<NEAR_PLANE&&zb<NEAR_PLANE&&zc<NEAR_PLANE)return;
    if(fcull>0.0f){
        zmin=za; if(zb<zmin)zmin=zb; if(zc<zmin)zmin=zc;
        if(zmin>fcull)return;
    }
    nd=*pndraw;
    if(!pbehind[a]&&!pbehind[b]&&!pbehind[c]){
        // Whole triangle in front of the near: use the projection already done.
        int ax=psx[a],ay=psy[a],bx=psx[b],by=psy[b],cx2=psx[c],cy2=psy[c];
        if(cull_mode){
            long area=(long)(bx-ax)*(long)(cy2-ay)-(long)(cx2-ax)*(long)(by-ay);
            if(cull_mode==1&&area>=0)return;   // discard back faces
            if(cull_mode==2&&area<=0)return;   // (inverted winding)
        }
        {   // frustum (viewport rect): all 3 outside on the same side
            int o0=0,o1=0,o2=0;
            if(ax<clip_x0)o0|=1; else if(ax>=clip_x1)o0|=2; if(ay<clip_y0)o0|=4; else if(ay>=clip_y1)o0|=8;
            if(bx<clip_x0)o1|=1; else if(bx>=clip_x1)o1|=2; if(by<clip_y0)o1|=4; else if(by>=clip_y1)o1|=8;
            if(cx2<clip_x0)o2|=1;else if(cx2>=clip_x1)o2|=2;if(cy2<clip_y0)o2|=4;else if(cy2>=clip_y1)o2|=8;
            if(o0&o1&o2)return;
        }
        tsort[nd].ci=0;   // doesn't need clipping
    } else {
        tsort[nd].ci=1;   // crosses the near: clip during rasterizing
    }
    depth=(za+zb+zc)*(1.0f/3.0f);
    tsort[nd].depth=depth;
    tsort[nd].idx=i;
    *pndraw=nd+1;
}

// ============================================================================
// render_sky: fills the viewport background with the cylindrical panoramic sky,
// treated as infinitely-distant geometry: each pixel depends ONLY on the
// ray direction (azimuth/elevation), not on position. So it stays "pinned"
// to the world and moves naturally when turning (yaw) and looking up/down (pitch).
//   azimuth(x)    = -yaw + atan((x-cx)/focal)          -> texture column (wraps 360)
//   elevation(y) =  pitch + atan((cy-y)/focal)         -> texture row
// Both axes use the SAME px-texture/radian scale (sky_w/2pi) to not distort.
// Drawn WITHOUT z-buffer (the geometry is rasterized on top). Respects the line
// doubler 'step'. The sign of 'base' matches the engine's handedness
// (rz=dx*cos-dz*sin); if the sky turned the wrong way, swap -yaw for +yaw.
// ============================================================================
static void render_sky(int vx,int vy,int vw,int vh,float focal,float hh,
                       float yaw,float pitch,char *vid,int W,int step){
    static int   skycol[SKY_MAXDIM];
    static int   skyrow[SKY_MAXDIM];
    static int   skyhz[SKY_MAXDIM];      // fog/haze level per row (if there's fog)
    static float rowang[SKY_MAXDIM];     // atan((center-y)/focal) per screen row
    int x,y,base, dofog=(fog_on&&fog_map!=NULL);
    float k=(float)sky_w/6.2831853f;     // texture px per radian (same in H and V)
    float hazespan=0.35f;                // rad (~20): below this elevation the sky blends with the fog
    if(vw>SKY_MAXDIM||vh>SKY_MAXDIM||sky_w<=0||sky_h<=0) return;
    // Per-column/row angle tables (atan). Only recomputed if the
    // viewport or texture size changes (not every frame).
    if(sky_cd_vw!=vw || sky_cd_vh!=vh || sky_cd_w!=sky_w){
        float half=(float)vw*0.5f, hrel=hh-(float)vy;
        for(x=0;x<vw;x++)
            sky_coldelta[x]=(int)((float)atan((double)(((float)x-half)/focal))*k);
        for(y=0;y<vh;y++)
            rowang[y]=(float)atan((double)((hrel-(float)y)/focal));
        sky_cd_vw=vw; sky_cd_vh=vh; sky_cd_w=sky_w;
    }
    // Texture column per screen column (azimuth, wrapped to [0,sky_w)).
    base=(int)(-yaw*k);
    for(x=0;x<vw;x++){
        int c=base+sky_coldelta[x]; c%=sky_w; if(c<0)c+=sky_w;
        skycol[x]=c;
    }
    // Texture row per screen row (elevation). Row sky_h-1 = horizon
    // (elevation 0), row 0 = zenith. Below the horizon it clamps (the floor covers it).
    // If there's fog: haze level per row by elevation (horizon->max,
    // zenith->0) so the sky blends with the fog SEAMLESSLY.
    for(y=0;y<vh;y++){
        float el=pitch+rowang[y];
        int ty=(sky_h-1)-(int)(el*k);
        if(ty<0)ty=0; else if(ty>=sky_h)ty=sky_h-1;
        skyrow[y]=ty*sky_w;
        if(dofog){
            int lv;
            if(el<=0.0f) lv=FOG_LEVELS-1;
            else if(el>=hazespan) lv=0;
            else lv=(int)((1.0f-el/hazespan)*(float)(FOG_LEVELS-1)+0.5f);
            skyhz[y]=lv;
        }
    }
    for(y=0;y<vh;y+=step){
        char *p=vid+(long)(vy+y)*W+vx;
        int row=skyrow[y];
        if(dofog && skyhz[y]>0){
            char *fm=&fog_map[skyhz[y]*256];
            for(x=0;x<vw;x++) p[x]=fm[(unsigned char)sky_data[row+skycol[x]]];
        } else {
            for(x=0;x<vw;x++) p[x]=sky_data[row+skycol[x]];
        }
    }
}

// Renders all the 3D object instances queued with DRAW_OBJ9 in the
// current view. Uses the global camera params (g_*) already set
// by render_view. Reuses tsort[]/order[]/bcnt[] (the world's sort is
// done) and the existing z-buffer -> correct occlusion with the static geometry.
static void render_obj9_instances(char *vid,int W,int H,float fcull){
    int qi;
    (void)H;
    if(!n_obj9_queue) return;
    for(qi=0;qi<n_obj9_queue;qi++){
        Obj9Inst  *inst=&obj9_queue[qi];
        Obj9Model *mdl =&obj9_models[inst->midx];
        int i,ti,ndraw=0;
        float iys=inst->ysin,iyc=inst->ycos;
        float ix=inst->ix,iy=inst->iy,iz=inst->iz;
        // Per-object far clip: discard the whole instance if its ORIGIN is
        // farther than fcull (same distance at which the world polygons
        // disappear). Cheap: 1 transform of the center, not of every vertex.
        if(fcull>0.0f){
            float dx=ix-g_Cx, dz=iz-g_Cz;
            float oz=(dx*g_ycos-dz*g_ysin)*g_pcos + (iy-g_Cy)*g_psin;
            if(oz>fcull) continue;
        }
        char *mtex=mdl->tex;
        int   mtw=mdl->tw,mth=mdl->th;
        char *mmip=mdl->mip;
        int   mmw=mdl->mw,mmh=mdl->mh;
        int has_tex=(mtex!=NULL&&mdl->nuv>0);
        // Animation frames to blend (vertex morphing). For static
        // models fr0==fr1 and blend=0, so it's a direct read with no cost.
        V3   *fr0=mdl->vframes+(long)inst->fa*mdl->nv;
        V3   *fr1=mdl->vframes+(long)inst->fb*mdl->nv;
        float blend=inst->fblend;
        int   morph=(mdl->nframes>1&&blend>0.0f);
        // Transform each vertex: local (Y rotation + translation) -> camera
        for(i=0;i<mdl->nv;i++){
            float vx,vy,vz;
            if(morph){
                vx=fr0[i].x+(fr1[i].x-fr0[i].x)*blend;
                vy=fr0[i].y+(fr1[i].y-fr0[i].y)*blend;
                vz=fr0[i].z+(fr1[i].z-fr0[i].z)*blend;
            } else {
                vx=fr0[i].x;vy=fr0[i].y;vz=fr0[i].z;
            }
            float wx=vx*iyc-vz*iys+ix;
            float wz=vx*iys+vz*iyc+iz;
            float wy=vy+iy;
            float dx=wx-g_Cx,dy=wy-g_Cy,dz=wz-g_Cz;
            float rx=dx*g_ysin+dz*g_ycos;
            float rz=dx*g_ycos-dz*g_ysin;
            float ocx=rx;
            float ocy=dy*g_pcos-rz*g_psin;
            float ocz=dy*g_psin+rz*g_pcos;
            obj9cv[i][0]=ocx;obj9cv[i][1]=ocy;obj9cv[i][2]=ocz;
            if(ocz>=NEAR_PLANE){
                float inv=fast_recip(ocz);
                obj9px[i]=g_hw+ocx*g_focal*inv;
                obj9py[i]=g_hh-ocy*g_focal*inv;
                obj9bh[i]=0;
            } else obj9bh[i]=1;
        }
        // Cull and prepare the sort (near cull + backface)
        for(ti=0;ti<mdl->nt&&ndraw<MAX_TRIS;ti++){
            int a=mdl->tris[ti].a,b=mdl->tris[ti].b,c=mdl->tris[ti].c;
            float za=obj9cv[a][2],zb=obj9cv[b][2],zc=obj9cv[c][2];
            if(za<NEAR_PLANE&&zb<NEAR_PLANE&&zc<NEAR_PLANE) continue;
            if(!obj9bh[a]&&!obj9bh[b]&&!obj9bh[c]){
                if(cull_mode){
                    float area=(obj9px[b]-obj9px[a])*(obj9py[c]-obj9py[a])
                              -(obj9px[c]-obj9px[a])*(obj9py[b]-obj9py[a]);
                    if(cull_mode==1&&area>=0.0f) continue;
                    if(cull_mode==2&&area<=0.0f) continue;
                }
                tsort[ndraw].ci=0;
            } else tsort[ndraw].ci=1;
            tsort[ndraw].depth=(za+zb+zc)*(1.0f/3.0f);
            tsort[ndraw].idx=ti;
            ndraw++;
        }
        if(!ndraw) continue;
        // Bucket sort front-to-back (reuses bcnt[]/order[])
        {
            int j,bk; float dmin=1e30f,dmax=-1e30f,inv;
            for(j=0;j<ndraw;j++){float d=tsort[j].depth;if(d<dmin)dmin=d;if(d>dmax)dmax=d;}
            if(dmax<=dmin){ for(j=0;j<ndraw;j++)order[j]=j; }
            else {
                inv=(float)(NBUCKETS-1)/(dmax-dmin);
                for(j=0;j<=NBUCKETS;j++)bcnt[j]=0;
                for(j=0;j<ndraw;j++){
                    bk=(int)((tsort[j].depth-dmin)*inv);
                    if(bk<0)bk=0;else if(bk>=NBUCKETS)bk=NBUCKETS-1;
                    bcnt[bk+1]++;
                }
                for(j=1;j<=NBUCKETS;j++)bcnt[j]+=bcnt[j-1];
                for(j=0;j<ndraw;j++){
                    bk=(int)((tsort[j].depth-dmin)*inv);
                    if(bk<0)bk=0;else if(bk>=NBUCKETS)bk=NBUCKETS-1;
                    order[bcnt[bk]++]=j;
                }
            }
        }
        // Rasterize (fast path + clip path, identical to the world)
        for(i=0;i<ndraw;i++){
            int oi=order[i],ti2=tsort[oi].idx;
            int a=mdl->tris[ti2].a,b=mdl->tris[ti2].b,c=mdl->tris[ti2].c;
            int textured=has_tex&&(mdl->tris[ti2].ua>=0);
            unsigned char flatcol=textured?0:mdl->tris[ti2].flat;
            if(tsort[oi].ci==0){
                // ---- Fast path: whole tri in front of the near ----
                float u0=0,v0=0,u1=0,v1=0,u2=0,v2=0;
                char *utex=mtex;int utw=mtw,uth=mth;
                float ax=obj9px[a],ay=obj9py[a],bx=obj9px[b],by=obj9py[b],cx2=obj9px[c],cy2=obj9py[c];
                {int mnx=(int)ax,mxx=(int)ax,mny=(int)ay,mxy=(int)ay;
                 if((int)bx<mnx)mnx=(int)bx;if((int)bx>mxx)mxx=(int)bx;
                 if((int)cx2<mnx)mnx=(int)cx2;if((int)cx2>mxx)mxx=(int)cx2;
                 if((int)by<mny)mny=(int)by;if((int)by>mxy)mxy=(int)by;
                 if((int)cy2<mny)mny=(int)cy2;if((int)cy2>mxy)mxy=(int)cy2;
                 if(mxx<clip_x0||mnx>=clip_x1||mxy<clip_y0||mny>=clip_y1) continue;}
                if(textured){
                    u0=mdl->uvs[mdl->tris[ti2].ua].u;v0=mdl->uvs[mdl->tris[ti2].ua].v;
                    u1=mdl->uvs[mdl->tris[ti2].ub].u;v1=mdl->uvs[mdl->tris[ti2].ub].v;
                    u2=mdl->uvs[mdl->tris[ti2].uc].u;v2=mdl->uvs[mdl->tris[ti2].uc].v;
                    if(mmip){
                        long sa=(long)((bx-ax)*(cy2-ay)-(cx2-ax)*(by-ay));
                        float uvx=(u1-u0)*(v2-v0)-(u2-u0)*(v1-v0);
                        float texA,scrA;
                        if(sa<0)sa=-sa;if(uvx<0.0f)uvx=-uvx;
                        texA=uvx*(float)mtw*(float)mth;
                        scrA=(float)sa*0.5f;if(scrA<1.0f)scrA=1.0f;
                        if(texA>scrA*4.0f){utex=mmip;utw=mmw;uth=mmh;}
                    }
                }
                raster_tri_z(vid,zbuf,W,H,
                    ax,ay,u0,v0,obj9cv[a][2],
                    bx,by,u1,v1,obj9cv[b][2],
                    cx2,cy2,u2,v2,obj9cv[c][2],
                    textured?utex:NULL,utw,uth,flatcol);
            } else {
                // ---- Clip path: crosses the near plane ----
                CVert in[8],out[10];
                int nin=3,nout=0,k;
                float sxf[10],syf[10],sw[10];
                int mnx=999999,mxx=-999999,mny=999999,mxy=-999999;
                in[0].cx=obj9cv[a][0];in[0].cy=obj9cv[a][1];in[0].cz=obj9cv[a][2];
                in[1].cx=obj9cv[b][0];in[1].cy=obj9cv[b][1];in[1].cz=obj9cv[b][2];
                in[2].cx=obj9cv[c][0];in[2].cy=obj9cv[c][1];in[2].cz=obj9cv[c][2];
                if(textured){
                    in[0].u=mdl->uvs[mdl->tris[ti2].ua].u;in[0].v=mdl->uvs[mdl->tris[ti2].ua].v;
                    in[1].u=mdl->uvs[mdl->tris[ti2].ub].u;in[1].v=mdl->uvs[mdl->tris[ti2].ub].v;
                    in[2].u=mdl->uvs[mdl->tris[ti2].uc].u;in[2].v=mdl->uvs[mdl->tris[ti2].uc].v;
                } else {
                    in[0].u=in[0].v=in[1].u=in[1].v=in[2].u=in[2].v=0.0f;
                }
                for(k=0;k<nin;k++){
                    CVert *cur=&in[k],*nxt=&in[(k+1)%nin];
                    int curIn=(cur->cz>=NEAR_PLANE),nxtIn=(nxt->cz>=NEAR_PLANE);
                    if(curIn)out[nout++]=*cur;
                    if(curIn!=nxtIn&&nout<10){
                        float t=(NEAR_PLANE-cur->cz)/(nxt->cz-cur->cz);
                        out[nout].cx=cur->cx+(nxt->cx-cur->cx)*t;
                        out[nout].cy=cur->cy+(nxt->cy-cur->cy)*t;
                        out[nout].cz=NEAR_PLANE;
                        out[nout].u =cur->u +(nxt->u -cur->u )*t;
                        out[nout].v =cur->v +(nxt->v -cur->v )*t;
                        nout++;
                    }
                }
                if(nout<3)continue;
                for(k=0;k<nout;k++){
                    float w=out[k].cz;if(w<NEAR_PLANE)w=NEAR_PLANE;
                    float inv=fast_recip(w);
                    int ix2,iy2;
                    sxf[k]=g_hw+out[k].cx*g_focal*inv;
                    syf[k]=g_hh-out[k].cy*g_focal*inv;
                    sw[k]=w;
                    ix2=fast_ftol(sxf[k]);iy2=fast_ftol(syf[k]);
                    if(ix2<mnx)mnx=ix2;if(ix2>mxx)mxx=ix2;
                    if(iy2<mny)mny=iy2;if(iy2>mxy)mxy=iy2;
                }
                if(mxx<clip_x0||mnx>=clip_x1||mxy<clip_y0||mny>=clip_y1)continue;
                for(k=1;k<nout-1;k++){
                    raster_tri_z(vid,zbuf,W,H,
                        sxf[0],syf[0],out[0].u,out[0].v,sw[0],
                        sxf[k],syf[k],out[k].u,out[k].v,sw[k],
                        sxf[k+1],syf[k+1],out[k+1].u,out[k+1].v,sw[k+1],
                        textured?mtex:NULL,mtw,mth,flatcol);
                }
            }
        }
    }
}

// Renders ONE view (viewport) into the buffer. The z-buffer/buffer use the full
// width W as stride; the drawing is limited to the viewport rect (clip_*).
static void render_view(M9View *v,char *vid,int W,int H){
    float Cx,Cy,Cz,yaw,pitch,ysin,ycos,psin,pcos;
    float focal,hw,hh,fcull;
    int i,ndraw,cbase;
    int vx,vy,vw,vh;

    // Resolve the viewport rect (vw/vh==0 -> full screen)
    vx=v->vx; vy=v->vy; vw=v->vw; vh=v->vh;
    if(vw<=0){vx=0;vw=W;} if(vh<=0){vy=0;vh=H;}
    if(vx<0){vw+=vx;vx=0;} if(vy<0){vh+=vy;vy=0;}
    if(vx+vw>W)vw=W-vx; if(vy+vh>H)vh=H-vy;
    if(vw<=0||vh<=0)return;
    clip_x0=vx; clip_x1=vx+vw; clip_y0=vy; clip_y1=vy+vh;

    if(!get_camera(v->cam,v->bind,&Cx,&Cy,&Cz,&yaw,&pitch,&cbase))return;

    // --- Collision: push the camera out of the walls and rewrite its
    //     position in the process (so advance/xadvance "don't go through" walls) --
    if(m9_collision && cbase>=0 && (n_walls>0 || (m9_step>0.0f && n_floors>0) || n_ceils>0)){
        float r=(float)mem[cbase+P_RADIUS]/m9_unitscale;
        float feet=(float)mem[cbase+P_Z]/m9_unitscale;   // player feet (OBJ units)
        float eyeoff=Cy-feet;                             // eye above the feet
        if(r<=0.0f) r=(float)COLLIDE_DEF/m9_unitscale;
        // Walls: sliding (those <= m9_step tall don't block: they're steps
        // the floor-follow climbs). Anti-tunneling sweep before the sliding.
        if(n_walls>0){
            if(v->haslast) collide_swept(v->lastx,v->lastz,&Cx,&Cz,Cy,r,feet,m9_step);
            collide_xz(&Cx,&Cz,Cy,r,feet,m9_step);
            v->lastx=Cx; v->lastz=Cz; v->haslast=1;
        }
        // Floor-follow + HEAD collision. Floor-follow raises the feet onto the
        // floor below (steps/ramps, <= m9_step; it does NOT lower the feet, that's
        // the PRG's gravity). Then the ceiling caps the feet so the head
        // (feet+PLAYER_BODY) can't go through a roof: that's what stops you from
        // jumping THROUGH the ceiling with collisions on. If either touched the
        // feet, re-derive the eye (Cy) and rewrite z once.
        {
            int feetadj=0;
            if(m9_step>0.0f && n_floors>0){
                float fy;
                if(floor_height(Cx,Cz,feet+m9_step,&fy) && feet<fy) feet=fy;
                feetadj=1;
            }
            if(n_ceils>0){
                float cyl;
                if(ceil_height(Cx,Cz,feet+0.1f,&cyl)){
                    float cap=cyl-PLAYER_BODY;        // highest feet keeping the head under
                    if(feet>cap){ feet=cap; feetadj=1; }
                }
            }
            if(feetadj){
                Cy=feet+eyeoff;
                {float fz=feet*m9_unitscale; mem[cbase+P_Z]=(int)(fz>=0?fz+0.5f:fz-0.5f);}
            }
        }
        {float fx=Cx*m9_unitscale, fz=Cz*m9_unitscale;
         mem[cbase+P_X]=(int)(fx>=0?fx+0.5f:fx-0.5f);
         mem[cbase+P_Y]=(int)(fz>=0?fz+0.5f:fz-0.5f);}
    }

    {int yi=(int)(yaw*TRIG_K)&TRIG_MASK; ysin=sin_tab[yi]; ycos=cos_tab[yi];}
    {int pi2=(int)(pitch*TRIG_K)&TRIG_MASK; psin=sin_tab[pi2]; pcos=cos_tab[pi2];}
    focal=(float)vw*m9_focal_k; hw=(float)vx+(float)vw*0.5f; hh=(float)vy+(float)vh*0.5f;

    // Cut-off distance (far clip): far_clip and, if there's fog, fog_far (what's
    // behind the fog is background color, no need to draw it).
    fcull=far_clip;
    if(fog_on&&fog_far>0.0f){ if(fcull<=0.0f||fog_far<fcull) fcull=fog_far; }

    // --- Background (ambient color) limited to the viewport rect ------------
    // With line doubler: only clear the lines that will be rendered; the
    // rest are duplicated later. We use memset (faster than the manual loop).
    {
        int step=m9_downscale>1?m9_downscale:1;
        if(sky_on && sky_data && vw<=SKY_MAXDIM && vh<=SKY_MAXDIM){
            render_sky(vx,vy,vw,vh,focal,hh,yaw,pitch,vid,W,step);   // panoramic sky
        } else {
            unsigned char bg=(unsigned char)PAL_ENV;
            int yy;
            for(yy=vy; yy<vy+vh; yy+=step)
                memset((unsigned char*)vid+(long)yy*W+vx, (int)bg, (size_t)vw);  // flat background
        }
    }

    // Z-buffer limited to the viewport rect (clear to 0 = far)
    {int yy, step=m9_downscale>1?m9_downscale:1;
     for(yy=vy; yy<vy+vh; yy+=step)
         memset(zbuf+(long)yy*W+vx, 0, (size_t)vw*2);
    }

    // 1. Save this view's camera params (g_*) for the LAZY
    //    vertex transform, and increment the frame stamp
    //    (vstamp and tstamp). Each vertex is transformed on demand the first time
    //    a visible triangle needs it (xform_vertex from consider_tri),
    //    so the vertices of cells discarded by the grid aren't even touched.
    g_Cx=Cx;g_Cy=Cy;g_Cz=Cz; g_ysin=ysin;g_ycos=ycos;g_psin=psin;g_pcos=pcos;
    g_focal=focal;g_hw=hw;g_hh=hh;
    frame_stamp++;

    // 2. Sort: near cull + far clip + backface + FRUSTUM culling. tsort.ci marks
    //    whether the triangle crosses the near (needs_clip) for the rasterizing.
    //    With grid (grid_n): only the triangles of the cells inside the horizontal
    //    frustum + far clip are considered (Tier 3). Without grid: all of them.
    ndraw=0;
    if(grid_n){
        int gx,gz;
        float fk=0.5f/m9_focal_k;     // tan(half horizontal FOV)
        for(gz=0;gz<GRID_DIM;gz++){
            float ccz=grid_minz+((float)gz+0.5f)*grid_ch;
            for(gx=0;gx<GRID_DIM;gx++){
                int cell=gz*GRID_DIM+gx, s=grid_start[cell], e=grid_start[cell+1], k;
                float ccx,dx,dz,rx,rz,lim;
                if(s>=e) continue;                 // empty cell
                ccx=grid_minx+((float)gx+0.5f)*grid_cw;
                dx=ccx-Cx; dz=ccz-Cz;
                rx=dx*ysin+dz*ycos;                // camera X axis (yaw, without pitch)
                rz=dx*ycos-dz*ysin;                // forward (the pitch doesn't affect XZ)
                if(rz < -grid_cellrad) continue;   // cell entirely behind
                if(fcull>0.0f && rz-grid_cellrad>fcull) continue;   // beyond the far clip
                lim=fk*(rz+grid_cellrad)+grid_cellrad;              // horizontal frustum
                if(rx>lim||-rx>lim) continue;
                for(k=s;k<e;k++){
                    int t=grid_items[k];
                    if(tstamp[t]==frame_stamp) continue;   // already processed (another cell)
                    tstamp[t]=frame_stamp;
                    consider_tri(t,fcull,&ndraw);
                }
            }
        }
    } else {
        for(i=0;i<n_tris;i++) consider_tri(i,fcull,&ndraw);
    }

    // 3. BUCKET SORT front-to-back in O(n): quantizes the depth into NBUCKETS
    //    buckets and sorts by count (no comparisons). With z-buffer the EXACT
    //    order within a bucket is irrelevant (the z-buffer decides the
    //    occlusion); a coarse near->far order is enough for the z-test
    //    to reject covered things early. Scales O(n) vs the O(n^2) of insertion.
    {
        int j,bk;
        float dmin=1e30f,dmax=-1e30f,inv;
        for(j=0;j<ndraw;j++){
            float d=tsort[j].depth;
            if(d<dmin)dmin=d; if(d>dmax)dmax=d;
        }
        if(dmax<=dmin){
            for(j=0;j<ndraw;j++)order[j]=j;
        } else {
            inv=(float)(NBUCKETS-1)/(dmax-dmin);
            for(j=0;j<=NBUCKETS;j++)bcnt[j]=0;
            for(j=0;j<ndraw;j++){
                bk=(int)((tsort[j].depth-dmin)*inv);
                if(bk<0)bk=0; else if(bk>=NBUCKETS)bk=NBUCKETS-1;
                bcnt[bk+1]++;
            }
            for(j=1;j<=NBUCKETS;j++)bcnt[j]+=bcnt[j-1];   // prefix -> start of each bucket
            for(j=0;j<ndraw;j++){
                bk=(int)((tsort[j].depth-dmin)*inv);
                if(bk<0)bk=0; else if(bk>=NBUCKETS)bk=NBUCKETS-1;
                order[bcnt[bk]++]=j;
            }
        }
    }

    // 4. Rasterize. FAST path if the tri doesn't cross the near (majority): uses the
    //    projection already computed, no clipping. If it crosses, Sutherland-H clip.
    for(i=0;i<ndraw;i++){
        int oi=order[i];
        int ti=tsort[oi].idx;
        int a=mesh_t[ti].a,b=mesh_t[ti].b,c=mesh_t[ti].c;
        // Texture selection by material: if the tri's slot has no texture,
        // it falls back to slot 0 (default texture loaded in LOAD_WLD9). Cost: 1 check
        // per triangle, outside the pixel loop (zero performance impact).
        {int mat=(int)mesh_t[ti].mat;
        char *mtex=mat_data[mat]?mat_data[mat]:mat_data[0];
        int   mtw =mat_data[mat]?mat_w[mat]:mat_w[0];
        int   mth =mat_data[mat]?mat_h[mat]:mat_h[0];
        char *mmip=mat_data[mat]?mat_mip[mat]:mat_mip[0];
        int   mmw =mat_data[mat]?matm_w[mat]:matm_w[0];
        int   mmh =mat_data[mat]?matm_h[mat]:matm_h[0];
        int textured=(mtex&&has_uvs&&mesh_t[ti].ua>=0&&mesh_t[ti].ub>=0&&mesh_t[ti].uc>=0);
        unsigned char flatcol = textured?0:mesh_t[ti].flat;

        if(tsort[oi].ci==0){
            // ---- Fast path: whole triangle in front of the near ----
            int ax=psx[a],ay=psy[a],bx=psx[b],by=psy[b],cx2=psx[c],cy2=psy[c];
            int mnx,mxx,mny,mxy;
            float u0=0,v0=0,u1=0,v1=0,u2=0,v2=0;
            mnx=ax; if(bx<mnx)mnx=bx; if(cx2<mnx)mnx=cx2;
            mxx=ax; if(bx>mxx)mxx=bx; if(cx2>mxx)mxx=cx2;
            mny=ay; if(by<mny)mny=by; if(cy2<mny)mny=cy2;
            mxy=ay; if(by>mxy)mxy=by; if(cy2>mxy)mxy=cy2;
            if(mxx<clip_x0||mnx>=clip_x1||mxy<clip_y0||mny>=clip_y1){continue;}
            {
            char *utex=mtex; int utw=mtw,uth=mth;
            if(textured){
                u0=mesh_uv[mesh_t[ti].ua].u; v0=mesh_uv[mesh_t[ti].ua].v;
                u1=mesh_uv[mesh_t[ti].ub].u; v1=mesh_uv[mesh_t[ti].ub].v;
                u2=mesh_uv[mesh_t[ti].uc].u; v2=mesh_uv[mesh_t[ti].uc].v;
                // LOD: if the triangle minifies the texture a lot, use the mipmap.
                if(mmip){
                    long sa=(long)(bx-ax)*(long)(cy2-ay)-(long)(cx2-ax)*(long)(by-ay);
                    float uvx=(u1-u0)*(v2-v0)-(u2-u0)*(v1-v0);
                    float texA,scrA;
                    if(sa<0)sa=-sa; if(uvx<0.0f)uvx=-uvx;
                    texA=uvx*(float)mtw*(float)mth;
                    scrA=(float)sa*0.5f; if(scrA<1.0f)scrA=1.0f;
                    if(texA>scrA*4.0f){utex=mmip;utw=mmw;uth=mmh;}
                }
            }
            raster_tri_z(vid,zbuf,W,H,
                psxf[a],psyf[a],u0,v0,camv[a][2],
                psxf[b],psyf[b],u1,v1,camv[b][2],
                psxf[c],psyf[c],u2,v2,camv[c][2],
                textured?utex:0, utw, uth, flatcol);
            }
            continue;
        }

        // ---- Clip path: the triangle crosses the near plane ----
        {
        CVert in[8],out[10];
        int nin=3,nout=0,k;
        float sxf[10],syf[10],sw[10];   // SUBPIXEL screen (for the rasterizing)
        int mnx=999999,mxx=-999999,mny=999999,mxy=-999999;
        in[0].cx=camv[a][0];in[0].cy=camv[a][1];in[0].cz=camv[a][2];
        in[1].cx=camv[b][0];in[1].cy=camv[b][1];in[1].cz=camv[b][2];
        in[2].cx=camv[c][0];in[2].cy=camv[c][1];in[2].cz=camv[c][2];
        if(textured){
            in[0].u=mesh_uv[mesh_t[ti].ua].u; in[0].v=mesh_uv[mesh_t[ti].ua].v;
            in[1].u=mesh_uv[mesh_t[ti].ub].u; in[1].v=mesh_uv[mesh_t[ti].ub].v;
            in[2].u=mesh_uv[mesh_t[ti].uc].u; in[2].v=mesh_uv[mesh_t[ti].uc].v;
        }else{
            in[0].u=in[0].v=in[1].u=in[1].v=in[2].u=in[2].v=0.0f;
        }
        for(k=0;k<nin;k++){
            CVert *cur=&in[k], *nxt=&in[(k+1)%nin];
            int curIn=(cur->cz>=NEAR_PLANE), nxtIn=(nxt->cz>=NEAR_PLANE);
            if(curIn) out[nout++]=*cur;
            if(curIn!=nxtIn && nout<10){
                float t=(NEAR_PLANE-cur->cz)/(nxt->cz-cur->cz);
                out[nout].cx=cur->cx+(nxt->cx-cur->cx)*t;
                out[nout].cy=cur->cy+(nxt->cy-cur->cy)*t;
                out[nout].cz=NEAR_PLANE;
                out[nout].u =cur->u +(nxt->u -cur->u )*t;
                out[nout].v =cur->v +(nxt->v -cur->v )*t;
                nout++;
            }
        }
        if(nout<3)continue;
        for(k=0;k<nout;k++){
            float w=out[k].cz; if(w<NEAR_PLANE)w=NEAR_PLANE;
            float inv=fast_recip(w);
            int ix,iy;
            sxf[k]=hw + out[k].cx*focal*inv;     // subpixel (float)
            syf[k]=hh - out[k].cy*focal*inv;
            sw[k] =w;
            ix=fast_ftol(sxf[k]); iy=fast_ftol(syf[k]);   // integer only for the bbox
            if(ix<mnx)mnx=ix; if(ix>mxx)mxx=ix;
            if(iy<mny)mny=iy; if(iy>mxy)mxy=iy;
        }
        if(mxx<clip_x0||mnx>=clip_x1||mxy<clip_y0||mny>=clip_y1)continue;
        for(k=1;k<nout-1;k++){
            raster_tri_z(vid,zbuf,W,H,
                sxf[0],syf[0],out[0].u,out[0].v,sw[0],
                sxf[k],syf[k],out[k].u,out[k].v,sw[k],
                sxf[k+1],syf[k+1],out[k+1].u,out[k+1].v,sw[k+1],
                textured?mtex:0, mtw, mth, flatcol);
        }
        }
        }   // close the {int mat=...} block
    }

    // --- 3D objects (DRAW_OBJ9): render after the world so the
    //     z-buffer already has the static geometry and the occlusion is correct. ---
    render_obj9_instances(vid,W,H,fcull);

    // --- Billboards drawn by the DLL in THIS viewport (multi-view) -------
    // Only if there's an FPG loaded in the DLL (MODE9_LOADFPG). Each c_m9 process is
    // projected with THIS view's camera and drawn with per-pixel occlusion.
    // The marker of the view's own camera isn't drawn.
    if(n_fpgspr>0){
        int camid = v->bind ? mem[v->bind+1] : v->cam;
        int first=*_id_init_offset, last=*_id_end_offset, step=*_process_size, b;
        if(step>0) for(b=first;b<=last;b+=step){
            int gr,si;
            float Px,Py,Pz,ocx,ocy,ocz,inv;
            if(mem[b+P_CTYPE]!=m9_ctype) continue;
            if(mem[b+P_ID]==0||mem[b+P_STATUS]==0||mem[b+P_ID]==camid) continue;
            Px=(float)mem[b+P_X]/m9_unitscale; Pz=(float)mem[b+P_Y]/m9_unitscale;
            Py=(float)mem[b+P_Z]/m9_unitscale;
            world_to_cam(Px,Py,Pz,Cx,Cy,Cz,ysin,ycos,psin,pcos,&ocx,&ocy,&ocz);
            if(ocz<NEAR_PLANE) continue;
            if(fcull>0.0f && ocz>fcull) continue;   // far clip: same as the geometry
            // graphic: angular (xgraph) by THIS view, or the process's graph
            {int xg=mem[b+P_XGRAPH];
             if(xg&&mem[xg]>0){
                 int n=mem[xg],sector; float oa,va,rel,tp=6.2831853f;
                 oa=(float)mem[b+P_ANGLE]*(3.14159265f/180000.0f);
                 va=(float)atan2((double)(Pz-Cz),(double)(Cx-Px));
                 rel=va-oa; rel=rel-tp*(float)floor((double)(rel/tp));
                 sector=(int)(rel/tp*(float)n+0.5f);
                 if(sector>=n)sector-=n; if(sector<0)sector+=n;
                 gr=mem[xg+1+sector];
             } else gr=mem[b+P_GRAPH];
            }
            si=find_sprite(gr); if(si<0) continue;
            inv=fast_recip(ocz);
            {int scx=fast_ftol(hw+ocx*focal*inv), scy=fast_ftol(hh-ocy*focal*inv);
             // The size must scale with the resolution like the geometry
             // (which projects with focal=vw*m9_focal_k). 320 = reference width
             // where size=100 is calibrated. Without this, the billboards shrink
             // (in fixed pixels) when the resolution goes up.
             int sp9=fast_ftol(100.0f*inv*((float)vw/320.0f)); int fin,zfix;
             int basesize=mem[b+P_SIZE];
             int flv; char *fm;
             if(sp9<1)sp9=1;
             fin=(sp9*basesize)/100; if(fin<1)fin=1;
             zfix=fast_ftol(inv*ZSCALE); if(zfix<0)zfix=0; else if(zfix>65535)zfix=65535;
             // Sprite fog by its distance (ocz), like the geometry.
             flv=fog_on?fog_level(ocz):0;
             fm=(flv>0)?&fog_map[flv*256]:0;
             blit_sprite(si,scx,scy,fin,zfix,vid,W,fm);
            }
        }
    }
    // --- Line doubler: copy each rendered line to the next q-1 -----
    // Done AFTER the billboards (which already write with correct z-test on
    // the even lines). Only the video buffer; the z-buffer is left with gaps
    // but is cleared at the start of the next frame, doesn't matter.
    if(m9_downscale>1){
        int yy,qq;
        for(yy=vy; yy<vy+vh; yy+=m9_downscale){
            unsigned char *src=(unsigned char*)vid+(long)yy*W+vx;
            for(qq=1; qq<m9_downscale && yy+qq<vy+vh; qq++)
                memcpy((unsigned char*)vid+(long)(yy+qq)*W+vx, src, (size_t)vw);
        }
    }
    // Note: the palette is NOT resent each frame (set_palette only in the setters).
}

// do_render: prepares the buffer and the z-buffer and renders all the active
// views (1 = full screen; >1 = split-screen).
static void do_render(void){
    int W,H,vi;
    char *vid;
    if(!m9_active||!mesh_ok||!_buffer||!_wide||!_height)return;
    vid=(char*)(*((unsigned long*)_buffer));
    if(!vid)return;
    W=*_wide;H=*_height;
    if(W<=0||H<=0)return;

    // Z-buffer (16 bits) at full-screen size; each view clears its rect.
    if(!zbuf||zbuf_w!=W||zbuf_h!=H){
        if(zbuf)div_free(zbuf);
        zbuf=(unsigned short*)div_malloc((size_t)((long)W*(long)H*(long)sizeof(unsigned short)));
        zbuf_w=W;zbuf_h=H;
    }
    if(!zbuf)return;

    for(vi=0; vi<MAX_VIEWS; vi++)
        if(views[vi].active) render_view(&views[vi],vid,W,H);
    n_obj9_queue=0;   // clear the queue after rendering all viewports
}

// ============================================================================
// MODE9_BILLBOARD / MODE9_PROJECT
// ============================================================================
// Projects (wx,wy,wz) DIV2 units to screen. Returns 1 visible (writes sx,sy and
// sz=depth in DIV2 units, szpct=scale%) or 0 if behind the camera.
static int project_world(int wx,int wy,int wz,int *sx,int *sy,int *depthu,int *sizepct){
    float Cx,Cy,Cz,yaw,pitch,ysin,ycos,psin,pcos;
    float Px,Py,Pz,ocx,ocy,ocz,focal,hw,hh,w;
    int W,H;
    if(!m9_active||!_wide||!_height)return 0;
    W=*_wide;H=*_height;
    if(!get_camera(m9_camera,m9_bind,&Cx,&Cy,&Cz,&yaw,&pitch,0))return 0;
    {int yi=(int)(yaw*TRIG_K)&TRIG_MASK; ysin=sin_tab[yi]; ycos=cos_tab[yi];}
    {int pi2=(int)(pitch*TRIG_K)&TRIG_MASK; psin=sin_tab[pi2]; pcos=cos_tab[pi2];}
    Px=(float)wx/m9_unitscale; Pz=(float)wy/m9_unitscale; Py=(float)wz/m9_unitscale;
    world_to_cam(Px,Py,Pz,Cx,Cy,Cz,ysin,ycos,psin,pcos,&ocx,&ocy,&ocz);
    if(ocz<NEAR_PLANE)return 0;
    // Far clip: hidden (same limit as the world geometry; with active fog
    // it also clips at fog_far, what's behind is the background color).
    {float fcull=far_clip;
     if(fog_on&&fog_far>0.0f){ if(fcull<=0.0f||fog_far<fcull) fcull=fog_far; }
     if(fcull>0.0f && ocz>fcull) return 0;}
    focal=(float)W*m9_focal_k; hw=(float)W*0.5f; hh=(float)H*0.5f;
    w=ocz;
    *sx=(int)(hw+ocx*focal/w);
    *sy=(int)(hh-ocy*focal/w);
    *depthu=(int)(w*m9_unitscale);
    // scale: 100% at 1 OBJ unit of distance (adjustable with basesize). Scales
    // with the resolution (320 = reference width) to not shrink at higher res.
    {int s=(int)(100.0f/w*((float)W/320.0f)+0.5f); if(s<1)s=1; if(s>2000)s=2000; *sizepct=s;}

    // OCCLUSION against the z-buffer: if the sprite's pixel has closer
    // geometry, the sprite is behind a wall -> hidden (mode 7/Doom style).
    // Uses the PREVIOUS frame's z-buffer (filled in do_render, which runs
    // after the process logic): 1 frame of lag, imperceptible.
    if(m9_occlude && zbuf && zbuf_w==W && zbuf_h==H){
        int ix=*sx, iy=*sy;
        if(ix>=0&&ix<W&&iy>=0&&iy<H){
            int zspr=(int)((1.0f/w)*ZSCALE);
            int zgeo=(int)zbuf[iy*W+ix];
            // ~3% bias so a sprite stuck to a surface doesn't flicker
            if(zgeo > zspr + (zspr>>5)) return 0;
        }
    }
    return 1;
}

// If the process has xgraph (angular graphics table, like in mode 7),
// it picks the graphic by the angle from which the camera sees the sprite.
// Table: mem[xg]=N (num graphics), mem[xg+1..xg+N]=codes spread over 360deg.
static void pick_angular_graph(int base,int wx,int wy){
    int xg=mem[base+P_XGRAPH];
    int n,sector;
    float Cx,Cy,Cz,yaw,pitch,Pxw,Pzw,objang,viewang,rel,twopi;
    if(!xg)return;
    n=mem[xg];
    if(n<=0)return;
    if(!get_camera(m9_camera,m9_bind,&Cx,&Cy,&Cz,&yaw,&pitch,0))return;
    Pxw=(float)wx/m9_unitscale; Pzw=(float)wy/m9_unitscale;
    objang =(float)mem[base+P_ANGLE]*(3.14159265f/180000.0f);
    // angle (DIV2 convention) of the sprite->camera direction
    viewang=(float)atan2((double)(Pzw-Cz),(double)(Cx-Pxw));
    rel=viewang-objang;
    twopi=6.2831853f;
    rel=rel-twopi*(float)floor((double)(rel/twopi));   // normalize to [0,2pi)
    sector=(int)(rel/twopi*(float)n+0.5f);
    if(sector>=n)sector-=n; if(sector<0)sector+=n;
    mem[base+P_GRAPH]=mem[xg+1+sector];
}

static void mode9_billboard_fn(void){
    int basesize=getparm(),wz=getparm(),wy=getparm(),wx=getparm(),pid=getparm();
    int sx,sy,depthu,sizepct,base;
    base=find_process(pid);
    if(base<0){retval(0);return;}
    if(!project_world(wx,wy,wz,&sx,&sy,&depthu,&sizepct)){
        mem[base+P_SIZE]=0;          // behind the camera or hidden by geometry
        retval(0);return;
    }
    mem[base+P_X]=sx;
    mem[base+P_Y]=sy;
    mem[base+P_SIZE]=(sizepct*basesize)/100;
    if(mem[base+P_SIZE]<1)mem[base+P_SIZE]=1;
    mem[base+P_Z]=depthu;            // greater depth -> drawn behind
    pick_angular_graph(base,wx,wy);  // graphic by angle (if there's xgraph)
    retval(1);
}

// MODE9_OCCLUDE(on): 1=hide billboards covered by geometry (def), 0=off.
static void mode9_occlude_fn(void){
    m9_occlude=getparm()?1:0;
    retval(0);
}
// MODE9_CTYPE(n): value of the c_m9 constant (automatic billboard ctype).
// Must match the PRG's CONST c_m9. Default 200.
static void mode9_ctype_fn(void){
    int n=getparm();
    if(n>2) m9_ctype=n;   // don't allow colliding with c_screen/scroll/m7
    retval(0);
}

// ============================================================================
// Automatic billboards (ctype = c_m9), mode-7 c_m7 style:
// the user only sets ctype=c_m9 and x,y,z (WORLD), size (scale %), graph and
// angle in the process; does NOT call anything each frame. After painting the 3D scenario
// all c_m9 processes are scanned, projected and converted to c_screen
// (with screen x,y, size scaled by distance and z=depth) so that
// DIV2 draws them with its own scaler. After painting their world
// coordinates and c_m9 ctype are restored (restore_m9_sprites in post_process_buffer).
// ============================================================================
static void process_m9_sprites(void){
    int first,last,step,b;
    n_m9spr=0;
    if(!m9_active) return;
    step=*_process_size; if(step<=0) return;
    first=*_id_init_offset; last=*_id_end_offset;
    for(b=first; b<=last; b+=step){
        if(mem[b+P_CTYPE]!=m9_ctype) continue;
        if(mem[b+P_ID]==0 || mem[b+P_STATUS]==0) continue;   // free/dead slot
        if(n_m9spr>=MAX_SPR) break;
        {
            int wx=mem[b+P_X], wy=mem[b+P_Y], wz=mem[b+P_Z];
            int basesize=mem[b+P_SIZE];
            int sx,sy,depthu,sizepct;
            // save the world state to restore it after painting
            m9spr[n_m9spr].base=b;
            m9spr[n_m9spr].wx=wx; m9spr[n_m9spr].wy=wy; m9spr[n_m9spr].wz=wz;
            m9spr[n_m9spr].sz=basesize; m9spr[n_m9spr].gr=mem[b+P_GRAPH];
            m9spr[n_m9spr].ct=mem[b+P_CTYPE];
            n_m9spr++;
            // convert to c_screen so DIV2 paints it as a 2D sprite
            mem[b+P_CTYPE]=C_SCREEN;
            if(!project_world(wx,wy,wz,&sx,&sy,&depthu,&sizepct)){
                mem[b+P_SIZE]=0;            // behind the camera or hidden -> don't paint
            } else {
                int s=(sizepct*basesize)/100;   // scale = distance * size_base%
                if(s<1)s=1;
                mem[b+P_X]=sx; mem[b+P_Y]=sy;
                mem[b+P_SIZE]=s;
                mem[b+P_Z]=depthu;          // greater depth -> painted behind
                pick_angular_graph(b,wx,wy);// graphic by angle (if there's xgraph)
            }
        }
    }
}
static void restore_m9_sprites(void){
    int i;
    for(i=0;i<n_m9spr;i++){
        int b=m9spr[i].base;
        mem[b+P_X]=m9spr[i].wx; mem[b+P_Y]=m9spr[i].wy; mem[b+P_Z]=m9spr[i].wz;
        mem[b+P_SIZE]=m9spr[i].sz; mem[b+P_GRAPH]=m9spr[i].gr;
        mem[b+P_CTYPE]=m9spr[i].ct;
    }
    n_m9spr=0;
}

static void mode9_project_fn(void){
    int osz=getparm(),osy=getparm(),osx=getparm();
    int wz=getparm(),wy=getparm(),wx=getparm();
    int sx,sy,depthu,sizepct;
    if(!project_world(wx,wy,wz,&sx,&sy,&depthu,&sizepct)){
        mem[osx]=0;mem[osy]=0;mem[osz]=0;retval(0);return;
    }
    mem[osx]=sx;mem[osy]=sy;mem[osz]=depthu;
    retval(1);
}

// ============================================================================
// DIV2 callbacks
// ============================================================================
// The 3D scenario is drawn as BACKGROUND: DIV2 paints the sprites/processes on top
// (mode 7 style). So the billboards stay in front of the scenario.
// After painting the scenario (and filling the z-buffer), the c_m9 billboards
// are prepared: projected and converted to c_screen so DIV2 paints them.
static void obj9_pump(void);   // incremental model loading (defined below)

void background_to_buffer(void){
    obj9_pump();        // background OBJ model loading (1 file per frame)
    if(m9_active){
        do_render();    // Mode 9 ACTIVE: the 3D scene IS the background
        // If an FPG is loaded in the DLL, billboards are drawn by the DLL itself
        // in each viewport (multi-view, see render_view). Otherwise the
        // DIV2 "c_screen" path (single view) converting c_m9 processes is used.
        if(n_fpgspr==0) process_m9_sprites();
    } else {
        // Mode 9 STOPPED (STOP_MODE9, or before START_MODE9): the PRG may be
        // showing a normal 2D screen (title screen, menus...). Since we
        // export this hook we have REPLACED DIV2's background blit, so
        // if we don't do it ourselves put_screen/put/scroll won't show. We copy
        // DIV2's video background to the buffer (equivalent to the flat engine
        // background_to_buffer; 8 bpp = 1 byte per pixel).
        char *buf=buffer, *bg=background;
        if(buf && bg && wide>0 && height>0)
            memcpy(buf, bg, (size_t)((long)wide*(long)height));
    }
}

// After drawing sprites, restore world coordinates and the c_m9 ctype
// of the billboards (so the PRG handles them with world coordinates).
void post_process_buffer(void){
    restore_m9_sprites();
}


// MODE9_DOWNSCALE(n): renders below native resolution by duplicating
// lines, SNES-Doom style (formerly MODE9_QUALITY).
//   1 = native resolution (default)
//   2 = every 2nd line -> the other is duplicated (~2x fill speed)
//   3 = every 3rd line (~3x)
//   4 = every 4th line (~4x, very chunky pixels)
// Actual gain depends on fill rate (pixels written per frame).
// In scenes with many triangles and few covered pixels, x2 nearly doubles FPS.
static void mode9_downscale_fn(void){
    int q=getparm();
    if(q>=1&&q<=4) m9_downscale=q;
    retval(0);
}

// MODE9_PERSPECTIVE(on): texture perspective correction.
//   0 = PERFORMANCE (default): affine correction in 16-px spans (fast; texture
//       may "wobble" slightly on steep surfaces).
//   1 = QUALITY: exact per-pixel 1/w -> perfect textures, slower.
// Always defaults to performance; user enables quality if desired.
static void mode9_perspective_fn(void){
    int on=getparm();
    m9_texseg = on ? 1 : TEX_SEG;
    if(m9_texseg<1) m9_texseg=1;
    retval(0);
}

// ============================================================================
// DLL entry points
// ============================================================================
// load_obj9_texture: loads the model MAP texture (optional). tname=="" or "0"
// leaves the model in flat-shading. Same logic as load_material_file but for
// an Obj9Model. Shared by LOAD_OBJ9 and LOAD_OBJ9_ANIM.
static void load_obj9_texture(Obj9Model *mdl,char *tname){
    if(tname[0]&&tname[0]!='0'){
        char magic[8]; FILE *f; int w,h,npoints;
        f=div_fopen(tname,"rb");
        if(f){
            if((int)fread(magic,1,8,f)==8&&magic[0]=='m'&&magic[1]=='a'&&magic[2]=='p'){
                unsigned short sw=0,sh=0,snp=0;
                if(fread(&sw,2,1,f)==1&&fread(&sh,2,1,f)==1){
                    fseek(f,1380L,SEEK_CUR);
                    if(fread(&snp,2,1,f)==1){
                        w=(int)sw;h=(int)sh;npoints=(int)snp;
                        if(w>0&&h>0&&w<=1024&&h<=1024){
                            if(npoints>0)fseek(f,(long)npoints*4L,SEEK_CUR);
                            mdl->tex=(char*)div_malloc((size_t)(w*h));
                            if(mdl->tex&&(int)fread(mdl->tex,1,(size_t)(w*h),f)==w*h){
                                int mx,my;
                                mdl->tw=w;mdl->th=h;
                                if(w>=2&&h>=2){
                                    int mw=w>>1,mh=h>>1;
                                    mdl->mip=(char*)div_malloc((size_t)(mw*mh));
                                    if(mdl->mip){
                                        for(my=0;my<mh;my++){
                                            char *d=mdl->mip+(long)my*mw;
                                            char *s=mdl->tex+(long)(my<<1)*w;
                                            for(mx=0;mx<mw;mx++)d[mx]=s[mx<<1];
                                        }
                                        mdl->mw=mw;mdl->mh=mh;
                                    }
                                }
                            } else {
                                if(mdl->tex){div_free(mdl->tex);mdl->tex=NULL;}
                            }
                        }
                    }
                }
            }
            div_fclose(f);
        }
    }
}

// ============================================================================
// LOAD_OBJ9(obj, tex, scale): loads a 3D object model (NPC/prop).
//   obj   : model .OBJ file (Y-up, triangulated; UVs optional).
//   tex   : .MAP texture ("" = flat-shading with world shading ramp).
//   scale : DIV2 units per OBJ unit of the model (same as unitscale in LOAD_WLD9).
// Returns the handle 1..MAX_OBJ9_MODELS IMMEDIATELY (0 if no free slots);
// file loading is done IN THE BACKGROUND (non-blocking, see obj9_pump). The
// model is not drawn until loading completes: DRAW_OBJ9 ignores it while loading
// (appears only when ready). Use MODE9_OBJ9_READY(handle) if you need to
// know when it is. Models are kept until STOP_MODE9/divend.
// ============================================================================
static void load_obj9_fn(void){
    int scale  =getparm();
    int tex_off=getparm();
    int obj_off=getparm();
    Obj9Model *mdl; Obj9Job *job; int midx;
    // Reserve model slot + job slot; actual parsing done by obj9_pump().
    if(n_obj9_models>=MAX_OBJ9_MODELS||n_obj9_jobs>=MAX_OBJ9_MODELS){retval(0);return;}
    midx=n_obj9_models;
    mdl=&obj9_models[midx];
    mdl->verts=NULL;mdl->vframes=NULL;mdl->nframes=1;mdl->uvs=NULL;mdl->tris=NULL;
    mdl->nv=mdl->nuv=mdl->nt=0;
    mdl->tex=NULL;mdl->tw=mdl->th=0;
    mdl->mip=NULL;mdl->mw=mdl->mh=0;
    mdl->ready=0;                              // still loading (background)
    n_obj9_models++;
    job=&obj9_jobs[n_obj9_jobs++];
    job->midx=midx; job->kind=OBJ9_KIND_STATIC;
    div2c(obj_off,job->oname,sizeof(job->oname));
    div2c(tex_off,job->tname,sizeof(job->tname));
    job->sfac=(scale>0)?((float)scale/m9_unitscale):1.0f;
    job->nframes=1; job->step=0; job->block=NULL; job->nv=0;
    retval(midx+1);   // 1-based handle, valid now; model appears when loaded
}

// ============================================================================
// LOAD_OBJ9_ANIM(base, tex, scale, nframes): like LOAD_OBJ9 but loads an
// animation via vertex morphing. Frames are files "<base><n>.OBJ":
// base="CAT", nframes=10 -> CAT0.OBJ..CAT9.OBJ. All must share topology
// (same triangles/UVs and vertex count); only positions change.
// The animation uses ONE slot from MAX_OBJ9_MODELS regardless of nframes.
// Returns the handle IMMEDIATELY (0 if no slots); the nframes files are
// loaded IN THE BACKGROUND at one per frame (no longer blocks or triggers
// watchdog/err 142; previously required adjusting max_process_time). Animate with
// DRAW_OBJ9_ANIM; check MODE9_OBJ9_READY(handle) if you want to wait until ready.
// ============================================================================
static void load_obj9_anim_fn(void){
    int nframes=getparm();
    int scale  =getparm();
    int tex_off=getparm();
    int base_off=getparm();
    Obj9Model *mdl; Obj9Job *job; int midx;
    if(nframes<1) nframes=1;
    // Reserve slot + job; the N frames are parsed in obj9_pump() at
    // ONE per frame, so a long animation no longer blocks or triggers the watchdog.
    if(n_obj9_models>=MAX_OBJ9_MODELS||n_obj9_jobs>=MAX_OBJ9_MODELS){retval(0);return;}
    midx=n_obj9_models;
    mdl=&obj9_models[midx];
    mdl->verts=NULL;mdl->vframes=NULL;mdl->nframes=1;mdl->uvs=NULL;mdl->tris=NULL;
    mdl->nv=mdl->nuv=mdl->nt=0;
    mdl->tex=NULL;mdl->tw=mdl->th=0;
    mdl->mip=NULL;mdl->mw=mdl->mh=0;
    mdl->ready=0;
    n_obj9_models++;
    job=&obj9_jobs[n_obj9_jobs++];
    job->midx=midx; job->kind=OBJ9_KIND_ANIM;
    div2c(base_off,job->oname,sizeof(job->oname));
    div2c(tex_off,job->tname,sizeof(job->tname));
    job->sfac=(scale>0)?((float)scale/m9_unitscale):1.0f;
    job->nframes=nframes; job->step=0; job->block=NULL; job->nv=0;
    retval(midx+1);
}

// ============================================================================
// obj9_pump: advances background loading ONE step (one file) per call.
// Called once per frame from background_to_buffer. Processes jobs in order
// (FIFO): first enqueued model is first to be ready. When a job completes
// (success or failure) it moves to the next; when the queue is empty, does nothing.
// ============================================================================
static void obj9_pump(void){
    Obj9Job *j; Obj9Model *mdl;
    if(obj9_job_head>=n_obj9_jobs){            // empty queue: reset counters
        if(n_obj9_jobs){n_obj9_jobs=0;obj9_job_head=0;}
        return;
    }
    j=&obj9_jobs[obj9_job_head];
    mdl=&obj9_models[j->midx];

    if(j->kind==OBJ9_KIND_STATIC){
        if(j->step==0){
            if(!load_obj9_model(mdl,j->oname,j->sfac)){ mdl->ready=-1; obj9_job_head++; return; }
            j->step=1;                          // texture next frame
        } else {
            load_obj9_texture(mdl,j->tname);
            mdl->ready=1; obj9_job_head++;       // model complete
        }
        return;
    }

    // ---- Animation (vertex morphing): 1 frame per call ----
    if(j->step==0){
        char fname[80];
        build_frame_name(fname,j->oname,0);     // frame 0: topology + positions
        if(!load_obj9_model(mdl,fname,j->sfac)){ mdl->ready=-1; obj9_job_head++; return; }
        j->nv=mdl->nv;
        if(j->nframes>1){
            // Contiguous block for nframes; copy frame 0 and point to it.
            j->block=(V3*)div_malloc((size_t)((long)j->nv*j->nframes*sizeof(V3)));
            if(j->block){
                int f; for(f=0;f<j->nv;f++) j->block[f]=mdl->verts[f];
                div_free(mdl->verts);            // free the 1-frame copy
                mdl->verts=j->block; mdl->vframes=j->block; mdl->nframes=j->nframes;
            } else {
                j->nframes=1;                    // out of memory: stays static (still valid)
            }
        }
        j->step=1;
        return;
    }
    if(j->step < j->nframes){                    // frames 1..nframes-1: positions only
        int f=j->step, got; char fname[80];
        build_frame_name(fname,j->oname,f);
        got=load_obj9_verts_only(fname,j->block+(long)f*j->nv,j->nv,j->sfac);
        if(got!=j->nv){                          // missing frame / different topology:
            int x; V3 *dst=j->block+(long)f*j->nv;//   duplicate frame 0 (no motion)
            for(x=0;x<j->nv;x++) dst[x]=j->block[x];
        }
        j->step++;
        return;
    }
    // Final step: texture and done.
    load_obj9_texture(mdl,j->tname);
    mdl->ready=1; obj9_job_head++;
}

// ============================================================================
// DRAW_OBJ9(handle, x, y, z, angle): queues an instance of model 'handle'
// (returned by LOAD_OBJ9) at world position (x,y,z) DIV2 units with
// the given orientation (thousandths of degree, DIV2 convention). Call EVERY FRAME
// before FRAME (same as DRAW_OBJ in KD3DT). Queue is flushed automatically
// after all views have been rendered.
// ============================================================================
static void draw_obj9_fn(void){
    int angle =getparm();
    int pz    =getparm();
    int py    =getparm();
    int px    =getparm();
    int handle=getparm();
    Obj9Inst *inst; float ar; int ai;
    if(handle<1||handle>n_obj9_models||n_obj9_queue>=MAX_OBJ9_INST){retval(0);return;}
    if(obj9_models[handle-1].ready!=1){retval(0);return;}   // still loading: skip draw
    inst=&obj9_queue[n_obj9_queue];
    inst->midx=handle-1;
    inst->ix=(float)px/m9_unitscale;   // DIV2.x -> OBJ.x
    inst->iz=(float)py/m9_unitscale;   // DIV2.y -> OBJ.z
    inst->iy=(float)pz/m9_unitscale;   // DIV2.z -> OBJ.y (height)
    ar=(float)angle*(6.2831853f/360000.0f);
    ai=(int)(ar*TRIG_K)&TRIG_MASK;
    inst->ysin=sin_tab[ai];inst->ycos=cos_tab[ai];
    inst->fa=0;inst->fb=0;inst->fblend=0.0f;   // no animation (frame 0)
    n_obj9_queue++;
    retval(0);
}

// ============================================================================
// DRAW_OBJ9_ANIM(handle, x, y, z, angle, fpos): like DRAW_OBJ9 but for models
// loaded with LOAD_OBJ9_ANIM. 'fpos' is the animation position in 1/256 of
// frame: integer part (fpos>>8) = frame, low part (fpos&255) = blend
// toward the next. Range is 0..nframes*256 and loops automatically,
// smoothly interpolating between keyframes with very few OBJ files.
// Example (10 frames, 3s loop using timer): see MODE9.PRG.
// ============================================================================
static void draw_obj9_anim_fn(void){
    int fpos  =getparm();
    int angle =getparm();
    int pz    =getparm();
    int py    =getparm();
    int px    =getparm();
    int handle=getparm();
    Obj9Inst *inst; Obj9Model *mdl; float ar; int ai,nf,fi;
    if(handle<1||handle>n_obj9_models||n_obj9_queue>=MAX_OBJ9_INST){retval(0);return;}
    if(obj9_models[handle-1].ready!=1){retval(0);return;}   // still loading: skip draw
    mdl=&obj9_models[handle-1];
    nf=mdl->nframes; if(nf<1) nf=1;
    inst=&obj9_queue[n_obj9_queue];
    inst->midx=handle-1;
    inst->ix=(float)px/m9_unitscale;
    inst->iz=(float)py/m9_unitscale;
    inst->iy=(float)pz/m9_unitscale;
    ar=(float)angle*(6.2831853f/360000.0f);
    ai=(int)(ar*TRIG_K)&TRIG_MASK;
    inst->ysin=sin_tab[ai];inst->ycos=cos_tab[ai];
    // Decompose fpos (1/256 of frame) into base frame + blend, with loop.
    if(fpos<0) fpos=0;
    fi=(fpos>>8)%nf;
    inst->fa=fi;
    inst->fb=(fi+1)%nf;                 // loop: after last frame wraps back to 0
    inst->fblend=(float)(fpos&255)*(1.0f/255.0f);
    n_obj9_queue++;
    retval(0);
}

// ============================================================================
// MODE9_OBJ9_READY(handle): loading state of a LOAD_OBJ9/_ANIM model.
//   1 = loaded and ready for DRAW_OBJ9.
//   0 = still loading in the background (keep calling until it returns 1).
//  -1 = invalid handle or load failed (missing file, etc.).
// Useful for showing a "Loading..." bar or waiting until everything is ready
// before the game starts. Not required: DRAW_OBJ9 already ignores
// models not yet ready on its own (the object appears when done).
// ============================================================================
static void mode9_obj9_ready_fn(void){
    int handle=getparm();
    if(handle<1||handle>n_obj9_models){retval(-1);return;}
    retval(obj9_models[handle-1].ready);
}

void __export divlibrary(LIBRARY_PARAMS){
    COM_export("LOAD_WLD9",       load_wld9_fn,       3);
    COM_export("MODE9_SETTEX",    mode9_settex_fn,    1);
    COM_export("MODE9_MATERIAL",  mode9_material_fn,  2);
    COM_export("LOAD_SKY9",       load_sky9_fn,       1);
    COM_export("SKY9_OFF",        sky9_off_fn,        0);
    COM_export("MODE9_LOADFPG",   mode9_loadfpg_fn,   1);
    COM_export("START_MODE9",     start_mode9_fn,     3);
    COM_export("STOP_MODE9",      stop_mode9_fn,      0);
    COM_export("MODE9_SCRW",      mode9_scrw_fn,      0);
    COM_export("MODE9_SCRH",      mode9_scrh_fn,      0);
    COM_export("MODE9_SET",       mode9_set_fn,       2);
    COM_export("MODE9_UNITSCALE", mode9_unitscale_fn, 1);
    COM_export("MODE9_CULL",      mode9_cull_fn,      1);
    COM_export("MODE9_FOV",       mode9_fov_fn,       1);
    COM_export("MODE9_REGION",    mode9_region_fn,    5);
    COM_export("MODE9_FARCLIP",   mode9_farclip_fn,   1);
    COM_export("MODE9_BIND",      mode9_bind_fn,      1);
    COM_export("SET_ENV_COLOR9",  set_env_color9_fn,  3);
    COM_export("SET_FOG9",        set_fog9_fn,        2);
    COM_export("MODE9_FOG_OFF",   mode9_fog_off_fn,   0);
    COM_export("MODE9_OCCLUDE",   mode9_occlude_fn,   1);
    COM_export("MODE9_CTYPE",     mode9_ctype_fn,     1);
    COM_export("MODE9_COLLISION", mode9_collision_fn, 1);
    COM_export("MODE9_COLLIDE",   mode9_collide_fn,   1);
    COM_export("MODE9_STEP",      mode9_step_fn,      1);
    COM_export("MODE9_DOWNSCALE",  mode9_downscale_fn,  1);
    COM_export("MODE9_PERSPECTIVE", mode9_perspective_fn, 1);
    COM_export("MODE9_SPLIT",     mode9_split_fn,     7);
    COM_export("MODE9_BILLBOARD", mode9_billboard_fn, 5);
    COM_export("MODE9_PROJECT",   mode9_project_fn,   6);
    COM_export("LOAD_OBJ9",       load_obj9_fn,       3);
    COM_export("DRAW_OBJ9",       draw_obj9_fn,       5);
    COM_export("LOAD_OBJ9_ANIM",  load_obj9_anim_fn,  4);
    COM_export("DRAW_OBJ9_ANIM",  draw_obj9_anim_fn,  6);
    COM_export("MODE9_OBJ9_READY",mode9_obj9_ready_fn,1);
}

void __export divmain(COMMON_PARAMS){
    int i;
    g_import=DIV_import;g_export=DIV_export;
    #define DIV_import g_import
    #define DIV_export g_export

    AutoLoad();
    GLOBAL_IMPORT();

    // Build trigonometric tables (once, on DLL load).
    // 4096 entries = 0.088-degree resolution, much finer than
    // DIV2's angle granularity (1 thousandth of a degree = 0.001 degrees).
    {int i; for(i=0;i<TRIG_SIZE;i++){
        float a=(float)i*(6.2831853f/(float)TRIG_SIZE);
        sin_tab[i]=(float)sin(a); cos_tab[i]=(float)cos(a);
    }}

    m9_active=0;m9_camera=0;m9_height=0;m9_pitch=0;m9_unitscale=100.0f;
    m9_bind=0;m9_region=0;m9_occlude=1;m9_ctype=200;far_clip=0.0f;cull_mode=1;
    m9_collision=1;n_walls=0;m9_downscale=1;m9_texseg=TEX_SEG;   // collisions ON by default
    m9_focal_k=FOCAL_K;fog_scale=0.0f;
    m9_step=0.0f;n_floors=0;n_ceils=0;
    grid_n=0;frame_stamp=0;
    grid_items=NULL;grid_cap=0;tstamp=NULL;tstamp_cap=0;
    wgrid_items=NULL;wgrid_cap=0;wgrid_n=0;wstamp_frame=0;n_wcand=0;
    for(i=0;i<MAX_VERTS;i++) vstamp[i]=0;   // no stale stamps across runs
    for(i=0;i<MAX_VIEWS;i++){views[i].active=0;views[i].haslast=0;views[i].region=-1;}
    for(i=0;i<MAX_REGIONS;i++) reg_def[i]=0;   // no regions defined
    n_m9spr=0;n_fpgspr=0;
    {int m; for(m=0;m<MAX_MATS;m++){mat_data[m]=NULL;mat_w[m]=mat_h[m]=0;mat_mip[m]=NULL;matm_w[m]=matm_h[m]=0;}}
    n_mats=1;
    {int m; for(m=0;m<MAX_OBJ9_MODELS;m++){
        obj9_models[m].verts=NULL;obj9_models[m].vframes=NULL;obj9_models[m].nframes=1;
        obj9_models[m].uvs=NULL;obj9_models[m].tris=NULL;
        obj9_models[m].nv=obj9_models[m].nuv=obj9_models[m].nt=0;
        obj9_models[m].tex=NULL;obj9_models[m].tw=obj9_models[m].th=0;
        obj9_models[m].mip=NULL;obj9_models[m].mw=obj9_models[m].mh=0;
        obj9_models[m].ready=0;
    }}
    n_obj9_models=0;n_obj9_queue=0;
    n_obj9_jobs=0;obj9_job_head=0;   // no pending background loads
    sky_data=NULL;sky_w=0;sky_h=0;sky_on=0;sky_cd_vw=0;sky_cd_w=0;
    fog_map=NULL;fog_on=0;
    env_r=env_g=env_b=0;
    m9_msg[0]=0;

    DIV_export("background_to_buffer",background_to_buffer);
    DIV_export("post_process_buffer", post_process_buffer);

    // Save reserved palette (flat ramp + background entry)
    for(i=0;i<PAL_SHADES*3;i++) orig_pal[i]=palette[PAL_BASE*3+i];
    for(i=0;i<3;i++) orig_pal[PAL_SHADES*3+i]=palette[PAL_ENV*3+i];

    // Default flat ramp: grey
    {int j;
     for(j=0;j<PAL_SHADES;j++){
         float t=AMBIENT+(1.0f-AMBIENT)*(float)j/(float)(PAL_SHADES-1);
         palette[(PAL_BASE+j)*3+0]=(char)((int)(50.0f*t+0.5f));
         palette[(PAL_BASE+j)*3+1]=(char)((int)(50.0f*t+0.5f));
         palette[(PAL_BASE+j)*3+2]=(char)((int)(50.0f*t+0.5f));
     }}
    // Default background: black
    palette[PAL_ENV*3+0]=0;palette[PAL_ENV*3+1]=0;palette[PAL_ENV*3+2]=0;
    set_palette=1;

    #undef DIV_import
    #undef DIV_export
}

void __export divend(COMMON_PARAMS){
    int i;
    (void)DIV_import;(void)DIV_export;
    {int m; for(m=0;m<MAX_MATS;m++){
        if(mat_data[m]){div_free(mat_data[m]);mat_data[m]=NULL;mat_w[m]=mat_h[m]=0;}
        if(mat_mip[m]){div_free(mat_mip[m]);mat_mip[m]=NULL;matm_w[m]=matm_h[m]=0;}
    }}
    {int m; for(m=0;m<n_obj9_models;m++){
        // verts == vframes (same block); free once and null both.
        if(obj9_models[m].verts){div_free(obj9_models[m].verts);obj9_models[m].verts=NULL;}
        obj9_models[m].vframes=NULL;obj9_models[m].nframes=1;
        if(obj9_models[m].uvs)  {div_free(obj9_models[m].uvs);  obj9_models[m].uvs=NULL;}
        if(obj9_models[m].tris) {div_free(obj9_models[m].tris); obj9_models[m].tris=NULL;}
        if(obj9_models[m].tex)  {div_free(obj9_models[m].tex);  obj9_models[m].tex=NULL;}
        if(obj9_models[m].mip)  {div_free(obj9_models[m].mip);  obj9_models[m].mip=NULL;}
    }}
    // In-flight jobs have no memory of their own: their 'block' is the same pointer
    // as obj9_models[].verts (already freed above). Just reset counters.
    n_obj9_models=0;n_obj9_queue=0;
    n_obj9_jobs=0;obj9_job_head=0;
    if(sky_data){div_free(sky_data);sky_data=NULL;sky_w=sky_h=0;sky_on=0;}
    if(grid_items){div_free(grid_items);grid_items=NULL;grid_cap=0;}
    if(wgrid_items){div_free(wgrid_items);wgrid_items=NULL;wgrid_cap=0;wgrid_n=0;}
    if(tstamp){div_free(tstamp);tstamp=NULL;tstamp_cap=0;}
    if(fog_map){div_free(fog_map);fog_map=NULL;}
    if(zbuf){div_free(zbuf);zbuf=NULL;zbuf_w=zbuf_h=0;}
    free_sprites();
    mesh_ok=0;m9_active=0;
    for(i=0;i<PAL_SHADES*3;i++) palette[PAL_BASE*3+i]=orig_pal[i];
    for(i=0;i<3;i++) palette[PAL_ENV*3+i]=orig_pal[PAL_SHADES*3+i];
    set_palette=1;
}

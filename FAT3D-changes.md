# FAT3D: what it adds to ROLLER, and how it works

**FAT3D: Fatal Racing, tuned for modern PCs** is the RenacidaMods build of Fatal Racing (Gremlin, 1995). It's **built on [ROLLER](https://github.com/FatalDecomp/ROLLER) by FatalDecomp**, whose rebuild of the game made all of this possible. FAT3D only adds to it.

The aim: Fatal Racing as it might have looked with full 3D-card rendering of the late 1990s and early 2000s, at today's resolutions and refresh rates. The name comes from FAT3D.EXE, the game's own first 3D-card version (the 1996 3D Blaster patch).

Source: branch `renacida-mods` of https://github.com/RenacidaMods/FAT3D. One commit per change; most are behind a setting. Nothing here needs or ships game files: players use their own copy of Fatal Racing.

Last updated: 19/11/2026.

---

## Where the settings are

Press the backtick key (\`) in game for ROLLER's settings overlay. The look settings are under **Graphics**, and a **compact tuning panel** (a button at the top of the Graphics column) shows just the look sliders in a small, see-through, movable window. Settings save to `FATDATA\ROLLER.INI`.

All the lighting and effects below are **GPU renderer only**. The original software renderer is unchanged.

| Change | Setting (INI key) | Default |
| --- | --- | --- |
| HD texture packs | HD Textures (next race) (`HDTextures`) | Off |
| Texture dump for upscaling | Dump Textures as PNG | Off |
| Render scale 4x, 5x and Screen | Render scale (`RenderScale`) | 1x |
| Ambient occlusion | Ambient occlusion (`AmbientOcclusion`, `AORadius`, `AOStrength`) | Off, radius 400, strength 100% |
| Sky gradient | Sky gradient (`SkyGradient`) | 100% (0% = original flat sky) |
| Soft HD clouds | part of the HD texture pack | |
| Sun shading | Sun shading (`SunShading`) | 60% |
| Real car shadows | Car shadows (`CarShadows`) | 60% darkness (0% = classic flat shadows) |
| Car shine | Car shine, Shine glint, Shine smoothing, Shine bevel, Shine darkness (`CarShine...`) | 100%, 400%, 100%, 25%, 55% |
| Cars darken in tunnels | always on with the GPU renderer | |
| HUD scale and map scale | HUD scale (`HUDScale`), Map scale (`HUDMapScale`) | 100% |
| Instant keyboard steering | Instant keyboard steering (`InstantKeyboardSteering`) | Off |
| CD music from MP3 | music source "CD" | |
| Reset graphics | Reset graphics | restores all of the above |

---

## HD textures

**What it does:** replaces the 1995 textures (64x64 tiles) with HD versions (256x256, 4x) from a texture pack, for track surfaces, walls, buildings, signs, cars, generic textures (clouds, smoke) and the menu and winner-screen pictures.

**How it works:**
- The GPU renderer already cuts every texture atlas into one GPU texture per 64x64 tile, with texture coordinates 0 to 1 per tile, so a bigger PNG can replace a tile with no other changes. Files are looked up as `hd_textures\<bank file name>\<tile>.png` next to the game folder.
- Cars have their own atlas per car design. The HD car loader builds that atlas from HD tiles at 4x, shares the sheet between cars of the same design, and pulls texture coordinates 2 HD texels in so filtering never reaches into the neighbouring tile (that showed as a square outline round the wheels).
- Car panels are drawn as a 4x4 grid instead of two triangles, so tapered panels (the Global's bonnet lettering) don't bend their texture along the diagonal.
- Loading uses several threads (PNG decoding was the slow part) and HD cars on the car-select screen get smooth filtering.
- HD cloud tiles use normal filtering and are drawn through the blend pipeline, so soft see-through edges fade smoothly instead of being cut out at 50%.
- The winner screen's background and the menus load HD pictures from `hd_textures\menu`.

**Making a pack:** "Dump Textures as PNG" writes every tile the game loads to `hd_textures_dump`. `src\tools\hd_texture_sheet.py` stitches dumped tiles into sheets, drives Upscayl to upscale them, and cuts the result back into tiles (keeping the original's see-through border pixels clear). The pack itself is made from the game's copyrighted art, so it's never shared: each player makes their own from their own copy.

## Resolution

- **Render scale 4x and 5x** of the original 640x400, and **"Screen"**, which renders the main view at exactly the window's pixel size (1:1 at 4K).
- **Frame pacing** follows refresh-rate changes instead of keeping the first one it saw (moving the window between monitors or changing refresh rate no longer leaves it paced wrongly).

## Lighting and effects

All of these share **one sun** (`scene_render_gpu_sun_direction()`: towards (0.60, 0.45, 0.66) in world space, up = +Z), so shading, shadows and car highlights always agree.

**Ambient occlusion** (soft contact shadow where walls meet road, under barriers, in tunnels): a half-resolution pass rebuilds view-space positions from the depth buffer, samples a 16-point hemisphere with a non-repeating per-pixel rotation, then a depth-aware 4x4 blur multiplies it into the scene. Fixes along the way: an angle bias so the shallow folds between road sections on hills don't darken; a height test so small lips between track sections don't draw stripes; normals measured over three texels on smooth surfaces (depth precision far away); the radius capped at 6% of the screen width close to the camera; and whole-texel depth lookups, because the half-size pass sat exactly on depth-texel borders and rounding picked the left or right texel differently per column, which drew screen-fixed stripes. An "AO only view (debug)" shows what it picks up.

**Sky gradient:** the sky polygon is drawn by its own shader, which rebuilds each pixel's view ray from the inverse view-projection and blends from a pale horizon to a deeper zenith made from the track's own sky colour, so it follows the camera's pitch and roll. Fog still takes the sky over as before.

**Soft clouds:** the five cloud tiles (gentex 8 to 12; each cloud picks one at random) are redrawn from the originals' shapes with real see-through edges (part of the texture pack, which each player makes from their own copy).

**Sun shading:** the world pixel shaders take the flat face normal from the screen-space change in world position, turn it towards the camera and light it half-Lambert from the sun, measured against flat ground: level surfaces keep their exact colour, faces turned away from the sun darken (about half at 100%), faces towards it brighten a little. Trees (camera-facing cut-outs), clouds, glass and shadows aren't shaded. Cars use their smoothed normals.

**Real car shadows:** each frame the car bodies are drawn from the sun's view into a 2048x2048 depth map (an orthographic box 12,000 units across, centred just ahead of the camera and snapped to whole texels so shadows don't shimmer). The world shaders project their own position into that view and darken where a car is nearer the sun, with soft edges from a 3x3 grid of 2x2 gathers, fading out at the edge of the map. Shadows fall over kerbs, banking and walls. Only cars cast, so the road never shadows itself. While on, the classic flat car shadows aren't drawn; 0% brings them back, and the game's shadows option still turns all car shadows off. Limit: cars more than about 9 car lengths from the view have no shadow.

**Car shine:** the car shader reflects a built-in environment (sky gradient, a dark block skyline on the horizon, darker ground) along the mirror direction, replacing part of the paint by Fresnel, so reflections slide across the panels as the car turns (the Screamer and Need for Speed II look). Details:
- Smoothing: per-corner normals averaged over neighbouring panels with cosine weights, so reflections flow round edges; a bevel strip along panel edges catches a rounded highlight.
- A sun highlight on the panels facing the sun (a half vector leaning towards the sun), because a true mirror glint only shows when looking towards the sun and read as back to front. Reflections dim on the side away from the sun.
- The underside (panels tipped more than about 9 degrees towards the ground) fades out, and the reflected ground can only darken the paint.
- The sun reaches the car shader already in the car's own coordinates (rebuilding it in the shader mirrored it on mirrored track sections).
- Menu and winner-screen cars get the same shine.

**Cars darken in tunnels:** a car under a covered section dims by 45%, easing in over about a quarter of a second and brightening almost at once on the way out. A section counts as covered if it has a **roof** (the rule the track drawing uses: a roof colour set and walls on both sides) or is a **tube**, where the left and right upper outer walls lean in until their tops meet over the road (track 2's second corner, track 3's tube, the start of track 8). Checked against the data of six tracks.

**Soft contact shadow under menu cars** on the car-select and winner screens.

## HUD and menus

- **HUD scale:** each HUD piece shrinks towards its own corner or edge (40 to 100%). Zoom messages (FINAL LAP, PIT WORK, laps to go) and the pause menus draw on their own unscaled layer, and the track map is drawn as one piece with its own scale towards the bottom-left, so nothing jumps around.
- **Winner screen** is drawn by the GPU menu renderer (HD car, shine, HD background) instead of the 640x400 software screen.
- Alt, Ctrl or Shift on their own no longer skip the intro or winner screen (Alt+Print Screen used to).

## Controls

- **Instant keyboard steering:** keys give full lock at once and centre on release, like a gamepad stick (the original ramps up over 0.36 seconds).

## Sound

- **CD music from MP3:** `audio\track02.mp3` onwards play as the CD soundtrack (streamed with the public-domain dr_mp3 decoder), for players whose disc image has no audio tracks.
- **Audio crash fix:** SDL 3.2.22 left a stale link in its device list when Windows switched the default sound output, so closing a sound stream later crashed (the same crash as ROLLER issue 307). FAT3D builds against https://github.com/RenacidaMods/SDL branch `renacida-audio-fix`: the SDL ROLLER pins, plus SDL main's three-line fix.

## Other fixes

- Bank 18 particle textures were never freed.
- The GPU renderer now honours the shadows option for car shadows.
- Snapshot test runs never save FATAL.INI or ROLLER.INI.

## Tools (in the code)

- `src\tools\hd_texture_sheet.py`: dump, stitch, upscale with Upscayl, cut back.
- The "Dump Textures as PNG" option.
- **GPU snapshots:** `--snapshot REPLAY.GSS --frames N,M --out DIR --snapshot-gpu vulkan|direct3d12 --snapshot-size 3840x2160 --snapshot-look ini|nopack|stock` renders replay frames through the GPU renderer with no window and saves them as PNGs, without the HUD. `ini` uses the player's ROLLER.INI look; `nopack` the same without HD textures (what a player sees before making a pack); `stock` is ROLLER's own GPU renderer, with HD textures, ambient occlusion, sky gradient, sun shading, real car shadows, car shine and tunnel darkening off (the player's texture filter and anti-aliasing stay, so a pair differs only by FAT3D's changes). For before-and-after screenshots, and for checking the GPU look headlessly. Settings are never saved. Editor and parity readbacks still skip the FAT3D look, so their outputs never change.

---

## Notes for anyone offering parts upstream

- `scene_render_gpu.c` is part of the track editor's `roller-core` library but reads `g_bDumpTexturesPNG` and `g_bHDTextures` from `roller.c`, which the editor doesn't link. Pass them in through setters first.
- Shaders: the headers store each shader as a byte array plus a separate `_size` constant; update both. Direct3D 12 needs each vertex and pixel shader pair compiled by the same shadercross (current builds pack SV_Position last); all scene shaders were rebuilt together on 27/10/2026.
- Readbacks that keep the exact output push the flat-sky uniforms to the sky pipeline, which FAT3D's sky gradient shader replaces, so a readback that draws the sky gets a wrong colour (pure green). The parity tests pass, but fix this before offering the sky gradient upstream.
- Good first pull requests: the shadows-option fix, the SDL audio fix, MP3 music. Link https://github.com/FatalDecomp/ROLLER/discussions/311 from any of them.

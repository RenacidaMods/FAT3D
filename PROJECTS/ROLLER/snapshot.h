#ifndef _ROLLER_SNAPSHOT_H
#define _ROLLER_SNAPSHOT_H
//-------------------------------------------------------------------------------------------------
#include "types.h"
//-------------------------------------------------------------------------------------------------

#define SNAPSHOT_MAX_FRAMES 64

typedef enum
{
  SNAPSHOT_KIND_NONE = 0,
  SNAPSHOT_KIND_REPLAY,
  SNAPSHOT_KIND_SCENE,
} eSnapshotKind;

typedef struct
{
  eSnapshotKind eKind;
  char szReplayName[64];
  char szSceneName[64];
  char szOutDir[256];
  int iFramesAy[SNAPSHOT_MAX_FRAMES];
  int iNumFrames;
  int iCapturedCount;
  int iMaxFrame;
  int iPresentFrame;
  // FAT3D: GPU snapshots (--snapshot-gpu). Empty backend = software snapshots.
  char szGpuBackend[16];
  int iGpuWidth;
  int iGpuHeight;
  int bGpuStockLook;       // 1 = ROLLER's own GPU look (FAT3D extras off)
  int bGpuNoPack;          // 1 = the player's look without HD textures
  int iGpuLastFrame;       // last replay frame read back, so none is written twice
} tSnapshotConfig;

extern int g_bSnapshotMode;
extern tSnapshotConfig g_SnapshotConfig;

//-------------------------------------------------------------------------------------------------

// Sets the replay file the snapshot driver will load. Truncates if too long.
void SnapshotSetReplay(const char *szReplay);

// Sets the named scene the snapshot driver will render. Truncates if too long.
void SnapshotSetScene(const char *szScene);

// Sets the output directory PNGs are written into.
void SnapshotSetOutDir(const char *szOutDir);

// Parses a comma-separated list of non-negative frame indices into the config.
// Returns 0 on success, 1 on malformed input or overflow.
int SnapshotParseFrames(const char *szFramesArg);

// Top-of-tick hook: zeroes scrbuf so unused HUD/border regions can never leak
// from the previous frame. No-op outside snapshot mode.
void SnapshotZeroScreen(void);

// End-of-tick hook called in place of GPU presentation. Writes an indexed PNG
// when currentreplayframe matches a requested frame index. Sets quit_game once
// every requested frame has been captured.
void SnapshotPresent(void);

// Returns non-zero once snapshot mode has captured all requested frames.
int SnapshotShouldStop(void);

// Queues one raw set-1 keyboard scancode for scene snapshots that need to
// reach a real submenu state through the normal input handler.
void SnapshotQueueRawKey(uint8 byRawKey);

// Drives the replay one tick per loop iteration in lieu of the SDL tick timer.
// No-op outside snapshot mode.
void SnapshotAdvanceTick(void);

// Substitutes for load_fatal_config() in snapshot mode. Pins every
// rendering-relevant global (textures_off, game_svga, game_size, view_limit,
// allengines, cheat_mode, etc.) to a fixed default set so the captured pixels
// are independent of the developer's local fatal.ini.
void SnapshotApplyFixedSettings(void);

// Pins record-table data after LoadRecords() so menu snapshots do not drift
// with the developer's mutable dgkfc.rec file.
void SnapshotApplyFixedRecords(void);

// FAT3D: GPU snapshots render replay frames through the GPU renderer with no
// window and read them back as RGBA PNGs, for screenshots and for checking the
// GPU look headlessly. Settings come from ROLLER.INI (snapshot runs never save it).
// Sets the backend ("vulkan" or "direct3d12"). Returns 0 on success.
int SnapshotSetGpuBackend(const char *szBackend);
// Parses "WIDTHxHEIGHT" for GPU snapshots. Returns 0 on success.
int SnapshotParseGpuSize(const char *szSize);
// "ini" (the player's settings), "nopack" (the same without HD textures) or
// "stock" (FAT3D extras off). Returns 0 on success.
int SnapshotParseGpuLook(const char *szLook);
int SnapshotGpuActive(void);
// Called after ROLLER.INI is loaded: hides the FPS counter and, for "stock",
// turns off everything FAT3D adds to the GPU renderer. No-op without a GPU
// snapshot.
void SnapshotApplyGpuLook(void);
// Called once per GPU frame. Returns 1 (with the size) when this replay frame
// should be read back instead of submitted normally.
int SnapshotGpuWantsFrame(int *piWidth, int *piHeight);
// Called after every GPU frame; pbyRgba is the read-back frame or NULL. Writes
// the PNG and stops the run once every requested frame is done.
void SnapshotGpuFrameDone(const uint8 *pbyRgba, int iWidth, int iHeight);

//-------------------------------------------------------------------------------------------------
#endif

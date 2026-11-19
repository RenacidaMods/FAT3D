#include "snapshot.h"
#include "png_writer.h"
#include "3d.h"
#include "replay.h"
#include "sound.h"
#include "frontend.h"
#include "roller.h"
#include "control.h"
#include "drawtrk3.h"
#include "colision.h"
#include "func2.h"
#include <SDL3/SDL_filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#define strtok_r strtok_s
#endif

//-------------------------------------------------------------------------------------------------

int g_bSnapshotMode = 0;
tSnapshotConfig g_SnapshotConfig = { 0 };

typedef struct SnapshotRecordFixture
{
  int iTrackIdx;
  int iLapCentiseconds;
  int iCarIdx;
  int iKills;
  char szName[9];
} SnapshotRecordFixture;

static const SnapshotRecordFixture g_SnapshotRecordFixtures[] = {
  { 1, 6127, 0, 0, "HUMAN" },
};

//-------------------------------------------------------------------------------------------------

void SnapshotSetReplay(const char *szReplay)
{
  if (!szReplay) return;
  g_SnapshotConfig.eKind = SNAPSHOT_KIND_REPLAY;
  strncpy(g_SnapshotConfig.szReplayName, szReplay, sizeof(g_SnapshotConfig.szReplayName) - 1);
  g_SnapshotConfig.szReplayName[sizeof(g_SnapshotConfig.szReplayName) - 1] = '\0';
}

//-------------------------------------------------------------------------------------------------

void SnapshotSetScene(const char *szScene)
{
  if (!szScene) return;
  g_SnapshotConfig.eKind = SNAPSHOT_KIND_SCENE;
  strncpy(g_SnapshotConfig.szSceneName, szScene, sizeof(g_SnapshotConfig.szSceneName) - 1);
  g_SnapshotConfig.szSceneName[sizeof(g_SnapshotConfig.szSceneName) - 1] = '\0';
}

//-------------------------------------------------------------------------------------------------

void SnapshotSetOutDir(const char *szOutDir)
{
  if (!szOutDir) return;
  strncpy(g_SnapshotConfig.szOutDir, szOutDir, sizeof(g_SnapshotConfig.szOutDir) - 1);
  g_SnapshotConfig.szOutDir[sizeof(g_SnapshotConfig.szOutDir) - 1] = '\0';
}

//-------------------------------------------------------------------------------------------------

int SnapshotParseFrames(const char *szFramesArg)
{
  if (!szFramesArg || !*szFramesArg) return 1;

  char szBuf[512];
  strncpy(szBuf, szFramesArg, sizeof(szBuf) - 1);
  szBuf[sizeof(szBuf) - 1] = '\0';

  g_SnapshotConfig.iNumFrames = 0;
  g_SnapshotConfig.iMaxFrame = -1;
  g_SnapshotConfig.iCapturedCount = 0;
  g_SnapshotConfig.iPresentFrame = 0;

  char *pSaveptr = NULL;
  char *pTok = strtok_r(szBuf, ",", &pSaveptr);
  while (pTok) {
    while (*pTok == ' ' || *pTok == '\t') ++pTok;
    if (!*pTok) return 1;
    char *pEnd = NULL;
    long lVal = strtol(pTok, &pEnd, 10);
    if (pEnd == pTok || (*pEnd != '\0' && *pEnd != ' ' && *pEnd != '\t') || lVal < 0)
      return 1;
    if (g_SnapshotConfig.iNumFrames >= SNAPSHOT_MAX_FRAMES)
      return 1;
    g_SnapshotConfig.iFramesAy[g_SnapshotConfig.iNumFrames++] = (int)lVal;
    if ((int)lVal > g_SnapshotConfig.iMaxFrame)
      g_SnapshotConfig.iMaxFrame = (int)lVal;
    pTok = strtok_r(NULL, ",", &pSaveptr);
  }

  return g_SnapshotConfig.iNumFrames > 0 ? 0 : 1;
}

//-------------------------------------------------------------------------------------------------

void SnapshotZeroScreen(void)
{
  if (!g_bSnapshotMode || !scrbuf) return;
  size_t bytes = (size_t)(SVGA_ON ? 256000 : 64000);
  memset(scrbuf, 0, bytes);
}

//-------------------------------------------------------------------------------------------------

static void SnapshotCopyStem(char *szStem, size_t uiStemSize)
{
  const char *szSource = g_SnapshotConfig.eKind == SNAPSHOT_KIND_SCENE
    ? g_SnapshotConfig.szSceneName
    : g_SnapshotConfig.szReplayName;

  strncpy(szStem, szSource, uiStemSize - 1);
  szStem[uiStemSize - 1] = '\0';

  // Strip the directory portion if any (caller may have passed a path).
  char *pSlash = strrchr(szStem, '/');
  char *pBack = strrchr(szStem, '\\');
  char *pBase = pSlash;
  if (pBack && (!pBase || pBack > pBase)) pBase = pBack;
  if (pBase) memmove(szStem, pBase + 1, strlen(pBase + 1) + 1);

  // Strip extension.
  char *pDot = strrchr(szStem, '.');
  if (pDot) *pDot = '\0';

  // Lowercase.
  for (char *p = szStem; *p; ++p) {
    if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
  }
}

static void SnapshotBuildPath(char *szOut, size_t uiOutSize, int iFrame)
{
  char szStem[64];
  SnapshotCopyStem(szStem, sizeof(szStem));

  size_t uiDirLen = strlen(g_SnapshotConfig.szOutDir);
  int bHasSep = uiDirLen > 0 &&
    (g_SnapshotConfig.szOutDir[uiDirLen - 1] == '/' ||
     g_SnapshotConfig.szOutDir[uiDirLen - 1] == '\\');
  snprintf(szOut, uiOutSize, "%s%s%s_%d.png",
           g_SnapshotConfig.szOutDir,
           bHasSep ? "" : "/",
           szStem,
           iFrame);
}

//-------------------------------------------------------------------------------------------------

void SnapshotPresent(void)
{
  if (!g_bSnapshotMode || !scrbuf) return;
  if (SnapshotGpuActive()) return;   // GPU snapshots capture in SnapshotGpuFrameDone
  if (!g_bPaletteSet) return;

  int iCaptureKey = currentreplayframe;
  if (g_SnapshotConfig.eKind == SNAPSHOT_KIND_SCENE) {
    iCaptureKey = ++g_SnapshotConfig.iPresentFrame;
  }

  int bMatch = 0;
  for (int i = 0; i < g_SnapshotConfig.iNumFrames; ++i) {
    if (g_SnapshotConfig.iFramesAy[i] == iCaptureKey) {
      bMatch = 1;
      break;
    }
  }

  if (bMatch) {
    char szPath[512];
    SnapshotBuildPath(szPath, sizeof(szPath), iCaptureKey);

    // Make sure the output directory exists before writing the PNG.
    SDL_CreateDirectory(g_SnapshotConfig.szOutDir);

    int iRc = RollerWriteIndexedPng(szPath, scrbuf, palette, 640, 400);
    if (iRc != 0) {
      fprintf(stderr, "snapshot: PNG write failed (rc=%d) for '%s'\n", iRc, szPath);
    } else {
      fprintf(stdout, "snapshot: wrote '%s' (frame %d)\n", szPath, iCaptureKey);
      fflush(stdout);
    }
    g_SnapshotConfig.iCapturedCount++;
  }

  if (g_SnapshotConfig.iCapturedCount >= g_SnapshotConfig.iNumFrames ||
      (g_SnapshotConfig.eKind != SNAPSHOT_KIND_SCENE && currentreplayframe >= g_SnapshotConfig.iMaxFrame) ||
      (g_SnapshotConfig.eKind == SNAPSHOT_KIND_SCENE && g_SnapshotConfig.iPresentFrame >= g_SnapshotConfig.iMaxFrame)) {
    quit_game = 1;
    racing = 0;
  }
}

//-------------------------------------------------------------------------------------------------

int SnapshotShouldStop(void)
{
  return g_bSnapshotMode &&
         g_SnapshotConfig.iCapturedCount >= g_SnapshotConfig.iNumFrames;
}

//-------------------------------------------------------------------------------------------------

void SnapshotQueueRawKey(uint8 byRawKey)
{
  if (!g_bSnapshotMode) return;
  key_buffer[write_key] = byRawKey;
  write_key = (write_key + 1) & 0x3F;
}

//-------------------------------------------------------------------------------------------------

void SnapshotAdvanceTick(void)
{
  if (!g_bSnapshotMode) return;
  tick_clock_step();
  SDL_SetAtomicInt(&iTicksPending, 0);
  if (!frontend_on)
    game_tick_step();
}

//-------------------------------------------------------------------------------------------------

void SnapshotApplyFixedSettings(void)
{
  // Replaces load_fatal_config() in snapshot mode. Pinning these defaults
  // makes the captured pixels independent of the developer's local
  // fatal.ini (which would otherwise toggle texturing options, draw
  // distance, screen size, etc., and silently drift the baselines).
  fatal_ini_loaded = -1;     // skip the auto-config branch keyed on machine_speed
  textures_off = 0;          // every rendering feature enabled
  game_svga = -1;            // SVGA / 640x400 framebuffer
  game_size = 128;           // full game screen size
  game_view[0] = 1;          // chase camera
  game_view[1] = 1;
  allengines = -1;           // engine particles on
  view_limit = 32;           // max draw distance
  cheat_mode = 0;            // no cheats
  replay_record = 0;
  soundon = 0;               // headless: no audio
  musicon = 0;
  names_on = 1;              // standard player-name overlay setting
  level = 0;
  damage_level = 0;
  infinite_laps = 0;
}

//-------------------------------------------------------------------------------------------------

void SnapshotApplyFixedRecords(void)
{
  if (!g_bSnapshotMode) return;

  for (int i = 0; i < 25; ++i) {
    memset(RecordNames[i], 0, sizeof(RecordNames[i]));
    strcpy(RecordNames[i], "-----");
    RecordLaps[i] = 128.0f;
    RecordCars[i] = -1;
    RecordKills[i] = 0;
  }

  for (int i = 0; i < (int)(sizeof(g_SnapshotRecordFixtures) / sizeof(g_SnapshotRecordFixtures[0])); ++i) {
    const SnapshotRecordFixture *pFixture = &g_SnapshotRecordFixtures[i];
    if (pFixture->iTrackIdx < 0 || pFixture->iTrackIdx >= 25) continue;

    RecordLaps[pFixture->iTrackIdx] = (float)pFixture->iLapCentiseconds * 0.01f;
    RecordCars[pFixture->iTrackIdx] = pFixture->iCarIdx;
    RecordKills[pFixture->iTrackIdx] = pFixture->iKills;
    memset(RecordNames[pFixture->iTrackIdx], 0, sizeof(RecordNames[pFixture->iTrackIdx]));
    strncpy(RecordNames[pFixture->iTrackIdx], pFixture->szName, sizeof(RecordNames[pFixture->iTrackIdx]) - 1);
  }
}

//-------------------------------------------------------------------------------------------------
// FAT3D: GPU snapshots

int SnapshotSetGpuBackend(const char *szBackend)
{
  if (!szBackend || (strcmp(szBackend, "vulkan") != 0 && strcmp(szBackend, "direct3d12") != 0))
    return 1;
  strncpy(g_SnapshotConfig.szGpuBackend, szBackend, sizeof(g_SnapshotConfig.szGpuBackend) - 1);
  g_SnapshotConfig.szGpuBackend[sizeof(g_SnapshotConfig.szGpuBackend) - 1] = '\0';
  g_SnapshotConfig.iGpuLastFrame = -1;
  if (g_SnapshotConfig.iGpuWidth <= 0) {
    g_SnapshotConfig.iGpuWidth = 1920;
    g_SnapshotConfig.iGpuHeight = 1080;
  }
  return 0;
}

//-------------------------------------------------------------------------------------------------

int SnapshotParseGpuSize(const char *szSize)
{
  int iWidth = 0, iHeight = 0;
  if (!szSize || sscanf(szSize, "%dx%d", &iWidth, &iHeight) != 2)
    return 1;
  if (iWidth < 64 || iHeight < 64 || iWidth > 8192 || iHeight > 8192)
    return 1;
  g_SnapshotConfig.iGpuWidth = iWidth;
  g_SnapshotConfig.iGpuHeight = iHeight;
  return 0;
}

//-------------------------------------------------------------------------------------------------

int SnapshotParseGpuLook(const char *szLook)
{
  if (!szLook) return 1;
  g_SnapshotConfig.bGpuStockLook = 0;
  g_SnapshotConfig.bGpuNoPack = 0;
  if (strcmp(szLook, "stock") == 0) g_SnapshotConfig.bGpuStockLook = 1;
  else if (strcmp(szLook, "nopack") == 0) g_SnapshotConfig.bGpuNoPack = 1;
  else if (strcmp(szLook, "ini") != 0) return 1;
  return 0;
}

//-------------------------------------------------------------------------------------------------

int SnapshotGpuActive(void)
{
  return g_bSnapshotMode && g_SnapshotConfig.szGpuBackend[0] != '\0';
}

//-------------------------------------------------------------------------------------------------

void SnapshotApplyGpuLook(void)
{
  if (!SnapshotGpuActive()) return;
  g_iFpsDisplay = 0;          // no FPS counter or name tags in screenshots
  names_on = 0;
  if (g_SnapshotConfig.bGpuNoPack)
    g_bHDTextures = false;    // what a player sees before making a texture pack
  if (g_SnapshotConfig.bGpuStockLook) {
    // ROLLER's own GPU renderer: everything FAT3D adds switched off. The
    // player's other settings (texture filter, anti-aliasing) stay, so both
    // looks differ only by FAT3D's changes.
    g_bHDTextures = false;
    g_bAmbientOcclusion = false;
    g_fSkyGradient = 0.0f;
    g_fSunShade = 0.0f;
    g_fCarShadows = 0.0f;
    g_fCarShine = 0.0f;
    g_fHUDScale = 1.0f;
    g_fHUDMapScale = 1.0f;
  }
}

//-------------------------------------------------------------------------------------------------

int SnapshotGpuWantsFrame(int *piWidth, int *piHeight)
{
  if (!SnapshotGpuActive() || currentreplayframe == g_SnapshotConfig.iGpuLastFrame)
    return 0;
  for (int i = 0; i < g_SnapshotConfig.iNumFrames; ++i) {
    if (g_SnapshotConfig.iFramesAy[i] == currentreplayframe) {
      *piWidth = g_SnapshotConfig.iGpuWidth;
      *piHeight = g_SnapshotConfig.iGpuHeight;
      return 1;
    }
  }
  return 0;
}

//-------------------------------------------------------------------------------------------------

void SnapshotGpuFrameDone(const uint8 *pbyRgba, int iWidth, int iHeight)
{
  if (!SnapshotGpuActive()) return;

  int iWidthWanted, iHeightWanted;
  if (SnapshotGpuWantsFrame(&iWidthWanted, &iHeightWanted)) {
    g_SnapshotConfig.iGpuLastFrame = currentreplayframe;
    g_SnapshotConfig.iCapturedCount++;
    if (pbyRgba) {
      char szPath[512];
      SnapshotBuildPath(szPath, sizeof(szPath), currentreplayframe);
      SDL_CreateDirectory(g_SnapshotConfig.szOutDir);
      if (RollerWriteRgbaPng(szPath, pbyRgba, iWidth, iHeight) != 0) {
        fprintf(stderr, "snapshot: PNG write failed for '%s'\n", szPath);
      } else {
        fprintf(stdout, "snapshot: wrote '%s' (frame %d, %dx%d, %s look)\n", szPath,
                currentreplayframe, iWidth, iHeight,
                g_SnapshotConfig.bGpuStockLook ? "stock" : g_SnapshotConfig.bGpuNoPack ? "nopack" : "ini");
        fflush(stdout);
      }
    } else {
      fprintf(stderr, "snapshot: GPU readback failed for frame %d\n", currentreplayframe);
    }
  }

  if (g_SnapshotConfig.iCapturedCount >= g_SnapshotConfig.iNumFrames ||
      currentreplayframe >= g_SnapshotConfig.iMaxFrame) {
    quit_game = 1;
    racing = 0;
  }
}

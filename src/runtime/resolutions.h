/* The F10 menu's game resolutions, for both hosts (present.c, src/linux/menu.c).
 *
 * The game's own resolution: the INI's [Video] for the next start, and the
 * options in memory (GameOptionsClass, the fields its "Resolution = %d X %d"
 * log line prints) so the next game opens at it and the game's own save on
 * exit does not put the old one back. The memory is only written while it
 * holds a plausible resolution. Needs recomp_target.h first. */
#pragma once

#ifdef RA2_TARGET_GAMEMD
#define RA2_OPT_W_VA (0x00A8EB60u + 0x24)
#else
#define RA2_OPT_W_VA (0x00A40B18u + 0x20)   /* Red Alert 2 */
#endif
#define RA2_OPT_H_VA (RA2_OPT_W_VA + 4)

/* 21:9 too (1720x720, 2560x1080, 3440x1440, 3840x1600): an ultrawide screen at
 * its own size, not a 16:9 game with bars. 1720x720 is half of 3440x1440, so it
 * scales by a whole 2x there and is small enough for HD vehicles. Nothing wider
 * than the presenter's 4096x2160 frame, so no 5120x2160 or 32:9. */
static const int k_res[][2] = { { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1366, 768 }, { 1600, 900 },
                                { 1720, 720 }, { 1920, 1080 }, { 2560, 1080 }, { 2560, 1440 },
                                { 3440, 1440 }, { 3840, 1600 }, { 3840, 2160 } };
#define NRES ((int)(sizeof k_res / sizeof k_res[0]))

static inline int res_plausible(int w, int h) { return w >= 320 && w <= 8192 && h >= 200 && h <= 8192; }

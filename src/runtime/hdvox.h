/* HD voxels: vehicles drawn at 2x or 3x the rest of the picture (hdvox.c). */
#pragma once
#include <stdint.h>

/* on: draw voxel units at the HD scale; dump_dir (or NULL): also write the
 * first 40 renders as BMPs, 1x and Nx. */
void hdvox_configure(int on, const char* dump_dir);

/* The HD render scale, 2 or 3 (ra2_vox_set_scale clamps to [2, 3] and drops
 * the records/stamps/memo built at the old scale on a change). RA2_HD_SCALE
 * overrides it at configure; the F10 menu and ra2.ini set it too. */
int  ra2_vox_scale(void);
void ra2_vox_set_scale(int n);
/* ra2_vox_hd_on: the machinery, and the game's voxel caches off (the lift's
 * patches read it); ra2_vox_hd_show: the player's switch (F10). The
 * presenter keeps the first on for the whole run, so the caches never fill:
 * caches filled while HD was off and then bypassed again drew every turret
 * facing the camera (issue #8). */
extern int ra2_vox_hd_on, ra2_vox_hd_show;

/* The presenter's frame at Nx (N = ra2_vox_scale()): frame16 the 1x frame
 * (16-bit, pitch in bytes), out Nw x Nh BGRX, with the units' Nx pixels where
 * they still show. */
void hdvox_compose(const uint8_t* frame16, int pitch, int w, int h, uint32_t* out);

/* The HD layer as a sparse overlay, for a presenter that scales the 1x frame
 * itself (SDL_Renderer, D3D11): the CPU no longer upscales the whole frame
 * (95% of hdvox_compose at 1080p). ov is an Nw x Nh ARGB8888 buffer (N =
 * ra2_vox_scale()) the caller keeps zeroed between resolution changes; this
 * fills the units' Nx pixels opaque where they still show and leaves the rest
 * transparent. *dx..*dh get the Nx sub-rect that changed since the last call
 * (upload only that). Returns the scale N (2 or 3) if HD is live -- draw the
 * Nw x Nh overlay over the scaled 1x base -- or 0 for none. */
int hdvox_overlay(const uint8_t* frame16, int pitch, int w, int h, uint32_t* ov,
                  int* dx, int* dy, int* dw, int* dh);

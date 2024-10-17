/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Vita context. */
#include "../../retroarch.h"
#include "../common/egl_common.h"
#include "../../verbosity.h"

#define ATTR_VITA_WIDTH 960
#define ATTR_VITA_HEIGHT 544

#define SCE_KERNEL_CPU_MASK_SHIFT  (16)
#define SCE_KERNEL_CPU_MASK_USER_0 (0x01 << SCE_KERNEL_CPU_MASK_SHIFT)
#define SCE_KERNEL_CPU_MASK_USER_1 (0x02 << SCE_KERNEL_CPU_MASK_SHIFT)
#define SCE_KERNEL_CPU_MASK_USER_2 (0x04 << SCE_KERNEL_CPU_MASK_SHIFT)

typedef struct
{
   egl_ctx_data_t egl;
   Psp2NativeWindow* native_window;
   bool resize;
   unsigned width, height;
   float refresh_rate;
} vita_ctx_data_t;

static void vita_swap_interval(void *data, int interval) {
   vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)data;
   egl_set_swap_interval(&ctx_vita->egl, interval);
}

static void vita_get_video_size(void *data, unsigned *width, unsigned *height) {
   *width  = ATTR_VITA_WIDTH;
   *height = ATTR_VITA_HEIGHT;
}

static void vita_check_window(void *data, bool *quit, bool *resize, unsigned *width, unsigned *height) {
   unsigned new_width, new_height;

   vita_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height) {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   *quit = (bool)false;
}

static void vita_swap_buffers(void *data) {
   vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)data;
   egl_swap_buffers(&ctx_vita->egl);
}

static void vita_destroy(void *data) {
  vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)data;

  if (ctx_vita) {
     egl_destroy(&ctx_vita->egl);
     ctx_vita->resize = false;
     free(ctx_vita);
  }
}

static bool vita_set_video_mode(void *data, unsigned width, unsigned height, bool fullscreen) {
  /* Create an EGL rendering context */
   static const EGLint ctx_attr_list[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   Psp2NativeWindow win;
   win.type = PSP2_DRAWABLE_TYPE_WINDOW;
   win.windowSize = PSP2_WINDOW_960X544;

   win.numFlipBuffers = 2;
   win.flipChainThrdAffinity = SCE_KERNEL_CPU_MASK_USER_1;

   vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)data;
   ctx_vita->width           = ATTR_VITA_WIDTH;
   ctx_vita->height          = ATTR_VITA_HEIGHT;
   ctx_vita->native_window   = &win;
   ctx_vita->refresh_rate    = 60;

   if (!egl_create_context(&ctx_vita->egl, ctx_attr_list)) {
      RARCH_WARN("There was an issue creating the context");
      goto error;
   }

   if (!egl_create_surface(&ctx_vita->egl, ctx_vita->native_window)) {
      RARCH_WARN("There was an issue creating the surface: (%x)\n", eglGetError());
      goto error;
   }

   return true;

error:
   egl_report_error();
   vita_destroy(data);
   return false;
}


static void vita_input_driver(void *data, const char *name, input_driver_t **input, void **input_data) {
   *input      = NULL;
   *input_data = NULL;
}

static bool vita_has_focus(void *data) { return true; }
static bool vita_suppress_screensaver(void *data, bool enable) { return false; }
static enum gfx_ctx_api vita_get_api(void *data) { return GFX_CTX_OPENGL_ES_API; }

static bool vita_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor) {
   if (api != GFX_CTX_OPENGL_ES_API) {
      return false;
   }

   if (egl_bind_api(EGL_OPENGL_ES_API) == EGL_FALSE) {
      RARCH_WARN("EGL Bind API Error: (%x)\n", eglGetError());
      return false;
   }

   return true;
}

static void vita_show_mouse(void *data, bool state) { }

static void vita_bind_hw_render(void *data, bool enable) {
   vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)data;
   egl_bind_hw_render(&ctx_vita->egl, enable);
}

static void *vita_init(void *video_driver) {
   EGLint n = 0;
   EGLint major, minor;

   static const EGLint attribs[] = {
      EGL_BUFFER_SIZE,     EGL_DONT_CARE,
      EGL_RED_SIZE,        8,
      EGL_GREEN_SIZE,      8,
      EGL_BLUE_SIZE,       8,
      EGL_ALPHA_SIZE,      8,
      EGL_STENCIL_SIZE,    8,
      EGL_DEPTH_SIZE,      16,
      EGL_SAMPLES,         4,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE
   };

   vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)calloc(1, sizeof(*ctx_vita));

   if (!ctx_vita) {
      return NULL;
   }

    if (!egl_init_context(&ctx_vita->egl, EGL_NONE, EGL_DEFAULT_DISPLAY, &major, &minor, &n, attribs, NULL)) {
      egl_report_error();
      RARCH_WARN("[VITA]: EGL error: %x.\n", eglGetError());
      goto error;
    }

   return ctx_vita;

error:
   vita_destroy(video_driver);
   return NULL;
}

static uint32_t vita_get_flags(void *data) {
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void vita_set_flags(void *data, uint32_t flags) { }

static float vita_get_refresh_rate(void *data) {
   vita_ctx_data_t *ctx_vita = (vita_ctx_data_t *)data;
   return ctx_vita->refresh_rate;
}

const gfx_ctx_driver_t vita_ctx = {
   vita_init,
   vita_destroy,
   vita_get_api,
   vita_bind_api,
   vita_swap_interval,
   vita_set_video_mode,
   vita_get_video_size,
   vita_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   NULL, /* update_title */
   vita_check_window,
   NULL, /* set_resize */
   vita_has_focus,
   vita_suppress_screensaver,
   false, /* has_windowed */
   vita_swap_buffers,
   vita_input_driver,
   egl_get_proc_address,
   NULL, /* image_buffer_init maybe we support this with IMGEGL*/
   NULL, /* image_buffer_write */
   vita_show_mouse,
   "vita",
   vita_get_flags,
   vita_set_flags,
   vita_bind_hw_render,
   NULL, /* get_context_data */
   NULL /* make_current */
};

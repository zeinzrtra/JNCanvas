#pragma once

#include <obs.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
  #ifdef BUILDING_JN_CANVAS
    #define JN_CANVAS_EXPORT __declspec(dllexport)
  #else
    #define JN_CANVAS_EXPORT __declspec(dllimport)
  #endif
#else
  #define JN_CANVAS_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Checks if JNCanvas is currently initialized and active in OBS.
 */
JN_CANVAS_EXPORT bool jn_canvas_is_active(void);

/**
 * Returns the secondary canvas view pointer (obs_view_t).
 */
JN_CANVAS_EXPORT obs_view_t *jn_canvas_get_view(void);

/**
 * Returns the secondary canvas root output source.
 * This can be directly tapped by encoders / stream outputs in JNMultiStream.
 */
JN_CANVAS_EXPORT obs_source_t *jn_canvas_get_output_source(void);

/**
 * Gets the current configured resolution of the secondary canvas.
 */
JN_CANVAS_EXPORT void jn_canvas_get_resolution(uint32_t *width, uint32_t *height);

/**
 * Gets the currently active scene in JNCanvas.
 */
JN_CANVAS_EXPORT obs_scene_t *jn_canvas_get_current_scene(void);

/**
 * Adds an existing source into a specific JNCanvas scene (Zero GPU duplicate capture).
 */
JN_CANVAS_EXPORT bool jn_canvas_clone_source_to_scene(obs_source_t *source, const char *target_scene_name);

#ifdef __cplusplus
}
#endif

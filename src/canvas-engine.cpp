#include "canvas-engine.hpp"
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/math-defs.h>
#include <iostream>

CanvasEngine &CanvasEngine::instance() {
    static CanvasEngine inst;
    return inst;
}

CanvasEngine::CanvasEngine() {}

CanvasEngine::~CanvasEngine() {
    shutdown();
}

bool CanvasEngine::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_initialized_) return true;

    // Create secondary view
    view_ = obs_view_create();
    if (!view_) {
        blog(LOG_ERROR, "[JNCanvas] Failed to create secondary obs_view_t!");
        return false;
    }

    // Create a default vertical scene
    obs_scene_t *default_scene = obs_scene_create("JNCanvas Default");
    if (default_scene) {
        scenes_.push_back(default_scene);
        current_scene_ = default_scene;
        obs_source_t *scene_source = obs_scene_get_source(default_scene);
        obs_view_set_source(view_, 0, scene_source);
    }

    is_initialized_ = true;
    blog(LOG_INFO, "[JNCanvas] Secondary Canvas Engine initialized successfully (%ux%u)", width_, height_);
    return true;
}

void CanvasEngine::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized_) return;

    if (view_) {
        obs_view_set_source(view_, 0, nullptr);
        obs_view_destroy(view_);
        view_ = nullptr;
    }

    for (obs_scene_t *scene : scenes_) {
        if (scene) {
            obs_scene_release(scene);
        }
    }
    scenes_.clear();
    current_scene_ = nullptr;

    is_initialized_ = false;
    blog(LOG_INFO, "[JNCanvas] Secondary Canvas Engine shut down.");
}

void CanvasEngine::set_resolution(uint32_t w, uint32_t h) {
    std::lock_guard<std::mutex> lock(mutex_);
    width_ = (w > 0) ? w : 1080;
    height_ = (h > 0) ? h : 1920;
    blog(LOG_INFO, "[JNCanvas] Canvas resolution set to %ux%u", width_, height_);
}

obs_source_t *CanvasEngine::output_source() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_scene_) return nullptr;
    return obs_scene_get_source(current_scene_);
}

void CanvasEngine::set_current_scene(obs_scene_t *scene) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!scene || scene == current_scene_) return;

    current_scene_ = scene;
    if (view_) {
        obs_source_t *src = obs_scene_get_source(scene);
        obs_view_set_source(view_, 0, src);
    }
}

obs_scene_t *CanvasEngine::create_scene(const std::string &name) {
    std::lock_guard<std::mutex> lock(mutex_);
    obs_scene_t *scene = obs_scene_create(name.c_str());
    if (scene) {
        scenes_.push_back(scene);
        if (!current_scene_) {
            set_current_scene(scene);
        }
    }
    return scene;
}

void CanvasEngine::remove_scene(obs_scene_t *scene) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(scenes_.begin(), scenes_.end(), scene);
    if (it != scenes_.end()) {
        scenes_.erase(it);
        if (current_scene_ == scene) {
            current_scene_ = scenes_.empty() ? nullptr : scenes_.front();
            if (view_) {
                obs_source_t *src = current_scene_ ? obs_scene_get_source(current_scene_) : nullptr;
                obs_view_set_source(view_, 0, src);
            }
        }
        obs_scene_release(scene);
    }
}

std::vector<obs_scene_t *> CanvasEngine::get_all_scenes() {
    std::lock_guard<std::mutex> lock(mutex_);
    return scenes_;
}

bool CanvasEngine::clone_source_to_scene(obs_source_t *source, const std::string &scene_name) {
    if (!source) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    obs_scene_t *target_scene = nullptr;
    for (auto *s : scenes_) {
        obs_source_t *s_src = obs_scene_get_source(s);
        if (s_src && scene_name == obs_source_get_name(s_src)) {
            target_scene = s;
            break;
        }
    }

    if (!target_scene) {
        // Create if doesn't exist yet
        target_scene = obs_scene_create(scene_name.c_str());
        if (!target_scene) return false;
        scenes_.push_back(target_scene);
    }

    // Add source reference directly without re-capture overhead
    obs_sceneitem_t *item = obs_scene_add(target_scene, source);
    if (item) {
        // Auto scale to fit or center inside vertical canvas
        struct vec2 pos;
        vec2_set(&pos, 0.0f, 0.0f);
        obs_sceneitem_set_pos(item, &pos);
        return true;
    }
    return false;
}

bool CanvasEngine::clone_source_to_main_scene(obs_source_t *source, const std::string &main_scene_name) {
    if (!source) return false;

    obs_source_t *main_scene_src = obs_get_source_by_name(main_scene_name.c_str());
    if (!main_scene_src) return false;

    obs_scene_t *main_scene = obs_scene_from_source(main_scene_src);
    bool success = false;
    if (main_scene) {
        obs_sceneitem_t *item = obs_scene_add(main_scene, source);
        success = (item != nullptr);
    }
    obs_source_release(main_scene_src);
    return success;
}

void CanvasEngine::render(uint32_t cx, uint32_t cy) {
    if (!view_ || cx == 0 || cy == 0) return;

    // Render secondary canvas view with proper aspect ratio letterboxing
    float scale_x = (float)cx / (float)width_;
    float scale_y = (float)cy / (float)height_;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    float draw_w = (float)width_ * scale;
    float draw_h = (float)height_ * scale;
    float offset_x = ((float)cx - draw_w) * 0.5f;
    float offset_y = ((float)cy - draw_h) * 0.5f;

    gs_matrix_push();
    gs_matrix_translate3f(offset_x, offset_y, 0.0f);
    gs_matrix_scale3f(scale, scale, 1.0f);

    obs_view_render(view_);

    gs_matrix_pop();
}

// ============================================================================
// Exported C-API Implementation (For JNMultiStream and third-party tools)
// ============================================================================

extern "C" {

bool jn_canvas_is_active(void) {
    return CanvasEngine::instance().is_active();
}

obs_view_t *jn_canvas_get_view(void) {
    return CanvasEngine::instance().view();
}

obs_source_t *jn_canvas_get_output_source(void) {
    return CanvasEngine::instance().output_source();
}

void jn_canvas_get_resolution(uint32_t *width, uint32_t *height) {
    if (width) *width = CanvasEngine::instance().width();
    if (height) *height = CanvasEngine::instance().height();
}

obs_scene_t *jn_canvas_get_current_scene(void) {
    return CanvasEngine::instance().current_scene();
}

bool jn_canvas_clone_source_to_scene(obs_source_t *source, const char *target_scene_name) {
    if (!source || !target_scene_name) return false;
    return CanvasEngine::instance().clone_source_to_scene(source, target_scene_name);
}

}

#pragma once

#include <obs.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include "jn-canvas-export.h"

class CanvasEngine {
public:
    static CanvasEngine &instance();

    bool init();
    void shutdown();

    // Canvas Properties
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    void set_resolution(uint32_t w, uint32_t h);

    // View & Source
    obs_view_t *view() const { return view_; }
    obs_source_t *output_source() const;

    // Scene Management
    obs_scene_t *current_scene() const { return current_scene_; }
    void set_current_scene(obs_scene_t *scene);
    obs_scene_t *create_scene(const std::string &name);
    void remove_scene(obs_scene_t *scene);
    std::vector<obs_scene_t *> get_all_scenes();

    // Source Cloning
    bool clone_source_to_scene(obs_source_t *source, const std::string &scene_name);
    bool clone_source_to_main_scene(obs_source_t *source, const std::string &main_scene_name);

    // State
    bool is_active() const { return is_initialized_; }

    // Rendering control
    void render(uint32_t cx, uint32_t cy);

private:
    CanvasEngine();
    ~CanvasEngine();

    CanvasEngine(const CanvasEngine &) = delete;
    CanvasEngine &operator=(const CanvasEngine &) = delete;

    mutable std::mutex mutex_;
    bool is_initialized_ = false;

    uint32_t width_ = 1080;
    uint32_t height_ = 1920;

    obs_view_t *view_ = nullptr;
    obs_scene_t *current_scene_ = nullptr;
    std::vector<obs_scene_t *> scenes_;
};


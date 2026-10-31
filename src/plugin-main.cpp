#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include "canvas-engine.hpp"
#include "record-engine.hpp"
#include "ui/canvas-preview-dock.hpp"
#include "ui/canvas-control-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("jn-canvas", "en-US")

static CanvasPreviewDock *preview_dock = nullptr;
static CanvasControlDock *control_dock = nullptr;

MODULE_EXPORT const char *obs_module_name(void) {
    return "JNCanvas";
}

MODULE_EXPORT const char *obs_module_description(void) {
    return "Secondary / Vertical Canvas Plugin with Dedicated Scenes and Per-Canvas Tools";
}

static void on_frontend_event(enum obs_frontend_event event, void *private_data) {
    Q_UNUSED(private_data);
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        QMainWindow *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
        if (!main_win) return;

        // Create Preview Dock (can be placed on left, right, top, or floating)
        preview_dock = new CanvasPreviewDock(main_win);
        obs_frontend_add_dock_by_id("JNCanvasPreviewDock", "Canvas 2 - Preview", preview_dock);
        if (preview_dock->parentWidget()) {
            preview_dock->parentWidget()->setVisible(true);
        }

        // Create Control Dock (Canvas 2 Tabbed Scenes & Sources)
        control_dock = new CanvasControlDock(main_win);
        obs_frontend_add_dock_by_id("JNCanvasControlDock", "Canvas 2", control_dock);
        if (control_dock->parentWidget()) {
            control_dock->parentWidget()->setVisible(true);
        }

        blog(LOG_INFO, "[JNCanvas] Preview and Control docks attached to OBS Studio successfully.");
    } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED ||
               event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        if (control_dock) {
            control_dock->refresh_sources();
        }
    }
}

bool obs_module_load(void) {
    blog(LOG_INFO, "[JNCanvas] Loading JNCanvas plugin v1.0.0...");

    // Initialize core canvas engine
    if (!CanvasEngine::instance().init()) {
        blog(LOG_ERROR, "[JNCanvas] Failed to initialize CanvasEngine!");
        return false;
    }

    // Initialize record engine
    RecordEngine::instance().init();

    // Hook frontend events to load UI once OBS window is ready
    obs_frontend_add_event_callback(on_frontend_event, nullptr);

    blog(LOG_INFO, "[JNCanvas] JNCanvas plugin loaded successfully.");
    return true;
}

void obs_module_unload(void) {
    blog(LOG_INFO, "[JNCanvas] Unloading JNCanvas plugin...");

    obs_frontend_remove_event_callback(on_frontend_event, nullptr);

    obs_frontend_remove_dock("JNCanvasPreviewDock");
    obs_frontend_remove_dock("JNCanvasControlDock");

    RecordEngine::instance().shutdown();
    CanvasEngine::instance().shutdown();

    blog(LOG_INFO, "[JNCanvas] JNCanvas plugin unloaded.");
}


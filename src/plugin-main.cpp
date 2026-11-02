#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include "canvas-engine.hpp"
#include "record-engine.hpp"
#include "ui/canvas-view-dock.hpp"
#include "ui/canvas-scenes-dock.hpp"
#include "ui/canvas-sources-dock.hpp"
#include "ui/canvas-central-view.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("jn-canvas", "en-US")

static CanvasCentralView *central_view = nullptr;
static CanvasViewDock *canvas_dock = nullptr;
static CanvasScenesDock *scenes_dock = nullptr;
static CanvasSourcesDock *sources_dock = nullptr;

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

        // 1. Integrated Central Canvas Preview (Clean layout, NO top tabs)
        central_view = new CanvasCentralView(main_win);
        central_view->init();

        // 2. Native "Canvas" Dock (Switch between Canvas 1, Canvas 2, Keduanya)
        canvas_dock = new CanvasViewDock(central_view, main_win);
        obs_frontend_add_dock_by_id("JNCanvasViewDock", "Canvas", canvas_dock);

        // 3. Native Scenes Dock for Canvas 2 ("Scenes (Canvas 2)")
        scenes_dock = new CanvasScenesDock(main_win);
        obs_frontend_add_dock_by_id("JNCanvasScenesDock", "Scenes (Canvas 2)", scenes_dock);

        // 4. Native Sources Dock for Canvas 2 ("Sources (Canvas 2)")
        sources_dock = new CanvasSourcesDock(main_win);
        obs_frontend_add_dock_by_id("JNCanvasSourcesDock", "Sources (Canvas 2)", sources_dock);

        // Connect scene selection in Scenes dock to update Sources dock
        QObject::connect(scenes_dock, &CanvasScenesDock::scene_changed,
                         sources_dock, &CanvasSourcesDock::on_scene_changed);

        // Connect canvas interactive selection with Sources dock
        if (central_view && central_view->display()) {
            QObject::connect(central_view->display(), &UnifiedCanvasDisplay::item_selected,
                             sources_dock, &CanvasSourcesDock::select_sceneitem);
            QObject::connect(sources_dock, &CanvasSourcesDock::sceneitem_selected,
                             central_view->display(), &UnifiedCanvasDisplay::select_item);
        }

        blog(LOG_INFO, "[JNCanvas] Canvas dock, Scenes dock, and Sources dock registered successfully.");
    } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED ||
               event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        if (scenes_dock) {
            scenes_dock->refresh_scenes();
        }
        if (sources_dock) {
            sources_dock->refresh_sources();
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

    obs_frontend_remove_dock("JNCanvasViewDock");
    obs_frontend_remove_dock("JNCanvasScenesDock");
    obs_frontend_remove_dock("JNCanvasSourcesDock");

    if (central_view) {
        delete central_view;
        central_view = nullptr;
    }

    canvas_dock = nullptr;
    scenes_dock = nullptr;
    sources_dock = nullptr;

    RecordEngine::instance().shutdown();
    CanvasEngine::instance().shutdown();

    blog(LOG_INFO, "[JNCanvas] JNCanvas plugin unloaded.");
}

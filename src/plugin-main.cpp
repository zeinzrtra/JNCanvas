#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include "canvas-engine.hpp"
#include "record-engine.hpp"
#include "ui/canvas-preview-dock.hpp"
#include "ui/canvas-control-dock.hpp"
#include "ui/canvas-central-view.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("jn-canvas", "en-US")

static CanvasCentralView *central_view = nullptr;
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

        // 1. Integrated Central Canvas Preview (Tabbed View + Side-by-Side Dual View)
        central_view = new CanvasCentralView(main_win);
        central_view->init();

        // 2. Control Dock (Canvas 2 Tabbed Scenes & Sources)
        control_dock = new CanvasControlDock(main_win);
        obs_frontend_add_dock_by_id("JNCanvasControlDock", "Canvas 2", control_dock);

        // Auto-tabify with native scenesDock so it smoothly integrates into the bottom dock area
        QDockWidget *scenes_dock = main_win->findChild<QDockWidget *>("scenesDock");
        QDockWidget *control_dock_widget = qobject_cast<QDockWidget *>(control_dock->parentWidget());
        if (scenes_dock && control_dock_widget) {
            main_win->tabifyDockWidget(scenes_dock, control_dock_widget);
            scenes_dock->raise(); // keep native scenes active by default
        } else if (control_dock_widget) {
            control_dock_widget->setVisible(true);
        }

        // 3. Optional detached preview dock (starts hidden so no floating window clutter)
        preview_dock = new CanvasPreviewDock(main_win);
        obs_frontend_add_dock_by_id("JNCanvasPreviewDock", "Canvas 2 - Preview (Detached)", preview_dock);
        if (preview_dock->parentWidget()) {
            preview_dock->parentWidget()->setVisible(false);
        }

        blog(LOG_INFO, "[JNCanvas] Central Canvas View and Control docks attached to OBS Studio successfully.");
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

    if (central_view) {
        delete central_view;
        central_view = nullptr;
    }

    RecordEngine::instance().shutdown();
    CanvasEngine::instance().shutdown();

    blog(LOG_INFO, "[JNCanvas] JNCanvas plugin unloaded.");
}


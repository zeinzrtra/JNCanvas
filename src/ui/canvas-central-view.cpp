#include "canvas-central-view.hpp"
#include <obs-module.h>
#include <QGridLayout>
#include <QBoxLayout>

CanvasCentralView::CanvasCentralView(QMainWindow *main_window, CanvasDock *dock, QObject *parent)
    : QObject(parent), main_win_(main_window), canvas_dock_(dock) {}

CanvasCentralView::~CanvasCentralView() {
    if (native_preview_) {
        native_preview_->show();
    }
}

bool CanvasCentralView::init() {
    if (!main_win_ || !canvas_dock_) return false;

    native_preview_ = main_win_->findChild<QWidget *>("preview");
    preview_container_ = main_win_->findChild<QWidget *>("previewContainer");

    if (!native_preview_ && !preview_container_) {
        blog(LOG_WARNING, "[JNCanvas] Could not locate central preview widgets in OBS main window.");
        return false;
    }

    setup_central_preview();
    set_mode(CanvasViewMode::MainOnly);

    blog(LOG_INFO, "[JNCanvas] Central clean unified canvas view initialized successfully.");
    return true;
}

void CanvasCentralView::setup_central_preview() {
    QWidget *parent_w = preview_container_ ? preview_container_ : main_win_;
    canvas_dock_->setParent(parent_w);
    canvas_dock_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    canvas_dock_->setMinimumSize(32, 32);

    bool inserted = false;
    if (preview_container_) {
        QGridLayout *grid = preview_container_->findChild<QGridLayout *>("gridLayout");
        if (grid) {
            grid->addWidget(canvas_dock_, 1, 1);
            grid->setColumnStretch(0, 1);
            grid->setColumnStretch(1, 1);
            inserted = true;
            blog(LOG_INFO, "[JNCanvas] canvas_dock_ added directly to gridLayout at (1, 1)");
        } else {
            const auto layouts = preview_container_->findChildren<QLayout *>();
            for (QLayout *l : layouts) {
                if (native_preview_ && l->indexOf(native_preview_) != -1) {
                    if (auto *gl = qobject_cast<QGridLayout *>(l)) {
                        gl->addWidget(canvas_dock_, 1, 1);
                        gl->setColumnStretch(0, 1);
                        gl->setColumnStretch(1, 1);
                        inserted = true;
                        blog(LOG_INFO, "[JNCanvas] canvas_dock_ added to detected QGridLayout at (1, 1)");
                        break;
                    } else if (auto *bl = qobject_cast<QBoxLayout *>(l)) {
                        int idx = bl->indexOf(native_preview_);
                        bl->insertWidget(idx + 1, canvas_dock_, 1);
                        inserted = true;
                        blog(LOG_INFO, "[JNCanvas] canvas_dock_ inserted into detected QBoxLayout at %d", idx + 1);
                        break;
                    }
                }
            }
        }
    }

    if (!inserted) {
        QHBoxLayout *preview_layout = main_win_->findChild<QHBoxLayout *>("previewLayout");
        if (preview_layout) {
            preview_layout->addWidget(canvas_dock_, 1);
        } else if (preview_container_ && preview_container_->layout()) {
            preview_container_->layout()->addWidget(canvas_dock_);
        }
    }

    canvas_dock_->hide();
}

void CanvasCentralView::set_mode(CanvasViewMode mode) {
    current_mode_ = mode;
    apply_mode(mode);
    emit mode_changed(mode);
}

void CanvasCentralView::apply_mode(CanvasViewMode mode) {
    blog(LOG_INFO, "[JNCanvas] apply_mode switching to mode=%d", (int)mode);
    switch (mode) {
    case CanvasViewMode::MainOnly:
        if (canvas_dock_) canvas_dock_->hide();
        if (native_preview_) native_preview_->show();
        break;
    case CanvasViewMode::Canvas2Only:
        if (native_preview_) native_preview_->hide();
        if (canvas_dock_) {
            canvas_dock_->show();
            if (canvas_dock_->GetCurrentScene()) {
                bool has_items = false;
                obs_scene_enum_items(
                    canvas_dock_->GetCurrentScene(),
                    [](obs_scene_t *, obs_sceneitem_t *, void *p) -> bool {
                        *static_cast<bool *>(p) = true;
                        return false;
                    },
                    &has_items);
                if (!has_items) {
                    canvas_dock_->CloneFromMain();
                }
            }
        }
        break;
    case CanvasViewMode::DualView:
        if (native_preview_) native_preview_->show();
        if (canvas_dock_) {
            canvas_dock_->show();
            if (canvas_dock_->GetCurrentScene()) {
                bool has_items = false;
                obs_scene_enum_items(
                    canvas_dock_->GetCurrentScene(),
                    [](obs_scene_t *, obs_sceneitem_t *, void *p) -> bool {
                        *static_cast<bool *>(p) = true;
                        return false;
                    },
                    &has_items);
                if (!has_items) {
                    canvas_dock_->CloneFromMain();
                }
            }
        }
        break;
    }
}

#include "canvas-central-view.hpp"
#include <obs-module.h>
#include <QGridLayout>
#include <QBoxLayout>

CanvasCentralView::CanvasCentralView(QMainWindow *main_window, QObject *parent)
    : QObject(parent), main_win_(main_window) {}

CanvasCentralView::~CanvasCentralView() {
    if (native_preview_) {
        native_preview_->show();
    }
    if (unified_display_) {
        delete unified_display_;
        unified_display_ = nullptr;
    }
}

bool CanvasCentralView::init() {
    if (!main_win_) return false;

    // Locate OBS native preview widget and its parent container
    native_preview_ = main_win_->findChild<QWidget *>("preview");
    preview_container_ = main_win_->findChild<QWidget *>("previewContainer");

    if (!native_preview_ && !preview_container_) {
        blog(LOG_WARNING, "[JNCanvas] Could not locate central preview widgets in OBS main window.");
        return false;
    }

    setup_unified_display();

    // Default to Canvas 1 (native OBS preview) on startup
    set_mode(CanvasViewMode::MainOnly);

    blog(LOG_INFO, "[JNCanvas] Central clean unified canvas view initialized successfully.");
    return true;
}

void CanvasCentralView::setup_unified_display() {
    QWidget *parent_w = preview_container_ ? preview_container_ : main_win_;
    unified_display_ = new UnifiedCanvasDisplay(parent_w);
    unified_display_->setObjectName("JNUnifiedCanvasDisplay");
    unified_display_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    unified_display_->setMinimumSize(32, 32);

    bool inserted = false;
    if (preview_container_) {
        // Look for the gridLayout that contains preview
        QGridLayout *grid = preview_container_->findChild<QGridLayout *>("gridLayout");
        if (grid) {
            grid->addWidget(unified_display_, 1, 0, 1, 2);
            inserted = true;
            blog(LOG_INFO, "[JNCanvas] unified_display_ added directly to gridLayout at (1, 0, 1, 2)");
        } else {
            // Find any layout containing native_preview_
            const auto layouts = preview_container_->findChildren<QLayout *>();
            for (QLayout *l : layouts) {
                if (native_preview_ && l->indexOf(native_preview_) != -1) {
                    if (auto *gl = qobject_cast<QGridLayout *>(l)) {
                        gl->addWidget(unified_display_, 1, 0, 1, 2);
                        inserted = true;
                        blog(LOG_INFO, "[JNCanvas] unified_display_ added to detected QGridLayout");
                        break;
                    } else if (auto *bl = qobject_cast<QBoxLayout *>(l)) {
                        int idx = bl->indexOf(native_preview_);
                        bl->insertWidget(idx, unified_display_, 1);
                        inserted = true;
                        blog(LOG_INFO, "[JNCanvas] unified_display_ inserted into detected QBoxLayout at %d", idx);
                        break;
                    }
                }
            }
        }
    }

    if (!inserted) {
        QHBoxLayout *preview_layout = main_win_->findChild<QHBoxLayout *>("previewLayout");
        if (preview_layout) {
            preview_layout->addWidget(unified_display_, 1);
            blog(LOG_INFO, "[JNCanvas] unified_display_ added to previewLayout as fallback");
        } else if (preview_container_ && preview_container_->layout()) {
            preview_container_->layout()->addWidget(unified_display_);
            blog(LOG_INFO, "[JNCanvas] unified_display_ added to previewContainer layout as fallback");
        }
    }

    unified_display_->hide();
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
        if (unified_display_) unified_display_->hide();
        if (native_preview_) native_preview_->show();
        break;
    case CanvasViewMode::Canvas2Only:
        if (native_preview_) native_preview_->hide();
        if (unified_display_) {
            unified_display_->show();
            unified_display_->set_mode(UnifiedViewMode::Canvas2Only);
        }
        break;
    case CanvasViewMode::DualView:
        if (native_preview_) native_preview_->hide();
        if (unified_display_) {
            unified_display_->show();
            unified_display_->set_mode(UnifiedViewMode::DualView);
        }
        break;
    }
}

#include "canvas-central-view.hpp"
#include <obs-module.h>

CanvasCentralView::CanvasCentralView(QMainWindow *main_window, QObject *parent)
    : QObject(parent), main_win_(main_window) {}

CanvasCentralView::~CanvasCentralView() {
    if (unified_display_) {
        unified_display_->deleteLater();
        unified_display_ = nullptr;
    }
}

bool CanvasCentralView::init() {
    if (!main_win_) return false;

    canvas_editor_ = main_win_->findChild<QWidget *>("canvasEditor");
    preview_layout_ = main_win_->findChild<QHBoxLayout *>("previewLayout");
    main_preview_container_ = main_win_->findChild<QWidget *>("previewContainer");

    if (!canvas_editor_ || !preview_layout_ || !main_preview_container_) {
        blog(LOG_WARNING, "[JNCanvas] Could not locate central preview widgets in OBS main window.");
        return false;
    }

    setup_unified_display();

    // Default to MainOnly on startup (clean native OBS preview)
    set_mode(CanvasViewMode::MainOnly);

    blog(LOG_INFO, "[JNCanvas] Central clean unified canvas view initialized successfully.");
    return true;
}

void CanvasCentralView::setup_unified_display() {
    unified_display_ = new UnifiedCanvasDisplay(canvas_editor_);
    unified_display_->setObjectName("JNUnifiedCanvasDisplay");

    // Add to preview layout alongside main_preview_container_
    preview_layout_->addWidget(unified_display_, 1);
    unified_display_->hide();
}

void CanvasCentralView::set_mode(CanvasViewMode mode) {
    current_mode_ = mode;
    apply_mode(mode);
    emit mode_changed(mode);
}

void CanvasCentralView::apply_mode(CanvasViewMode mode) {
    switch (mode) {
    case CanvasViewMode::MainOnly:
        if (unified_display_) unified_display_->hide();
        if (main_preview_container_) main_preview_container_->show();
        break;
    case CanvasViewMode::Canvas2Only:
        if (main_preview_container_) main_preview_container_->hide();
        if (unified_display_) {
            unified_display_->show();
            unified_display_->set_mode(UnifiedViewMode::Canvas2Only);
        }
        break;
    case CanvasViewMode::DualView:
        if (main_preview_container_) main_preview_container_->hide();
        if (unified_display_) {
            unified_display_->show();
            unified_display_->set_mode(UnifiedViewMode::DualView);
        }
        break;
    }
}

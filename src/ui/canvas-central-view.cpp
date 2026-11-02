#include "canvas-central-view.hpp"
#include <obs-module.h>
#include <QBoxLayout>

CanvasCentralView::CanvasCentralView(QMainWindow *main_window, QObject *parent)
    : QObject(parent), main_win_(main_window) {}

CanvasCentralView::~CanvasCentralView() {
    if (preview_container_) {
        preview_container_->show();
    }
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

    // Find the inner preview display widget and its parent container
    native_preview_ = main_win_->findChild<QWidget *>("preview");
    preview_container_ = main_win_->findChild<QWidget *>("previewContainer");

    if (!native_preview_ && !preview_container_) {
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
    QWidget *parent_widget = preview_container_ ? preview_container_ : main_win_;
    unified_display_ = new UnifiedCanvasDisplay(parent_widget);
    unified_display_->setObjectName("JNUnifiedCanvasDisplay");

    // Insert directly into previewContainer's layout in place of native preview,
    // preserving the zoom toolbar below it and keeping properties / controls perfectly aligned
    if (preview_container_ && preview_container_->layout()) {
        QBoxLayout *box_layout = qobject_cast<QBoxLayout *>(preview_container_->layout());
        if (box_layout && native_preview_) {
            int idx = box_layout->indexOf(native_preview_);
            box_layout->insertWidget(idx, unified_display_, 1);
        } else {
            preview_container_->layout()->addWidget(unified_display_);
        }
    } else {
        QHBoxLayout *preview_layout = main_win_->findChild<QHBoxLayout *>("previewLayout");
        if (preview_layout) {
            preview_layout->addWidget(unified_display_, 1);
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
    switch (mode) {
    case CanvasViewMode::MainOnly:
        if (unified_display_) unified_display_->hide();
        if (native_preview_) native_preview_->show();
        if (preview_container_) preview_container_->show();
        break;
    case CanvasViewMode::Canvas2Only:
        if (preview_container_) preview_container_->show();
        if (native_preview_) native_preview_->hide();
        if (unified_display_) {
            unified_display_->show();
            unified_display_->set_mode(UnifiedViewMode::Canvas2Only);
        }
        break;
    case CanvasViewMode::DualView:
        if (preview_container_) preview_container_->show();
        if (native_preview_) native_preview_->hide();
        if (unified_display_) {
            unified_display_->show();
            unified_display_->set_mode(UnifiedViewMode::DualView);
        }
        break;
    }
}

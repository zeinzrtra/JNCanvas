#include "canvas-central-view.hpp"
#include <obs-module.h>
#include <QBoxLayout>

CanvasCentralView::CanvasCentralView(QMainWindow *main_window, QObject *parent)
    : QObject(parent), main_win_(main_window) {}

CanvasCentralView::~CanvasCentralView() {
    if (stack_widget_ && preview_container_ && native_preview_) {
        QBoxLayout *layout = qobject_cast<QBoxLayout *>(preview_container_->layout());
        int idx = layout ? layout->indexOf(stack_widget_) : -1;

        // Restore native preview back to its original parent and layout
        stack_widget_->removeWidget(native_preview_);
        native_preview_->setParent(preview_container_);
        if (layout && idx >= 0) {
            layout->insertWidget(idx, native_preview_, 1);
        }
        native_preview_->show();

        // Delete stack widget and its child unified_display_
        delete stack_widget_;
        stack_widget_ = nullptr;
        unified_display_ = nullptr;
    } else if (unified_display_) {
        delete unified_display_;
        unified_display_ = nullptr;
    }
}

bool CanvasCentralView::init() {
    if (!main_win_) return false;

    // Locate OBS native preview widget and its parent container
    native_preview_ = main_win_->findChild<QWidget *>("preview");
    preview_container_ = main_win_->findChild<QWidget *>("previewContainer");

    if (!native_preview_ || !preview_container_) {
        blog(LOG_WARNING, "[JNCanvas] Could not locate native preview or previewContainer.");
        return false;
    }

    QBoxLayout *layout = qobject_cast<QBoxLayout *>(preview_container_->layout());
    if (!layout) {
        blog(LOG_WARNING, "[JNCanvas] previewContainer has no box layout.");
        return false;
    }

    int idx = layout->indexOf(native_preview_);
    if (idx < 0) {
        blog(LOG_WARNING, "[JNCanvas] native_preview_ not found in container layout.");
        return false;
    }

    // Create a QStackedWidget at the EXACT slot where native_preview_ was.
    // This guarantees the zoom toolbar and properties bar never move a single pixel!
    stack_widget_ = new QStackedWidget(preview_container_);
    stack_widget_->setObjectName("JNCanvasStackWidget");

    layout->removeWidget(native_preview_);
    layout->insertWidget(idx, stack_widget_, 1);

    // Page 0: Native OBS Preview (100% standard OBS, mouse selection, red handles)
    stack_widget_->addWidget(native_preview_);

    // Page 1: Unified Canvas (Canvas 2 / Dual View with interactive editing)
    unified_display_ = new UnifiedCanvasDisplay(stack_widget_);
    unified_display_->setObjectName("JNUnifiedCanvasDisplay");
    stack_widget_->addWidget(unified_display_);

    // Start with Native Preview (Canvas 1)
    set_mode(CanvasViewMode::MainOnly);

    blog(LOG_INFO, "[JNCanvas] Seamless QStackedWidget preview integration initialized.");
    return true;
}

void CanvasCentralView::set_mode(CanvasViewMode mode) {
    current_mode_ = mode;
    apply_mode(mode);
    emit mode_changed(mode);
}

void CanvasCentralView::apply_mode(CanvasViewMode mode) {
    if (!stack_widget_) return;

    switch (mode) {
    case CanvasViewMode::MainOnly:
        stack_widget_->setCurrentIndex(0);
        break;
    case CanvasViewMode::Canvas2Only:
        stack_widget_->setCurrentIndex(1);
        if (unified_display_) {
            unified_display_->set_mode(UnifiedViewMode::Canvas2Only);
        }
        break;
    case CanvasViewMode::DualView:
        stack_widget_->setCurrentIndex(1);
        if (unified_display_) {
            unified_display_->set_mode(UnifiedViewMode::DualView);
        }
        break;
    }
}

#include "canvas-preview-dock.hpp"
#include "../canvas-engine.hpp"
#include <QResizeEvent>
#include <QVBoxLayout>

CanvasDisplayWidget::CanvasDisplayWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(180, 320); // 9:16 aspect ratio base
}

CanvasDisplayWidget::~CanvasDisplayWidget() {
    destroy_display();
}

void CanvasDisplayWidget::create_display() {
    if (display_) return;

    gs_init_data info = {};
    info.cx = width();
    info.cy = height();
    info.format = GS_RGBA;
    info.zsformat = GS_ZS_NONE;
    info.window.id = (void *)(uintptr_t)winId();

    display_ = obs_display_create(&info, 0x00000000);
    if (display_) {
        obs_display_add_draw_callback(display_, render_display, this);
    }
}

void CanvasDisplayWidget::destroy_display() {
    if (display_) {
        obs_display_remove_draw_callback(display_, render_display, this);
        obs_display_destroy(display_);
        display_ = nullptr;
    }
}

void CanvasDisplayWidget::render_display(void *data, uint32_t cx, uint32_t cy) {
    Q_UNUSED(data);
    CanvasEngine::instance().render(cx, cy);
}

void CanvasDisplayWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (!display_ && isVisible()) {
        create_display();
    }
}

void CanvasDisplayWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (display_) {
        obs_display_resize(display_, width(), height());
    }
}

void CanvasDisplayWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    create_display();
}

void CanvasDisplayWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    // Destroy display on hide to conserve GPU power (Smart Throttling)
    destroy_display();
}

// ============================================================================
// CanvasPreviewDock
// ============================================================================

CanvasPreviewDock::CanvasPreviewDock(QWidget *parent)
    : QDockWidget(tr("Canvas 2 - Preview"), parent) {
    setObjectName("JNCanvasPreviewDock");

    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    display_widget_ = new CanvasDisplayWidget(container);
    layout->addWidget(display_widget_);

    setWidget(container);
}


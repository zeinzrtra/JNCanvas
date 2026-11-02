#include "unified-canvas-display.hpp"
#include "../canvas-engine.hpp"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

UnifiedCanvasDisplay::UnifiedCanvasDisplay(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

UnifiedCanvasDisplay::~UnifiedCanvasDisplay() {
    destroy_display();
}

void UnifiedCanvasDisplay::set_mode(UnifiedViewMode mode) {
    mode_ = mode;
    fit_to_screen();
    update();
}

void UnifiedCanvasDisplay::create_display() {
    if (display_) return;

    gs_init_data info = {};
    info.cx = width() > 0 ? width() : 1280;
    info.cy = height() > 0 ? height() : 720;
    info.format = GS_RGBA;
    info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
    info.window.hwnd = (HWND)(uintptr_t)winId();
#else
    info.window.id = (void *)(uintptr_t)winId();
#endif

    display_ = obs_display_create(&info, 0x00000000);
    if (display_) {
        obs_display_add_draw_callback(display_, draw_callback, this);
    }
}

void UnifiedCanvasDisplay::destroy_display() {
    if (display_) {
        obs_display_remove_draw_callback(display_, draw_callback, this);
        obs_display_destroy(display_);
        display_ = nullptr;
    }
}

void UnifiedCanvasDisplay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (!display_ && isVisible()) {
        create_display();
    }
}

void UnifiedCanvasDisplay::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (display_) {
        obs_display_resize(display_, width(), height());
    }
    fit_to_screen();
}

void UnifiedCanvasDisplay::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    create_display();
    fit_to_screen();
}

void UnifiedCanvasDisplay::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    destroy_display();
}

void UnifiedCanvasDisplay::fit_to_screen() {
    if (width() <= 0 || height() <= 0) return;

    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }

    uint32_t vert_w = CanvasEngine::instance().width();
    uint32_t vert_h = CanvasEngine::instance().height();

    if (mode_ == UnifiedViewMode::DualView) {
        float gap = 60.0f;
        float total_w = (float)main_w + gap + (float)vert_w;
        float total_h = (float)std::max(main_h, vert_h);

        float scale_x = (float)width() / total_w;
        float scale_y = (float)height() / total_h;
        zoom_ = std::min(scale_x, scale_y) * 0.90f;

        pan_x_ = ((float)width() - total_w * zoom_) * 0.5f;
        pan_y_ = ((float)height() - total_h * zoom_) * 0.5f;
    } else {
        float scale_x = (float)width() / (float)vert_w;
        float scale_y = (float)height() / (float)vert_h;
        zoom_ = std::min(scale_x, scale_y) * 0.90f;

        pan_x_ = ((float)width() - (float)vert_w * zoom_) * 0.5f;
        pan_y_ = ((float)height() - (float)vert_h * zoom_) * 0.5f;
    }
}

void UnifiedCanvasDisplay::wheelEvent(QWheelEvent *event) {
    float delta = event->angleDelta().y();
    if (delta == 0.0f) return;

    float factor = (delta > 0) ? 1.1f : 0.909f;
    float new_zoom = zoom_ * factor;
    if (new_zoom < 0.05f || new_zoom > 10.0f) return;

    QPointF mouse_pos = event->position();
    pan_x_ = mouse_pos.x() - (mouse_pos.x() - pan_x_) * factor;
    pan_y_ = mouse_pos.y() - (mouse_pos.y() - pan_y_) * factor;
    zoom_ = new_zoom;

    update();
}

void UnifiedCanvasDisplay::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier || event->modifiers() & Qt::ControlModifier))) {
        is_panning_ = true;
        last_mouse_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void UnifiedCanvasDisplay::mouseMoveEvent(QMouseEvent *event) {
    if (is_panning_) {
        pan_x_ += (float)(event->pos().x() - last_mouse_pos_.x());
        pan_y_ += (float)(event->pos().y() - last_mouse_pos_.y());
        last_mouse_pos_ = event->pos();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void UnifiedCanvasDisplay::mouseReleaseEvent(QMouseEvent *event) {
    if (is_panning_) {
        is_panning_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void UnifiedCanvasDisplay::mouseDoubleClickEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    fit_to_screen();
    update();
}

void UnifiedCanvasDisplay::draw_solid_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
    if (!solid) return;

    gs_eparam_t *param = gs_effect_get_param_by_name(solid, "color");
    struct vec4 color;
    vec4_set(&color, r, g, b, a);
    gs_effect_set_vec4(param, &color);

    gs_matrix_push();
    gs_matrix_translate3f(x, y, 0.0f);

    gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
    if (tech) {
        gs_technique_begin(tech);
        gs_technique_begin_pass(tech, 0);
        gs_draw_sprite(nullptr, 0, (uint32_t)w, (uint32_t)h);
        gs_technique_end_pass(tech);
        gs_technique_end(tech);
    }

    gs_matrix_pop();
}

void UnifiedCanvasDisplay::draw_canvas_frame(float x, float y, float w, float h, float r, float g, float b) {
    float border = 3.0f;
    // Outer border frame
    draw_solid_rect(x - border, y - border, w + border * 2.0f, h + border * 2.0f, r, g, b, 1.0f);
    // Inner black canvas backing (ensures non-transparent solid canvas)
    draw_solid_rect(x, y, w, h, 0.0f, 0.0f, 0.0f, 1.0f);
}

void UnifiedCanvasDisplay::draw_callback(void *param, uint32_t cx, uint32_t cy) {
    auto *self = static_cast<UnifiedCanvasDisplay *>(param);
    if (self) {
        self->render(cx, cy);
    }
}

void UnifiedCanvasDisplay::render(uint32_t cx, uint32_t cy) {
    if (cx == 0 || cy == 0) return;

    // 1. Draw workspace background (OBS dark workspace tone #141619)
    draw_solid_rect(0.0f, 0.0f, (float)cx, (float)cy, 0.078f, 0.086f, 0.098f, 1.0f);

    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }

    uint32_t vert_w = CanvasEngine::instance().width();
    uint32_t vert_h = CanvasEngine::instance().height();

    // 2. Apply viewport transformation (Zoom & Pan applied together to all canvases)
    gs_matrix_push();
    gs_matrix_translate3f(pan_x_, pan_y_, 0.0f);
    gs_matrix_scale3f(zoom_, zoom_, 1.0f);

    if (mode_ == UnifiedViewMode::DualView) {
        float gap = 60.0f;
        float max_h = (float)std::max(main_h, vert_h);
        float main_x = 0.0f;
        float main_y = (max_h - (float)main_h) * 0.5f;
        float vert_x = (float)main_w + gap;
        float vert_y = (max_h - (float)vert_h) * 0.5f;

        // Render Canvas 1 (Main 16:9)
        draw_canvas_frame(main_x, main_y, (float)main_w, (float)main_h, 0.35f, 0.38f, 0.45f);
        gs_matrix_push();
        gs_matrix_translate3f(main_x, main_y, 0.0f);
        obs_render_main_texture();
        gs_matrix_pop();

        // Render Canvas 2 (Vertical 9:16)
        draw_canvas_frame(vert_x, vert_y, (float)vert_w, (float)vert_h, 0.30f, 0.58f, 1.0f);
        obs_view_t *view = CanvasEngine::instance().view();
        if (view) {
            gs_matrix_push();
            gs_matrix_translate3f(vert_x, vert_y, 0.0f);
            obs_view_render(view);
            gs_matrix_pop();
        }
    } else {
        // Mode == Canvas2Only: Centered 9:16 vertical canvas
        draw_canvas_frame(0.0f, 0.0f, (float)vert_w, (float)vert_h, 0.30f, 0.58f, 1.0f);
        obs_view_t *view = CanvasEngine::instance().view();
        if (view) {
            obs_view_render(view);
        }
    }

    gs_matrix_pop();
}

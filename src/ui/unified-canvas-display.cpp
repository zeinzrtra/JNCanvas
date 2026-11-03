#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "unified-canvas-display.hpp"
#include "../canvas-engine.hpp"
#include <obs-frontend-api.h>
#include <QMenu>
#include <QAction>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

static inline QSize get_pixel_size(QWidget *widget) {
    const qreal dpr = widget->devicePixelRatioF();
    return QSize((std::max)(1, qRound(widget->width() * dpr)),
                 (std::max)(1, qRound(widget->height() * dpr)));
}

UnifiedCanvasDisplay::UnifiedCanvasDisplay(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NativeWindow);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(32, 32);
}

UnifiedCanvasDisplay::~UnifiedCanvasDisplay() {
    destroy_display();
}

void UnifiedCanvasDisplay::set_mode(UnifiedViewMode mode) {
    mode_ = mode;
    is_custom_zoom_ = false;
    update();
}

void UnifiedCanvasDisplay::reset_zoom() {
    is_custom_zoom_ = false;
    update();
}

void UnifiedCanvasDisplay::fit_to_screen() {
    is_custom_zoom_ = false;
    update();
}

void UnifiedCanvasDisplay::select_item(obs_sceneitem_t *item) {
    selected_item_ = item;
    update();
}

void UnifiedCanvasDisplay::create_display() {
    if (display_) return;

    QSize size = get_pixel_size(this);
    gs_init_data info = {};
    info.cx = (uint32_t)size.width();
    info.cy = (uint32_t)size.height();
    info.format = GS_RGBA;
    info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
    info.window.hwnd = (HWND)(uintptr_t)winId();
#else
    info.window.id = (void *)(uintptr_t)winId();
#endif

    display_ = obs_display_create(&info, 0xFF141619);
    if (display_) {
        obs_display_add_draw_callback(display_, draw_callback, this);
    }

    if (!vert_texrender_) {
        obs_enter_graphics();
        vert_texrender_ = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
        obs_leave_graphics();
    }
}

void UnifiedCanvasDisplay::destroy_display() {
    if (display_) {
        obs_display_remove_draw_callback(display_, draw_callback, this);
        obs_display_destroy(display_);
        display_ = nullptr;
    }
    if (vert_texrender_) {
        obs_enter_graphics();
        gs_texrender_destroy(vert_texrender_);
        obs_leave_graphics();
        vert_texrender_ = nullptr;
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
        QSize size = get_pixel_size(this);
        obs_display_resize(display_, size.width(), size.height());
    }
    update();
}

void UnifiedCanvasDisplay::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    create_display();
    is_custom_zoom_ = false;
    update();
}

void UnifiedCanvasDisplay::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    destroy_display();
}

void UnifiedCanvasDisplay::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    if (hovered_item_) {
        hovered_item_ = nullptr;
        update();
    }
}

void UnifiedCanvasDisplay::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        is_space_pressed_ = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void UnifiedCanvasDisplay::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        is_space_pressed_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

bool UnifiedCanvasDisplay::map_screen_to_viewport(const QPointF &screen_pos, float &vx, float &vy) const {
    if (zoom_ <= 0.0f) return false;
    const qreal dpr = devicePixelRatioF();
    float px = (float)(screen_pos.x() * dpr);
    float py = (float)(screen_pos.y() * dpr);
    vx = (px - pan_x_) / zoom_;
    vy = (py - pan_y_) / zoom_;
    return true;
}

bool UnifiedCanvasDisplay::map_viewport_to_canvas2(float vx, float vy, float &sx, float &sy) const {
    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }

    uint32_t vert_w = CanvasEngine::instance().width();
    uint32_t vert_h = CanvasEngine::instance().height();
    if (vert_w == 0) vert_w = 1080;
    if (vert_h == 0) vert_h = 1920;

    float base_h = (float)main_h;
    float aspect_vert = (float)vert_w / (float)vert_h;
    float draw_vert_w = base_h * aspect_vert;
    float draw_vert_h = base_h;

    float gap = 40.0f;
    float c2_x = (mode_ == UnifiedViewMode::DualView) ? ((float)main_w + gap) : 0.0f;
    float c2_y = 0.0f;

    if (vx >= c2_x && vx <= c2_x + draw_vert_w && vy >= c2_y && vy <= c2_y + draw_vert_h) {
        sx = (vx - c2_x) * ((float)vert_w / draw_vert_w);
        sy = (vy - c2_y) * ((float)vert_h / draw_vert_h);
        return true;
    }
    return false;
}

bool UnifiedCanvasDisplay::get_item_visual_rect(obs_sceneitem_t *item, float &rx, float &ry, float &rw, float &rh) const {
    if (!item) return false;

    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }

    uint32_t vert_w = CanvasEngine::instance().width();
    uint32_t vert_h = CanvasEngine::instance().height();
    if (vert_w == 0) vert_w = 1080;
    if (vert_h == 0) vert_h = 1920;

    float base_h = (float)main_h;
    float aspect_vert = (float)vert_w / (float)vert_h;
    float draw_vert_w = base_h * aspect_vert;
    float draw_vert_h = base_h;
    float scale_c2 = draw_vert_h / (float)vert_h;

    float gap = 40.0f;
    float c2_x = (mode_ == UnifiedViewMode::DualView) ? ((float)main_w + gap) : 0.0f;
    float c2_y = 0.0f;

    struct vec2 pos, scale;
    obs_sceneitem_get_pos(item, &pos);
    obs_sceneitem_get_scale(item, &scale);
    obs_source_t *src = obs_sceneitem_get_source(item);
    if (!src) return false;

    float sw = (float)obs_source_get_width(src);
    float sh = (float)obs_source_get_height(src);
    if (sw <= 0.0f) sw = (float)vert_w;
    if (sh <= 0.0f) sh = (float)vert_h;

    float item_w = sw * scale.x;
    float item_h = sh * scale.y;

    rx = c2_x + pos.x * scale_c2;
    ry = c2_y + pos.y * scale_c2;
    rw = item_w * scale_c2;
    rh = item_h * scale_c2;
    return true;
}

ResizeHandle UnifiedCanvasDisplay::get_handle_at(float vx, float vy, float rx, float ry, float rw, float rh) const {
    float threshold = 14.0f;
    float mid_x = rx + rw * 0.5f;
    float mid_y = ry + rh * 0.5f;
    float rot_y = ry - 25.0f;

    // Rotation handle (top center above box)
    if (std::hypot(vx - mid_x, vy - rot_y) <= threshold) return ResizeHandle::Rot;

    // 4 Corners
    if (std::abs(vx - rx) <= threshold && std::abs(vy - ry) <= threshold) return ResizeHandle::TopLeft;
    if (std::abs(vx - (rx + rw)) <= threshold && std::abs(vy - ry) <= threshold) return ResizeHandle::TopRight;
    if (std::abs(vx - rx) <= threshold && std::abs(vy - (ry + rh)) <= threshold) return ResizeHandle::BottomLeft;
    if (std::abs(vx - (rx + rw)) <= threshold && std::abs(vy - (ry + rh)) <= threshold) return ResizeHandle::BottomRight;

    // 4 Edge Centers
    if (std::abs(vx - mid_x) <= threshold && std::abs(vy - ry) <= threshold) return ResizeHandle::TopCenter;
    if (std::abs(vx - mid_x) <= threshold && std::abs(vy - (ry + rh)) <= threshold) return ResizeHandle::BottomCenter;
    if (std::abs(vx - rx) <= threshold && std::abs(vy - mid_y) <= threshold) return ResizeHandle::CenterLeft;
    if (std::abs(vx - (rx + rw)) <= threshold && std::abs(vy - mid_y) <= threshold) return ResizeHandle::CenterRight;

    return ResizeHandle::None;
}

void UnifiedCanvasDisplay::wheelEvent(QWheelEvent *event) {
    float delta = event->angleDelta().y();
    if (delta == 0.0f) return;

    is_custom_zoom_ = true;

    float factor = (delta > 0) ? 1.15f : 0.87f;
    float new_zoom = zoom_ * factor;
    if (new_zoom < 0.05f || new_zoom > 20.0f) return;

    const qreal dpr = devicePixelRatioF();
    QPointF mouse_pos = event->position() * dpr;
    pan_x_ = (float)(mouse_pos.x() - (mouse_pos.x() - pan_x_) * factor);
    pan_y_ = (float)(mouse_pos.y() - (mouse_pos.y() - pan_y_) * factor);
    zoom_ = new_zoom;

    update();
}

void UnifiedCanvasDisplay::mousePressEvent(QMouseEvent *event) {
    // 1. Pan requested: Middle button, Space+Left click, Alt/Ctrl+Left click
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && is_space_pressed_) ||
        (event->button() == Qt::LeftButton && (event->modifiers() & (Qt::AltModifier | Qt::ControlModifier)))) {
        drag_mode_ = DragMode::PanCanvas;
        last_mouse_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    float vx = 0.0f, vy = 0.0f;
    if (!map_screen_to_viewport(event->position(), vx, vy)) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // 2. Check resize handles on selected item
        if (selected_item_) {
            float rx, ry, rw, rh;
            if (get_item_visual_rect(selected_item_, rx, ry, rw, rh)) {
                ResizeHandle handle = get_handle_at(vx, vy, rx, ry, rw, rh);
                if (handle != ResizeHandle::None) {
                    if (handle == ResizeHandle::Rot) {
                        drag_mode_ = DragMode::RotateItem;
                        initial_item_rot_ = obs_sceneitem_get_rot(selected_item_);
                    } else {
                        drag_mode_ = DragMode::ResizeItem;
                    }
                    active_handle_ = handle;
                    obs_sceneitem_get_pos(selected_item_, &initial_item_pos_);
                    obs_sceneitem_get_scale(selected_item_, &initial_item_scale_);
                    float sx = 0.0f, sy = 0.0f;
                    map_viewport_to_canvas2(vx, vy, sx, sy);
                    drag_start_scene_pos_ = QPointF(sx, sy);
                    event->accept();
                    return;
                }
            }
        }

        // 3. Hit-test items in Canvas 2
        float sx = 0.0f, sy = 0.0f;
        if (map_viewport_to_canvas2(vx, vy, sx, sy)) {
            obs_scene_t *scene = CanvasEngine::instance().current_scene();
            obs_sceneitem_t *hit_item = nullptr;

            if (scene) {
                struct HitContext {
                    float sx, sy;
                    obs_sceneitem_t *hit;
                } ctx = {sx, sy, nullptr};

                obs_scene_enum_items(
                    scene,
                    [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
                        auto *c = static_cast<HitContext *>(param);
                        if (!obs_sceneitem_visible(item) || obs_sceneitem_locked(item)) return true;

                        struct vec2 pos, scale;
                        obs_sceneitem_get_pos(item, &pos);
                        obs_sceneitem_get_scale(item, &scale);
                        obs_source_t *src = obs_sceneitem_get_source(item);
                        if (!src) return true;

                        float sw = (float)obs_source_get_width(src);
                        float sh = (float)obs_source_get_height(src);
                        if (sw <= 0.0f) sw = (float)CanvasEngine::instance().width();
                        if (sh <= 0.0f) sh = (float)CanvasEngine::instance().height();

                        float w = sw * scale.x;
                        float h = sh * scale.y;

                        if (c->sx >= pos.x && c->sx <= pos.x + w && c->sy >= pos.y && c->sy <= pos.y + h) {
                            c->hit = item;
                        }
                        return true;
                    },
                    &ctx);

                hit_item = ctx.hit;
            }

            if (hit_item) {
                selected_item_ = hit_item;
                drag_mode_ = DragMode::MoveItem;
                obs_sceneitem_get_pos(selected_item_, &initial_item_pos_);
                obs_sceneitem_get_scale(selected_item_, &initial_item_scale_);
                drag_start_scene_pos_ = QPointF(sx, sy);
                setCursor(Qt::SizeAllCursor);
                emit item_selected(selected_item_);
                update();
                event->accept();
                return;
            } else {
                if (selected_item_) {
                    selected_item_ = nullptr;
                    emit item_selected(nullptr);
                    update();
                }
            }
        }

        // 4. Clicked on empty canvas background -> Pan
        drag_mode_ = DragMode::PanCanvas;
        last_mouse_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        show_context_menu(event->globalPosition().toPoint(), vx, vy);
        event->accept();
        return;
    }
}

void UnifiedCanvasDisplay::mouseMoveEvent(QMouseEvent *event) {
    float vx = 0.0f, vy = 0.0f;
    map_screen_to_viewport(event->position(), vx, vy);

    if (drag_mode_ == DragMode::MoveItem && selected_item_) {
        float sx = 0.0f, sy = 0.0f;
        if (map_viewport_to_canvas2(vx, vy, sx, sy)) {
            float dx = sx - (float)drag_start_scene_pos_.x();
            float dy = sy - (float)drag_start_scene_pos_.y();

            float target_x = initial_item_pos_.x + dx;
            float target_y = initial_item_pos_.y + dy;

            obs_source_t *src = obs_sceneitem_get_source(selected_item_);
            float sw = src ? (float)obs_source_get_width(src) : 1080.0f;
            float sh = src ? (float)obs_source_get_height(src) : 1920.0f;
            if (sw <= 0.0f) sw = 1080.0f;
            if (sh <= 0.0f) sh = 1920.0f;

            float item_w = sw * initial_item_scale_.x;
            float item_h = sh * initial_item_scale_.y;
            float cw = (float)CanvasEngine::instance().width();
            float ch = (float)CanvasEngine::instance().height();
            if (cw <= 0.0f) cw = 1080.0f;
            if (ch <= 0.0f) ch = 1920.0f;

            // Snap to Canvas 2 edges
            float snap_thr = 14.0f;
            snap_x_ = false;
            snap_y_ = false;

            // X-Axis Snapping
            if (std::abs(target_x - 0.0f) < snap_thr) {
                target_x = 0.0f;
                snap_x_ = true;
                snap_x_pos_ = 0.0f;
            } else if (std::abs((target_x + item_w * 0.5f) - cw * 0.5f) < snap_thr) {
                target_x = cw * 0.5f - item_w * 0.5f;
                snap_x_ = true;
                snap_x_pos_ = cw * 0.5f;
            } else if (std::abs((target_x + item_w) - cw) < snap_thr) {
                target_x = cw - item_w;
                snap_x_ = true;
                snap_x_pos_ = cw;
            }

            // Y-Axis Snapping
            if (std::abs(target_y - 0.0f) < snap_thr) {
                target_y = 0.0f;
                snap_y_ = true;
                snap_y_pos_ = 0.0f;
            } else if (std::abs((target_y + item_h * 0.5f) - ch * 0.5f) < snap_thr) {
                target_y = ch * 0.5f - item_h * 0.5f;
                snap_y_ = true;
                snap_y_pos_ = ch * 0.5f;
            } else if (std::abs((target_y + item_h) - ch) < snap_thr) {
                target_y = ch - item_h;
                snap_y_ = true;
                snap_y_pos_ = ch;
            }

            struct vec2 new_pos;
            new_pos.x = target_x;
            new_pos.y = target_y;

            obs_sceneitem_set_pos(selected_item_, &new_pos);
            update();
            event->accept();
            return;
        }
    }

    if (drag_mode_ == DragMode::ResizeItem && selected_item_) {
        float sx = 0.0f, sy = 0.0f;
        map_viewport_to_canvas2(vx, vy, sx, sy);
        float dx = sx - (float)drag_start_scene_pos_.x();
        float dy = sy - (float)drag_start_scene_pos_.y();

        obs_source_t *src = obs_sceneitem_get_source(selected_item_);
        float sw = src ? (float)obs_source_get_width(src) : 1080.0f;
        float sh = src ? (float)obs_source_get_height(src) : 1920.0f;
        if (sw <= 0.0f) sw = 1080.0f;
        if (sh <= 0.0f) sh = 1920.0f;

        struct vec2 new_scale = initial_item_scale_;
        struct vec2 new_pos = initial_item_pos_;

        switch (active_handle_) {
        case ResizeHandle::BottomRight: {
            float new_w = initial_item_scale_.x * sw + dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            break;
        }
        case ResizeHandle::BottomLeft: {
            float new_w = initial_item_scale_.x * sw - dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            new_pos.x = initial_item_pos_.x + (initial_item_scale_.x - new_scale.x) * sw;
            break;
        }
        case ResizeHandle::TopRight: {
            float new_w = initial_item_scale_.x * sw + dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            new_pos.y = initial_item_pos_.y + (initial_item_scale_.y - new_scale.y) * sh;
            break;
        }
        case ResizeHandle::TopLeft: {
            float new_w = initial_item_scale_.x * sw - dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            new_pos.x = initial_item_pos_.x + (initial_item_scale_.x - new_scale.x) * sw;
            new_pos.y = initial_item_pos_.y + (initial_item_scale_.y - new_scale.y) * sh;
            break;
        }
        case ResizeHandle::CenterRight: {
            float new_w = initial_item_scale_.x * sw + dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            break;
        }
        case ResizeHandle::CenterLeft: {
            float new_w = initial_item_scale_.x * sw - dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_pos.x = initial_item_pos_.x + (initial_item_scale_.x - new_scale.x) * sw;
            break;
        }
        case ResizeHandle::BottomCenter: {
            float new_h = initial_item_scale_.y * sh + dy;
            new_scale.y = (std::max)(0.01f, new_h / sh);
            break;
        }
        case ResizeHandle::TopCenter: {
            float new_h = initial_item_scale_.y * sh - dy;
            new_scale.y = (std::max)(0.01f, new_h / sh);
            new_pos.y = initial_item_pos_.y + (initial_item_scale_.y - new_scale.y) * sh;
            break;
        }
        default:
            break;
        }

        obs_sceneitem_set_pos(selected_item_, &new_pos);
        obs_sceneitem_set_scale(selected_item_, &new_scale);
        update();
        event->accept();
        return;
    }

    if (drag_mode_ == DragMode::RotateItem && selected_item_) {
        float sx = 0.0f, sy = 0.0f;
        map_viewport_to_canvas2(vx, vy, sx, sy);
        float rx, ry, rw, rh;
        if (get_item_visual_rect(selected_item_, rx, ry, rw, rh)) {
            float mid_x = rx + rw * 0.5f;
            float mid_y = ry + rh * 0.5f;
            float angle = std::atan2(vy - mid_y, vx - mid_x) * 180.0f / 3.14159265f + 90.0f;
            obs_sceneitem_set_rot(selected_item_, angle);
            update();
            event->accept();
            return;
        }
    }

    if (drag_mode_ == DragMode::PanCanvas) {
        is_custom_zoom_ = true;
        const qreal dpr = devicePixelRatioF();
        pan_x_ += (float)((event->pos().x() - last_mouse_pos_.x()) * dpr);
        pan_y_ += (float)((event->pos().y() - last_mouse_pos_.y()) * dpr);
        last_mouse_pos_ = event->pos();
        update();
        event->accept();
        return;
    }

    // Hover cursor & boundary feedback
    if (is_space_pressed_) {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    // Hit-test for hover
    float sx = 0.0f, sy = 0.0f;
    obs_sceneitem_t *hit_item = nullptr;
    if (map_viewport_to_canvas2(vx, vy, sx, sy)) {
        obs_scene_t *scene = CanvasEngine::instance().current_scene();
        if (scene) {
            struct HitContext {
                float sx, sy;
                obs_sceneitem_t *hit;
            } ctx = {sx, sy, nullptr};

            obs_scene_enum_items(
                scene,
                [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
                    auto *c = static_cast<HitContext *>(param);
                    if (!obs_sceneitem_visible(item) || obs_sceneitem_locked(item)) return true;

                    struct vec2 pos, scale;
                    obs_sceneitem_get_pos(item, &pos);
                    obs_sceneitem_get_scale(item, &scale);
                    obs_source_t *src = obs_sceneitem_get_source(item);
                    if (!src) return true;

                    float sw = (float)obs_source_get_width(src);
                    float sh = (float)obs_source_get_height(src);
                    if (sw <= 0.0f) sw = (float)CanvasEngine::instance().width();
                    if (sh <= 0.0f) sh = (float)CanvasEngine::instance().height();

                    float w = sw * scale.x;
                    float h = sh * scale.y;

                    if (c->sx >= pos.x && c->sx <= pos.x + w && c->sy >= pos.y && c->sy <= pos.y + h) {
                        c->hit = item;
                    }
                    return true;
                },
                &ctx);

            hit_item = ctx.hit;
        }
    }

    if (hovered_item_ != hit_item) {
        hovered_item_ = hit_item;
        update();
    }

    if (selected_item_) {
        float rx, ry, rw, rh;
        if (get_item_visual_rect(selected_item_, rx, ry, rw, rh)) {
            ResizeHandle handle = get_handle_at(vx, vy, rx, ry, rw, rh);
            switch (handle) {
            case ResizeHandle::TopLeft:
            case ResizeHandle::BottomRight:
                setCursor(Qt::SizeFDiagCursor);
                return;
            case ResizeHandle::TopRight:
            case ResizeHandle::BottomLeft:
                setCursor(Qt::SizeBDiagCursor);
                return;
            case ResizeHandle::TopCenter:
            case ResizeHandle::BottomCenter:
                setCursor(Qt::SizeVerCursor);
                return;
            case ResizeHandle::CenterLeft:
            case ResizeHandle::CenterRight:
                setCursor(Qt::SizeHorCursor);
                return;
            case ResizeHandle::Rot:
                setCursor(Qt::CrossCursor);
                return;
            case ResizeHandle::None:
                if (vx >= rx && vx <= rx + rw && vy >= ry && vy <= ry + rh) {
                    setCursor(Qt::SizeAllCursor);
                    return;
                }
                break;
            }
        }
    }

    unsetCursor();
    QWidget::mouseMoveEvent(event);
}

void UnifiedCanvasDisplay::mouseReleaseEvent(QMouseEvent *event) {
    drag_mode_ = DragMode::None;
    active_handle_ = ResizeHandle::None;
    snap_x_ = false;
    snap_y_ = false;
    if (is_space_pressed_) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
    update();
    QWidget::mouseReleaseEvent(event);
}

void UnifiedCanvasDisplay::mouseDoubleClickEvent(QMouseEvent *event) {
    float vx = 0.0f, vy = 0.0f;
    if (map_screen_to_viewport(event->position(), vx, vy)) {
        float sx = 0.0f, sy = 0.0f;
        if (map_viewport_to_canvas2(vx, vy, sx, sy) && selected_item_) {
            obs_source_t *src = obs_sceneitem_get_source(selected_item_);
            if (src) {
                obs_frontend_open_source_properties(src);
                event->accept();
                return;
            }
        }
    }
    fit_to_screen();
    event->accept();
}

void UnifiedCanvasDisplay::show_context_menu(const QPoint &global_pos, float, float) {
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #21252b;
            color: #abb2bf;
            border: 1px solid #3e4451;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 20px 6px 24px;
        }
        QMenu::item:selected {
            background-color: #2c313a;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: #3e4451;
            margin: 4px 8px;
        }
    )");

    // 1. Add Source submenu
    QMenu *add_menu = menu.addMenu(tr("➕ Tambah Sumber"));
    auto add_src_act = [&](const QString &label, const char *id) {
        QAction *act = add_menu->addAction(label);
        connect(act, &QAction::triggered, [this, id, label]() {
            add_source_by_id(id, label.toUtf8().constData());
        });
    };
    add_src_act(tr("Tangkapan Layar (Display Capture)"), "monitor_capture");
    add_src_act(tr("Tangkapan Jendela (Window Capture)"), "window_capture");
    add_src_act(tr("Perangkat Perekam Video (Kamera)"), "dshow_input");
    add_src_act(tr("Tangkapan Game (Game Capture)"), "game_capture");
    add_src_act(tr("Peramban (Browser)"), "browser_source");
    add_src_act(tr("Gambar (Image)"), "image_source");
    add_src_act(tr("Sumber Media (Media Source)"), "ffmpeg_source");
    add_src_act(tr("Sumber Warna (Color Source)"), "color_source");
    add_src_act(tr("Teks (GDI+)"), "text_gdiplus");

    // 2. Clone from Main OBS Scene
    QAction *clone_act = menu.addAction(tr("📋 Clone dari Canvas 1 (Main OBS Scene)"));
    connect(clone_act, &QAction::triggered, this, &UnifiedCanvasDisplay::clone_from_main);

    menu.addSeparator();

    if (selected_item_) {
        obs_source_t *src = obs_sceneitem_get_source(selected_item_);
        const char *name = src ? obs_source_get_name(src) : "Source";

        // Transform submenu
        QMenu *transform_menu = menu.addMenu(tr("📐 Transform"));
        QAction *act_fit = transform_menu->addAction(tr("Paskan ke Layar (Fit to Screen)"));
        connect(act_fit, &QAction::triggered, this, &UnifiedCanvasDisplay::transform_fit);

        QAction *act_stretch = transform_menu->addAction(tr("Regangkan ke Layar (Stretch to Screen)"));
        connect(act_stretch, &QAction::triggered, this, &UnifiedCanvasDisplay::transform_stretch);

        QAction *act_center = transform_menu->addAction(tr("Pusatkan ke Layar (Center to Screen)"));
        connect(act_center, &QAction::triggered, this, &UnifiedCanvasDisplay::transform_center);

        QAction *act_reset = transform_menu->addAction(tr("Reset Transform"));
        connect(act_reset, &QAction::triggered, this, &UnifiedCanvasDisplay::transform_reset);

        // Order submenu
        QMenu *order_menu = menu.addMenu(tr("📑 Urutan (Order)"));
        QAction *act_up = order_menu->addAction(tr("Pindah ke Atas"));
        connect(act_up, &QAction::triggered, this, &UnifiedCanvasDisplay::order_move_up);

        QAction *act_down = order_menu->addAction(tr("Pindah ke Bawah"));
        connect(act_down, &QAction::triggered, this, &UnifiedCanvasDisplay::order_move_down);

        menu.addSeparator();

        if (src) {
            QAction *act_props = menu.addAction(tr("⚙️ Properties"));
            connect(act_props, &QAction::triggered, [src]() {
                obs_frontend_open_source_properties(src);
            });

            QAction *act_filters = menu.addAction(tr("🎨 Filters"));
            connect(act_filters, &QAction::triggered, [src]() {
                obs_frontend_open_source_filters(src);
            });

            menu.addSeparator();
        }

        QAction *act_del = menu.addAction(tr("🗑️ Hapus Sumber (%1)").arg(QString::fromUtf8(name)));
        connect(act_del, &QAction::triggered, this, &UnifiedCanvasDisplay::remove_selected_item);
    }

    menu.exec(global_pos);
}

void UnifiedCanvasDisplay::add_source_by_id(const char *id, const char *name) {
    obs_scene_t *scene = CanvasEngine::instance().current_scene();
    if (!scene) return;

    obs_source_t *src = obs_source_create(id, name, nullptr, nullptr);
    if (!src) return;

    obs_sceneitem_t *item = obs_scene_add(scene, src);
    if (item) {
        uint32_t sw = obs_source_get_width(src);
        uint32_t cw = CanvasEngine::instance().width();
        if (sw > 0 && cw > 0 && sw > cw) {
            float scale = (float)cw / (float)sw;
            struct vec2 s = {scale, scale};
            obs_sceneitem_set_scale(item, &s);
        }
        selected_item_ = item;
        emit item_selected(selected_item_);
        emit sources_changed();
    }
    obs_source_release(src);
    update();
}

void UnifiedCanvasDisplay::clone_from_main() {
    obs_scene_t *dest_scene = CanvasEngine::instance().current_scene();
    obs_source_t *main_scene_src = obs_frontend_get_current_scene();
    if (!dest_scene || !main_scene_src) return;

    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }
    uint32_t cw = CanvasEngine::instance().width();
    uint32_t ch = CanvasEngine::instance().height();
    if (cw == 0) cw = 1080;
    if (ch == 0) ch = 1920;

    float ratio = (main_w > 0) ? ((float)cw / (float)main_w) : 1.0f;
    float scaled_h = (float)main_h * ratio;
    float offset_y = ((float)ch - scaled_h) * 0.5f;

    struct CloneContext {
        obs_scene_t *dest;
        float ratio;
        float offset_y;
        obs_sceneitem_t *first_item;
    } ctx = {dest_scene, ratio, offset_y, nullptr};

    obs_scene_t *main_scene = obs_scene_from_source(main_scene_src);
    if (main_scene) {
        obs_scene_enum_items(
            main_scene,
            [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
                auto *c = static_cast<CloneContext *>(param);
                obs_source_t *src = obs_sceneitem_get_source(item);
                if (src) {
                    obs_sceneitem_t *new_item = obs_scene_add(c->dest, src);
                    if (new_item) {
                        struct vec2 pos, scale;
                        obs_sceneitem_get_pos(item, &pos);
                        obs_sceneitem_get_scale(item, &scale);

                        struct vec2 new_pos = {pos.x * c->ratio, c->offset_y + pos.y * c->ratio};
                        struct vec2 new_scale = {scale.x * c->ratio, scale.y * c->ratio};

                        obs_sceneitem_set_pos(new_item, &new_pos);
                        obs_sceneitem_set_scale(new_item, &new_scale);
                        if (!c->first_item) c->first_item = new_item;
                    }
                }
                return true;
            },
            &ctx);

        if (ctx.first_item) {
            selected_item_ = ctx.first_item;
            emit item_selected(selected_item_);
        }
        emit sources_changed();
    }
    obs_source_release(main_scene_src);
    update();
}

void UnifiedCanvasDisplay::transform_fit() {
    if (!selected_item_) return;
    obs_source_t *src = obs_sceneitem_get_source(selected_item_);
    if (!src) return;

    uint32_t sw = obs_source_get_width(src);
    uint32_t sh = obs_source_get_height(src);
    uint32_t cw = CanvasEngine::instance().width();
    uint32_t ch = CanvasEngine::instance().height();
    if (sw == 0 || sh == 0) return;

    float scale = (std::min)((float)cw / (float)sw, (float)ch / (float)sh);
    struct vec2 s = {scale, scale};
    struct vec2 pos = {((float)cw - (float)sw * scale) * 0.5f,
                       ((float)ch - (float)sh * scale) * 0.5f};
    obs_sceneitem_set_pos(selected_item_, &pos);
    obs_sceneitem_set_scale(selected_item_, &s);
    update();
}

void UnifiedCanvasDisplay::transform_stretch() {
    if (!selected_item_) return;
    obs_source_t *src = obs_sceneitem_get_source(selected_item_);
    if (!src) return;

    uint32_t sw = obs_source_get_width(src);
    uint32_t sh = obs_source_get_height(src);
    uint32_t cw = CanvasEngine::instance().width();
    uint32_t ch = CanvasEngine::instance().height();
    if (sw == 0 || sh == 0) return;

    struct vec2 s = {(float)cw / (float)sw, (float)ch / (float)sh};
    struct vec2 pos = {0.0f, 0.0f};
    obs_sceneitem_set_pos(selected_item_, &pos);
    obs_sceneitem_set_scale(selected_item_, &s);
    update();
}

void UnifiedCanvasDisplay::transform_center() {
    if (!selected_item_) return;
    obs_source_t *src = obs_sceneitem_get_source(selected_item_);
    if (!src) return;

    uint32_t sw = obs_source_get_width(src);
    uint32_t sh = obs_source_get_height(src);
    uint32_t cw = CanvasEngine::instance().width();
    uint32_t ch = CanvasEngine::instance().height();

    struct vec2 scale;
    obs_sceneitem_get_scale(selected_item_, &scale);
    struct vec2 pos = {((float)cw - (float)sw * scale.x) * 0.5f,
                       ((float)ch - (float)sh * scale.y) * 0.5f};
    obs_sceneitem_set_pos(selected_item_, &pos);
    update();
}

void UnifiedCanvasDisplay::transform_reset() {
    if (!selected_item_) return;
    struct vec2 s = {1.0f, 1.0f};
    struct vec2 pos = {0.0f, 0.0f};
    obs_sceneitem_set_pos(selected_item_, &pos);
    obs_sceneitem_set_scale(selected_item_, &s);
    obs_sceneitem_set_rot(selected_item_, 0.0f);
    update();
}

void UnifiedCanvasDisplay::order_move_up() {
    if (!selected_item_) return;
    int order = (int)obs_sceneitem_get_order_position(selected_item_);
    obs_sceneitem_set_order_position(selected_item_, order + 1);
    emit sources_changed();
    update();
}

void UnifiedCanvasDisplay::order_move_down() {
    if (!selected_item_) return;
    int order = (int)obs_sceneitem_get_order_position(selected_item_);
    if (order > 0) {
        obs_sceneitem_set_order_position(selected_item_, order - 1);
        emit sources_changed();
        update();
    }
}

void UnifiedCanvasDisplay::remove_selected_item() {
    if (!selected_item_) return;
    obs_sceneitem_remove(selected_item_);
    selected_item_ = nullptr;
    emit item_selected(nullptr);
    emit sources_changed();
    update();
}

void UnifiedCanvasDisplay::draw_solid_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    if (w <= 0.0f || h <= 0.0f) return;

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
    if (w <= 0.0f || h <= 0.0f) return;
    float border = 3.0f;
    draw_solid_rect(x - border, y - border, w + border * 2.0f, h + border * 2.0f, r, g, b, 1.0f);
    draw_solid_rect(x, y, w, h, 0.0f, 0.0f, 0.0f, 1.0f);
}

void UnifiedCanvasDisplay::draw_hover_box(obs_sceneitem_t *item) {
    if (!item) return;

    float rx, ry, rw, rh;
    if (!get_item_visual_rect(item, rx, ry, rw, rh)) return;

    float thickness = 2.0f;
    // Cyan hover outline (#00b4d8)
    draw_solid_rect(rx, ry, rw, thickness, 0.0f, 0.705f, 0.847f, 1.0f);
    draw_solid_rect(rx, ry + rh - thickness, rw, thickness, 0.0f, 0.705f, 0.847f, 1.0f);
    draw_solid_rect(rx, ry, thickness, rh, 0.0f, 0.705f, 0.847f, 1.0f);
    draw_solid_rect(rx + rw - thickness, ry, thickness, rh, 0.0f, 0.705f, 0.847f, 1.0f);
}

void UnifiedCanvasDisplay::draw_snap_lines(float c2_x, float c2_y, float c2_w, float c2_h, float scale_c2) {
    if (snap_x_) {
        float vx = c2_x + snap_x_pos_ * scale_c2;
        draw_solid_rect(vx - 1.0f, c2_y, 2.0f, c2_h, 1.0f, 0.23f, 0.19f, 0.85f);
    }
    if (snap_y_) {
        float vy = c2_y + snap_y_pos_ * scale_c2;
        draw_solid_rect(c2_x, vy - 1.0f, c2_w, 2.0f, 1.0f, 0.23f, 0.19f, 0.85f);
    }
}

void UnifiedCanvasDisplay::draw_selection_box(obs_sceneitem_t *item) {
    if (!item) return;

    float rx, ry, rw, rh;
    if (!get_item_visual_rect(item, rx, ry, rw, rh)) return;

    float thickness = 2.5f;
    // Solid red bounding box (#ff3b30)
    draw_solid_rect(rx, ry, rw, thickness, 1.0f, 0.23f, 0.19f, 1.0f);
    draw_solid_rect(rx, ry + rh - thickness, rw, thickness, 1.0f, 0.23f, 0.19f, 1.0f);
    draw_solid_rect(rx, ry, thickness, rh, 1.0f, 0.23f, 0.19f, 1.0f);
    draw_solid_rect(rx + rw - thickness, ry, thickness, rh, 1.0f, 0.23f, 0.19f, 1.0f);

    float mid_x = rx + rw * 0.5f;
    float mid_y = ry + rh * 0.5f;
    float rot_y = ry - 25.0f;

    // Stem line to rotation handle
    draw_solid_rect(mid_x - 1.0f, rot_y, 2.0f, 25.0f, 1.0f, 0.23f, 0.19f, 1.0f);
    // Red circular knob on top
    float rot_size = 9.0f;
    draw_solid_rect(mid_x - rot_size * 0.5f, rot_y - rot_size * 0.5f, rot_size, rot_size, 1.0f, 0.23f, 0.19f, 1.0f);

    // 8 resize handles (white squares with red outline)
    float hs = 8.0f;
    float hsh = hs * 0.5f;
    auto draw_handle = [this, hs, hsh](float cx, float cy) {
        draw_solid_rect(cx - hsh - 1.0f, cy - hsh - 1.0f, hs + 2.0f, hs + 2.0f, 1.0f, 0.23f, 0.19f, 1.0f);
        draw_solid_rect(cx - hsh, cy - hsh, hs, hs, 1.0f, 1.0f, 1.0f, 1.0f);
    };

    // 4 corners
    draw_handle(rx, ry);
    draw_handle(rx + rw, ry);
    draw_handle(rx, ry + rh);
    draw_handle(rx + rw, ry + rh);

    // 4 edge midpoints
    draw_handle(mid_x, ry);
    draw_handle(mid_x, ry + rh);
    draw_handle(rx, mid_y);
    draw_handle(rx + rw, mid_y);
}

void UnifiedCanvasDisplay::draw_texture(gs_texture_t *tex, float x, float y, float w, float h) {
    if (!tex || w <= 0.0f || h <= 0.0f) return;
    gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    if (!effect) return;

    gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
    gs_effect_set_texture(image, tex);

    gs_matrix_push();
    gs_matrix_translate3f(x, y, 0.0f);

    while (gs_effect_loop(effect, "Draw")) {
        gs_draw_sprite(tex, 0, (uint32_t)w, (uint32_t)h);
    }

    gs_matrix_pop();
}

void UnifiedCanvasDisplay::draw_callback(void *param, uint32_t cx, uint32_t cy) {
    auto *self = static_cast<UnifiedCanvasDisplay *>(param);
    if (self) {
        self->render(cx, cy);
    }
}

void UnifiedCanvasDisplay::render(uint32_t cx, uint32_t cy) {
    if (cx < 10 || cy < 10) return;

    // 1. Draw workspace background
    draw_solid_rect(0.0f, 0.0f, (float)cx, (float)cy, 0.078f, 0.086f, 0.098f, 1.0f);

    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }

    uint32_t vert_w = CanvasEngine::instance().width();
    uint32_t vert_h = CanvasEngine::instance().height();
    if (vert_w == 0) vert_w = 1080;
    if (vert_h == 0) vert_h = 1920;

    // Off-screen render Canvas 2 at native 1080x1920 resolution
    obs_view_t *view = CanvasEngine::instance().view();
    if (view && vert_texrender_) {
        if (gs_texrender_begin(vert_texrender_, vert_w, vert_h)) {
            struct vec4 clear_color;
            vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
            gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH, &clear_color, 1.0f, 0);
            obs_view_render(view);
            gs_texrender_end(vert_texrender_);
        }
    }
    gs_texture_t *vert_tex = vert_texrender_ ? gs_texrender_get_texture(vert_texrender_) : nullptr;

    float base_h = (float)main_h;
    float aspect_vert = (float)vert_w / (float)vert_h;
    float draw_vert_w = base_h * aspect_vert;
    float draw_vert_h = base_h;
    float scale_c2 = draw_vert_h / (float)vert_h;

    float gap = 40.0f;
    float total_w = (mode_ == UnifiedViewMode::DualView) ? ((float)main_w + gap + draw_vert_w) : draw_vert_w;
    float total_h = base_h;

    float scale_x = (float)cx / total_w;
    float scale_y = (float)cy / total_h;
    float auto_zoom = (std::min)(scale_x, scale_y) * 0.94f;
    float auto_pan_x = ((float)cx - total_w * auto_zoom) * 0.5f;
    float auto_pan_y = ((float)cy - total_h * auto_zoom) * 0.5f;

    if (!is_custom_zoom_) {
        zoom_ = auto_zoom;
        pan_x_ = auto_pan_x;
        pan_y_ = auto_pan_y;
    }

    gs_matrix_push();
    gs_matrix_translate3f(pan_x_, pan_y_, 0.0f);
    gs_matrix_scale3f(zoom_, zoom_, 1.0f);

    float c2_x = 0.0f;
    float c2_y = 0.0f;

    if (mode_ == UnifiedViewMode::DualView) {
        float main_x = 0.0f;
        float main_y = 0.0f;
        c2_x = (float)main_w + gap;

        // Render Canvas 1
        draw_canvas_frame(main_x, main_y, (float)main_w, (float)main_h, 0.35f, 0.38f, 0.45f);
        gs_matrix_push();
        gs_matrix_translate3f(main_x, main_y, 0.0f);
        obs_render_main_texture();
        gs_matrix_pop();

        // Render Canvas 2
        draw_canvas_frame(c2_x, c2_y, draw_vert_w, draw_vert_h, 0.30f, 0.58f, 1.0f);
        if (vert_tex) {
            draw_texture(vert_tex, c2_x, c2_y, draw_vert_w, draw_vert_h);
        }
    } else {
        // Render Canvas 2 Only
        c2_x = 0.0f;
        c2_y = 0.0f;
        draw_canvas_frame(c2_x, c2_y, draw_vert_w, draw_vert_h, 0.30f, 0.58f, 1.0f);
        if (vert_tex) {
            draw_texture(vert_tex, c2_x, c2_y, draw_vert_w, draw_vert_h);
        }
    }

    // 1. Draw snapping lines when dragging
    draw_snap_lines(c2_x, c2_y, draw_vert_w, draw_vert_h, scale_c2);

    // 2. Draw hover outline ("batas ujung source")
    if (hovered_item_ && hovered_item_ != selected_item_) {
        draw_hover_box(hovered_item_);
    }

    // 3. Draw selection box (red outline + 8 handles + rot knob)
    if (selected_item_) {
        draw_selection_box(selected_item_);
    }

    gs_matrix_pop();
}

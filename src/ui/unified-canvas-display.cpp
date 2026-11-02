#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "unified-canvas-display.hpp"
#include "../canvas-engine.hpp"
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
    float threshold = 12.0f; // Visual tolerance in viewport pixels

    if (std::abs(vx - rx) <= threshold && std::abs(vy - ry) <= threshold) return ResizeHandle::TopLeft;
    if (std::abs(vx - (rx + rw)) <= threshold && std::abs(vy - ry) <= threshold) return ResizeHandle::TopRight;
    if (std::abs(vx - rx) <= threshold && std::abs(vy - (ry + rh)) <= threshold) return ResizeHandle::BottomLeft;
    if (std::abs(vx - (rx + rw)) <= threshold && std::abs(vy - (ry + rh)) <= threshold) return ResizeHandle::BottomRight;

    return ResizeHandle::None;
}

void UnifiedCanvasDisplay::wheelEvent(QWheelEvent *event) {
    float delta = event->angleDelta().y();
    if (delta == 0.0f) return;

    is_custom_zoom_ = true;

    float factor = (delta > 0) ? 1.1f : 0.909f;
    float new_zoom = zoom_ * factor;
    if (new_zoom < 0.05f || new_zoom > 15.0f) return;

    const qreal dpr = devicePixelRatioF();
    QPointF mouse_pos = event->position() * dpr;
    pan_x_ = (float)(mouse_pos.x() - (mouse_pos.x() - pan_x_) * factor);
    pan_y_ = (float)(mouse_pos.y() - (mouse_pos.y() - pan_y_) * factor);
    zoom_ = new_zoom;

    update();
}

void UnifiedCanvasDisplay::mousePressEvent(QMouseEvent *event) {
    float vx = 0.0f, vy = 0.0f;
    if (!map_screen_to_viewport(event->position(), vx, vy)) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // 1. Check if clicking on an active resize handle of selected_item_
        if (selected_item_) {
            float rx, ry, rw, rh;
            if (get_item_visual_rect(selected_item_, rx, ry, rw, rh)) {
                ResizeHandle handle = get_handle_at(vx, vy, rx, ry, rw, rh);
                if (handle != ResizeHandle::None) {
                    drag_mode_ = DragMode::ResizeItem;
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

        // 2. Hit-test scene items inside Canvas 2
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
                // Clicked on empty canvas area -> Deselect
                if (selected_item_) {
                    selected_item_ = nullptr;
                    emit item_selected(nullptr);
                    update();
                }
            }
        }
    }

    // 3. Middle click, Alt+click, Space+click, or click on background -> Pan canvas
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier || event->modifiers() & Qt::ControlModifier))) {
        drag_mode_ = DragMode::PanCanvas;
        last_mouse_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void UnifiedCanvasDisplay::mouseMoveEvent(QMouseEvent *event) {
    float vx = 0.0f, vy = 0.0f;
    map_screen_to_viewport(event->position(), vx, vy);

    if (drag_mode_ == DragMode::MoveItem && selected_item_) {
        float sx = 0.0f, sy = 0.0f;
        map_viewport_to_canvas2(vx, vy, sx, sy);
        float dx = sx - (float)drag_start_scene_pos_.x();
        float dy = sy - (float)drag_start_scene_pos_.y();

        struct vec2 new_pos;
        vec2_set(&new_pos, initial_item_pos_.x + dx, initial_item_pos_.y + dy);
        obs_sceneitem_set_pos(selected_item_, &new_pos);
        update();
        event->accept();
        return;
    }

    if (drag_mode_ == DragMode::ResizeItem && selected_item_) {
        float sx = 0.0f, sy = 0.0f;
        map_viewport_to_canvas2(vx, vy, sx, sy);
        float dx = sx - (float)drag_start_scene_pos_.x();

        obs_source_t *src = obs_sceneitem_get_source(selected_item_);
        float sw = src ? (float)obs_source_get_width(src) : 100.0f;
        float sh = src ? (float)obs_source_get_height(src) : 100.0f;
        if (sw <= 0.0f) sw = 1080.0f;
        if (sh <= 0.0f) sh = 1920.0f;

        struct vec2 new_scale = initial_item_scale_;
        struct vec2 new_pos = initial_item_pos_;

        if (active_handle_ == ResizeHandle::BottomRight) {
            float new_w = initial_item_scale_.x * sw + dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
        } else if (active_handle_ == ResizeHandle::BottomLeft) {
            float new_w = initial_item_scale_.x * sw - dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            new_pos.x = initial_item_pos_.x + (initial_item_scale_.x - new_scale.x) * sw;
        } else if (active_handle_ == ResizeHandle::TopRight) {
            float new_w = initial_item_scale_.x * sw + dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            new_pos.y = initial_item_pos_.y + (initial_item_scale_.y - new_scale.y) * sh;
        } else if (active_handle_ == ResizeHandle::TopLeft) {
            float new_w = initial_item_scale_.x * sw - dx;
            new_scale.x = (std::max)(0.01f, new_w / sw);
            new_scale.y = (initial_item_scale_.y / initial_item_scale_.x) * new_scale.x;
            new_pos.x = initial_item_pos_.x + (initial_item_scale_.x - new_scale.x) * sw;
            new_pos.y = initial_item_pos_.y + (initial_item_scale_.y - new_scale.y) * sh;
        }

        obs_sceneitem_set_pos(selected_item_, &new_pos);
        obs_sceneitem_set_scale(selected_item_, &new_scale);
        update();
        event->accept();
        return;
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

    // Hover cursor feedback
    if (selected_item_) {
        float rx, ry, rw, rh;
        if (get_item_visual_rect(selected_item_, rx, ry, rw, rh)) {
            ResizeHandle handle = get_handle_at(vx, vy, rx, ry, rw, rh);
            if (handle == ResizeHandle::TopLeft || handle == ResizeHandle::BottomRight) {
                setCursor(Qt::SizeFDiagCursor);
                return;
            } else if (handle == ResizeHandle::TopRight || handle == ResizeHandle::BottomLeft) {
                setCursor(Qt::SizeBDiagCursor);
                return;
            } else if (vx >= rx && vx <= rx + rw && vy >= ry && vy <= ry + rh) {
                setCursor(Qt::SizeAllCursor);
                return;
            }
        }
    }

    unsetCursor();
    QWidget::mouseMoveEvent(event);
}

void UnifiedCanvasDisplay::mouseReleaseEvent(QMouseEvent *event) {
    drag_mode_ = DragMode::None;
    active_handle_ = ResizeHandle::None;
    unsetCursor();
    QWidget::mouseReleaseEvent(event);
}

void UnifiedCanvasDisplay::mouseDoubleClickEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    fit_to_screen();
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
    // Outer border frame
    draw_solid_rect(x - border, y - border, w + border * 2.0f, h + border * 2.0f, r, g, b, 1.0f);
    // Inner black canvas backing (ensures non-transparent solid canvas)
    draw_solid_rect(x, y, w, h, 0.0f, 0.0f, 0.0f, 1.0f);
}

void UnifiedCanvasDisplay::draw_selection_box(obs_sceneitem_t *item) {
    if (!item) return;

    float rx, ry, rw, rh;
    if (!get_item_visual_rect(item, rx, ry, rw, rh)) return;

    float thickness = 2.0f;
    // Red color: #ff3b30 -> (1.0f, 0.23f, 0.19f, 1.0f)
    draw_solid_rect(rx, ry, rw, thickness, 1.0f, 0.23f, 0.19f, 1.0f);
    draw_solid_rect(rx, ry + rh - thickness, rw, thickness, 1.0f, 0.23f, 0.19f, 1.0f);
    draw_solid_rect(rx, ry, thickness, rh, 1.0f, 0.23f, 0.19f, 1.0f);
    draw_solid_rect(rx + rw - thickness, ry, thickness, rh, 1.0f, 0.23f, 0.19f, 1.0f);

    // 4 Corner handles (white squares with red outline)
    float hs = 7.0f;
    float hsh = hs * 0.5f;
    auto draw_handle = [this, hs, hsh](float cx, float cy) {
        draw_solid_rect(cx - hsh - 1.0f, cy - hsh - 1.0f, hs + 2.0f, hs + 2.0f, 1.0f, 0.23f, 0.19f, 1.0f);
        draw_solid_rect(cx - hsh, cy - hsh, hs, hs, 1.0f, 1.0f, 1.0f, 1.0f);
    };

    draw_handle(rx, ry);
    draw_handle(rx + rw, ry);
    draw_handle(rx, ry + rh);
    draw_handle(rx + rw, ry + rh);
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

    // 1. Draw workspace background (OBS dark canvas gray #141619)
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

    // Off-screen render Canvas 2 at native 1080x1920 resolution to preserve exact aspect ratio
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

    float base_h = (float)main_h; // Visual reference height = 1080.0f
    float aspect_vert = (float)vert_w / (float)vert_h; // 1080 / 1920 = 0.5625 (9:16)
    float draw_vert_w = base_h * aspect_vert; // 1080 * 0.5625 = 607.5f
    float draw_vert_h = base_h; // 1080.0f -> EXACT SAME HEIGHT AS MAIN!

    float gap = 40.0f;
    float total_w = (mode_ == UnifiedViewMode::DualView) ? ((float)main_w + gap + draw_vert_w) : draw_vert_w;
    float total_h = base_h;

    // Dynamically calculate auto-fit zoom and perfect mathematical center in actual viewport (cx, cy)
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

    // 2. Apply viewport transformation (Zoom & Pan applied together to all canvases)
    gs_matrix_push();
    gs_matrix_translate3f(pan_x_, pan_y_, 0.0f);
    gs_matrix_scale3f(zoom_, zoom_, 1.0f);

    if (mode_ == UnifiedViewMode::DualView) {
        float main_x = 0.0f;
        float main_y = 0.0f;
        float vert_x = (float)main_w + gap;
        float vert_y = 0.0f;

        // Render Canvas 1 (Main 16:9)
        draw_canvas_frame(main_x, main_y, (float)main_w, (float)main_h, 0.35f, 0.38f, 0.45f);
        gs_matrix_push();
        gs_matrix_translate3f(main_x, main_y, 0.0f);
        obs_render_main_texture();
        gs_matrix_pop();

        // Render Canvas 2 (Vertical 9:16) - EXACT SAME VISUAL HEIGHT!
        draw_canvas_frame(vert_x, vert_y, draw_vert_w, draw_vert_h, 0.30f, 0.58f, 1.0f);
        if (vert_tex) {
            draw_texture(vert_tex, vert_x, vert_y, draw_vert_w, draw_vert_h);
        }

        // Draw interactive red bounding box on Canvas 2 selected item
        if (selected_item_) {
            draw_selection_box(selected_item_);
        }
    } else {
        // Mode == Canvas2Only: Centered 9:16 vertical canvas (matching height)
        draw_canvas_frame(0.0f, 0.0f, draw_vert_w, draw_vert_h, 0.30f, 0.58f, 1.0f);
        if (vert_tex) {
            draw_texture(vert_tex, 0.0f, 0.0f, draw_vert_w, draw_vert_h);
        }

        // Draw interactive red bounding box on Canvas 2 selected item
        if (selected_item_) {
            draw_selection_box(selected_item_);
        }
    }

    gs_matrix_pop();
}

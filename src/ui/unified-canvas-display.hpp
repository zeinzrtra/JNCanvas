#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QWidget>
#include <obs.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <graphics/math-defs.h>
#include <QPoint>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

enum class UnifiedViewMode {
    DualView = 0,
    Canvas2Only = 1
};

enum class DragMode {
    None = 0,
    PanCanvas = 1,
    MoveItem = 2,
    ResizeItem = 3,
    RotateItem = 4
};

enum class ResizeHandle {
    None = 0,
    TopLeft = 1,
    TopCenter = 2,
    TopRight = 3,
    CenterLeft = 4,
    CenterRight = 5,
    BottomLeft = 6,
    BottomCenter = 7,
    BottomRight = 8,
    Rot = 9
};

class UnifiedCanvasDisplay : public QWidget {
    Q_OBJECT
public:
    explicit UnifiedCanvasDisplay(QWidget *parent = nullptr);
    ~UnifiedCanvasDisplay() override;

    void set_mode(UnifiedViewMode mode);
    UnifiedViewMode mode() const { return mode_; }

    void fit_to_screen();
    void reset_zoom();

    obs_sceneitem_t *selected_item() const { return selected_item_; }
    void select_item(obs_sceneitem_t *item);

    // Quick Context Actions
    void add_source_by_id(const char *id, const char *name);
    void clone_from_main();
    void transform_fit();
    void transform_stretch();
    void transform_center();
    void transform_reset();
    void order_move_up();
    void order_move_down();
    void remove_selected_item();

    QPaintEngine *paintEngine() const override { return nullptr; }

signals:
    void item_selected(obs_sceneitem_t *item);
    void sources_changed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    obs_display_t *display_ = nullptr;
    gs_texrender_t *vert_texrender_ = nullptr;
    UnifiedViewMode mode_ = UnifiedViewMode::DualView;

    bool is_custom_zoom_ = false;
    float zoom_ = 1.0f;
    float pan_x_ = 0.0f;
    float pan_y_ = 0.0f;
    bool is_space_pressed_ = false;

    // Interactive item manipulation & Gizmo
    obs_sceneitem_t *selected_item_ = nullptr;
    obs_sceneitem_t *hovered_item_ = nullptr;
    DragMode drag_mode_ = DragMode::None;
    ResizeHandle active_handle_ = ResizeHandle::None;
    struct vec2 initial_item_pos_ = {};
    struct vec2 initial_item_scale_ = {};
    float initial_item_rot_ = 0.0f;
    QPointF drag_start_scene_pos_ = {};
    QPoint last_mouse_pos_;

    // Snapping lines
    bool snap_x_ = false;
    float snap_x_pos_ = 0.0f;
    bool snap_y_ = false;
    float snap_y_pos_ = 0.0f;

    void create_display();
    void destroy_display();
    static void draw_callback(void *param, uint32_t cx, uint32_t cy);
    void render(uint32_t cx, uint32_t cy);

    bool map_screen_to_viewport(const QPointF &screen_pos, float &vx, float &vy) const;
    bool map_viewport_to_canvas2(float vx, float vy, float &sx, float &sy) const;
    bool get_item_visual_rect(obs_sceneitem_t *item, float &rx, float &ry, float &rw, float &rh) const;
    ResizeHandle get_handle_at(float vx, float vy, float rx, float ry, float rw, float rh) const;

    void draw_solid_rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void draw_canvas_frame(float x, float y, float w, float h, float r, float g, float b);
    void draw_selection_box(obs_sceneitem_t *item);
    void draw_hover_box(obs_sceneitem_t *item);
    void draw_snap_lines(float c2_x, float c2_y, float c2_w, float c2_h, float scale_c2);
    void draw_texture(gs_texture_t *tex, float x, float y, float w, float h);
    void show_context_menu(const QPoint &global_pos, float vx, float vy);
};

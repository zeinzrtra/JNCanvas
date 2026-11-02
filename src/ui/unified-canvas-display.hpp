#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QWidget>
#include <obs.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <graphics/math-defs.h>
#include <QPoint>
#include <QMouseEvent>
#include <QWheelEvent>

enum class UnifiedViewMode {
    DualView = 0,
    Canvas2Only = 1
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

private:
    obs_display_t *display_ = nullptr;
    gs_texrender_t *vert_texrender_ = nullptr;
    UnifiedViewMode mode_ = UnifiedViewMode::DualView;

    float zoom_ = 1.0f;
    float pan_x_ = 0.0f;
    float pan_y_ = 0.0f;

    bool is_panning_ = false;
    QPoint last_mouse_pos_;

    void create_display();
    void destroy_display();
    static void draw_callback(void *param, uint32_t cx, uint32_t cy);
    void render(uint32_t cx, uint32_t cy);

    void draw_solid_rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void draw_canvas_frame(float x, float y, float w, float h, float r, float g, float b);
    void draw_texture(gs_texture_t *tex, float x, float y, float w, float h);
};

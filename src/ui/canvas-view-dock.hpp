#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include "canvas-central-view.hpp"
#include "../vertical-canvas.hpp"

class CanvasViewDock : public QWidget {
    Q_OBJECT
public:
    explicit CanvasViewDock(CanvasCentralView *central_view, CanvasDock *canvas_dock, QWidget *parent = nullptr);
    ~CanvasViewDock() override = default;

private slots:
    void on_canvas1_clicked();
    void on_canvas2_clicked();
    void on_dual_clicked();
    void on_fit_clicked();
    void on_reset_zoom_clicked();
    void on_clone_clicked();
    void on_mode_changed(CanvasViewMode mode);

private:
    CanvasCentralView *central_view_ = nullptr;
    CanvasDock *canvas_dock_ = nullptr;

    QPushButton *btn_canvas1_ = nullptr;
    QPushButton *btn_canvas2_ = nullptr;
    QPushButton *btn_dual_ = nullptr;

    QPushButton *btn_fit_ = nullptr;
    QPushButton *btn_reset_zoom_ = nullptr;
    QPushButton *btn_clone_ = nullptr;

    QGroupBox *sources_group_ = nullptr;
    QLabel *info_canvas1_ = nullptr;
    QLabel *info_canvas2_ = nullptr;

    void setup_ui();
    void setup_stylesheet();
    void update_button_states(CanvasViewMode mode);
};

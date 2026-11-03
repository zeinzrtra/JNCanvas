#pragma once

#include <QWidget>
#include <QMainWindow>
#include "../vertical-canvas.hpp"

enum class CanvasViewMode {
    MainOnly = 0,
    Canvas2Only = 1,
    DualView = 2
};

class CanvasCentralView : public QObject {
    Q_OBJECT
public:
    explicit CanvasCentralView(QMainWindow *main_window, CanvasDock *dock, QObject *parent = nullptr);
    ~CanvasCentralView() override;

    bool init();
    void set_mode(CanvasViewMode mode);
    CanvasViewMode mode() const { return current_mode_; }
    CanvasDock *canvasDock() const { return canvas_dock_; }

signals:
    void mode_changed(CanvasViewMode mode);

private:
    QMainWindow *main_win_ = nullptr;
    CanvasDock *canvas_dock_ = nullptr;
    CanvasViewMode current_mode_ = CanvasViewMode::MainOnly;

    QWidget *native_preview_ = nullptr;
    QWidget *preview_container_ = nullptr;

    void setup_central_preview();
    void apply_mode(CanvasViewMode mode);
};

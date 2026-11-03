#pragma once

#include <QWidget>
#include <QMainWindow>
#include "unified-canvas-display.hpp"

enum class CanvasViewMode {
    MainOnly = 0,
    Canvas2Only = 1,
    DualView = 2
};

class CanvasCentralView : public QObject {
    Q_OBJECT
public:
    explicit CanvasCentralView(QMainWindow *main_window, QObject *parent = nullptr);
    ~CanvasCentralView() override;

    bool init();
    void set_mode(CanvasViewMode mode);
    CanvasViewMode mode() const { return current_mode_; }

    UnifiedCanvasDisplay *display() const { return unified_display_; }

signals:
    void mode_changed(CanvasViewMode mode);

private:
    QMainWindow *main_win_ = nullptr;
    CanvasViewMode current_mode_ = CanvasViewMode::MainOnly;

    QWidget *native_preview_ = nullptr;
    QWidget *preview_container_ = nullptr;
    UnifiedCanvasDisplay *unified_display_ = nullptr;

    void setup_unified_display();
    void apply_mode(CanvasViewMode mode);
    void auto_populate_if_empty();
};

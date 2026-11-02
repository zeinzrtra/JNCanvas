#pragma once

#include <QWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
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

private slots:
    void on_tab_changed(int index);

private:
    QMainWindow *main_win_ = nullptr;
    CanvasViewMode current_mode_ = CanvasViewMode::MainOnly;

    QWidget *canvas_editor_ = nullptr;
    QHBoxLayout *preview_layout_ = nullptr;
    QWidget *main_preview_container_ = nullptr;

    QTabBar *tab_bar_ = nullptr;
    QWidget *tab_container_ = nullptr;
    UnifiedCanvasDisplay *unified_display_ = nullptr;

    void setup_tab_bar();
    void setup_unified_display();
    void apply_mode(CanvasViewMode mode);
};

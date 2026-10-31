#pragma once

#include <QDockWidget>
#include <QWidget>
#include <obs.h>

class CanvasDisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit CanvasDisplayWidget(QWidget *parent = nullptr);
    ~CanvasDisplayWidget() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    obs_display_t *display_ = nullptr;
    void create_display();
    void destroy_display();

    static void render_display(void *data, uint32_t cx, uint32_t cy);
};

class CanvasPreviewDock : public QDockWidget {
    Q_OBJECT
public:
    explicit CanvasPreviewDock(QWidget *parent = nullptr);
    ~CanvasPreviewDock() override = default;

private:
    CanvasDisplayWidget *display_widget_ = nullptr;
};


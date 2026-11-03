#include "canvas-view-dock.hpp"
#include "../sources-dock.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

CanvasViewDock::CanvasViewDock(CanvasCentralView *central_view, CanvasDock *canvas_dock, QWidget *parent)
    : QWidget(parent), central_view_(central_view), canvas_dock_(canvas_dock) {
    setObjectName("JNCanvasViewDockWidget");
    setup_ui();
    setup_stylesheet();

    if (central_view_) {
        connect(central_view_, &CanvasCentralView::mode_changed,
                this, &CanvasViewDock::on_mode_changed);
        update_button_states(central_view_->mode());
    }
}

void CanvasViewDock::setup_ui() {
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(6, 6, 6, 6);
    main_layout->setSpacing(6);

    // 1. Canvas Selection Group
    QGroupBox *mode_group = new QGroupBox(tr("Canvas View"), this);
    QVBoxLayout *group_layout = new QVBoxLayout(mode_group);
    group_layout->setContentsMargins(6, 8, 6, 8);
    group_layout->setSpacing(4);

    btn_canvas1_ = new QPushButton(tr("🖥️ Canvas 1 (16:9)"), mode_group);
    btn_canvas1_->setCheckable(true);
    btn_canvas1_->setFixedHeight(32);
    connect(btn_canvas1_, &QPushButton::clicked, this, &CanvasViewDock::on_canvas1_clicked);

    btn_canvas2_ = new QPushButton(tr("📱 Canvas 2 (9:16)"), mode_group);
    btn_canvas2_->setCheckable(true);
    btn_canvas2_->setFixedHeight(32);
    connect(btn_canvas2_, &QPushButton::clicked, this, &CanvasViewDock::on_canvas2_clicked);

    btn_dual_ = new QPushButton(tr("🔀 Keduanya (Dual View)"), mode_group);
    btn_dual_->setCheckable(true);
    btn_dual_->setFixedHeight(32);
    connect(btn_dual_, &QPushButton::clicked, this, &CanvasViewDock::on_dual_clicked);

    group_layout->addWidget(btn_canvas1_);
    group_layout->addWidget(btn_canvas2_);
    group_layout->addWidget(btn_dual_);
    main_layout->addWidget(mode_group);

    // 2. Viewport Controls (Fit to Screen, Reset Zoom, Clone Main)
    QHBoxLayout *zoom_layout = new QHBoxLayout();
    zoom_layout->setContentsMargins(0, 0, 0, 0);
    zoom_layout->setSpacing(4);

    btn_fit_ = new QPushButton(tr("📐 Fit Screen"), this);
    btn_fit_->setFixedHeight(26);
    btn_fit_->setToolTip(tr("Paskan ukuran canvas"));
    connect(btn_fit_, &QPushButton::clicked, this, &CanvasViewDock::on_fit_clicked);

    btn_reset_zoom_ = new QPushButton(tr("100% Zoom"), this);
    btn_reset_zoom_->setFixedHeight(26);
    connect(btn_reset_zoom_, &QPushButton::clicked, this, &CanvasViewDock::on_reset_zoom_clicked);

    btn_clone_ = new QPushButton(tr("📋 Clone Main"), this);
    btn_clone_->setFixedHeight(26);
    btn_clone_->setToolTip(tr("Salin semua source dari scene utama ke canvas vertikal"));
    connect(btn_clone_, &QPushButton::clicked, this, &CanvasViewDock::on_clone_clicked);

    zoom_layout->addWidget(btn_fit_);
    zoom_layout->addWidget(btn_reset_zoom_);
    zoom_layout->addWidget(btn_clone_);
    main_layout->addLayout(zoom_layout);

    // 3. Embedded Canvas 2 Sources Management
    sources_group_ = new QGroupBox(tr("Sources (Canvas 2)"), this);
    QVBoxLayout *sources_layout = new QVBoxLayout(sources_group_);
    sources_layout->setContentsMargins(4, 8, 4, 6);
    sources_layout->setSpacing(4);

    if (canvas_dock_ && canvas_dock_->GetSourcesDock()) {
        sources_layout->addWidget(canvas_dock_->GetSourcesDock());
        canvas_dock_->GetSourcesDock()->show();
    }

    main_layout->addWidget(sources_group_, 1);

    // 4. Resolution Info
    QVBoxLayout *info_layout = new QVBoxLayout();
    info_layout->setContentsMargins(2, 2, 2, 2);
    info_layout->setSpacing(2);

    info_canvas1_ = new QLabel(tr("• Canvas 1: 1920x1080 (16:9)"), this);
    info_canvas1_->setStyleSheet("color: #7f848e; font-size: 11px;");

    info_canvas2_ = new QLabel(tr("• Canvas 2: 1080x1920 (9:16)"), this);
    info_canvas2_->setStyleSheet("color: #7f848e; font-size: 11px;");

    info_layout->addWidget(info_canvas1_);
    info_layout->addWidget(info_canvas2_);
    main_layout->addLayout(info_layout);
}

void CanvasViewDock::setup_stylesheet() {
    setStyleSheet(R"(
        QWidget#JNCanvasViewDockWidget {
            background-color: #1e2227;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 11px;
            color: #abb2bf;
            border: 1px solid #282c34;
            border-radius: 5px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
        }
        QPushButton {
            background-color: #282c34;
            border: 1px solid #3e4451;
            border-radius: 4px;
            color: #d7dae0;
            font-size: 12px;
            font-weight: 600;
            text-align: left;
            padding-left: 12px;
        }
        QPushButton:hover {
            background-color: #353b45;
            color: #ffffff;
            border-color: #4d94ff;
        }
        QPushButton:checked {
            background-color: #203c5d;
            border: 2px solid #4d94ff;
            color: #ffffff;
        }
    )");
}

void CanvasViewDock::on_canvas1_clicked() {
    if (central_view_) {
        central_view_->set_mode(CanvasViewMode::MainOnly);
    }
}

void CanvasViewDock::on_canvas2_clicked() {
    if (central_view_) {
        central_view_->set_mode(CanvasViewMode::Canvas2Only);
    }
}

void CanvasViewDock::on_dual_clicked() {
    if (central_view_) {
        central_view_->set_mode(CanvasViewMode::DualView);
    }
}

void CanvasViewDock::on_fit_clicked() {
    if (canvas_dock_) {
        canvas_dock_->FitScreen();
    }
}

void CanvasViewDock::on_reset_zoom_clicked() {
    if (canvas_dock_) {
        canvas_dock_->ResetZoom();
    }
}

void CanvasViewDock::on_clone_clicked() {
    if (canvas_dock_) {
        canvas_dock_->CloneFromMain();
    }
}

void CanvasViewDock::on_mode_changed(CanvasViewMode mode) {
    update_button_states(mode);
}

void CanvasViewDock::update_button_states(CanvasViewMode mode) {
    btn_canvas1_->blockSignals(true);
    btn_canvas2_->blockSignals(true);
    btn_dual_->blockSignals(true);

    btn_canvas1_->setChecked(mode == CanvasViewMode::MainOnly);
    btn_canvas2_->setChecked(mode == CanvasViewMode::Canvas2Only);
    btn_dual_->setChecked(mode == CanvasViewMode::DualView);

    btn_canvas1_->blockSignals(false);
    btn_canvas2_->blockSignals(false);
    btn_dual_->blockSignals(false);

    if (sources_group_) {
        if (mode == CanvasViewMode::MainOnly) {
            sources_group_->setTitle(tr("Sources (Canvas 2 - Vertical)"));
        } else {
            sources_group_->setTitle(tr("Sources (Canvas 2) - Active 🟢"));
        }
    }
}

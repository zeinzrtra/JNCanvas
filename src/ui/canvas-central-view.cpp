#include "canvas-central-view.hpp"
#include <QFrame>

CanvasCentralView::CanvasCentralView(QMainWindow *main_window, QObject *parent)
    : QObject(parent), main_win_(main_window) {}

CanvasCentralView::~CanvasCentralView() {
    if (vertical_container_) {
        vertical_container_->deleteLater();
        vertical_container_ = nullptr;
    }
    if (tab_container_) {
        tab_container_->deleteLater();
        tab_container_ = nullptr;
    }
}

bool CanvasCentralView::init() {
    if (!main_win_) return false;

    canvas_editor_ = main_win_->findChild<QWidget *>("canvasEditor");
    preview_layout_ = main_win_->findChild<QHBoxLayout *>("previewLayout");
    main_preview_container_ = main_win_->findChild<QWidget *>("previewContainer");

    if (!canvas_editor_ || !preview_layout_ || !main_preview_container_) {
        blog(LOG_WARNING, "[JNCanvas] Could not locate central preview widgets in OBS main window.");
        return false;
    }

    setup_tab_bar();
    setup_vertical_container();

    // Default to DualView so user immediately sees both side-by-side, or MainOnly
    set_mode(CanvasViewMode::DualView);

    blog(LOG_INFO, "[JNCanvas] Central canvas tabbed view integrated successfully.");
    return true;
}

void CanvasCentralView::setup_tab_bar() {
    QWidget *central = main_win_->centralWidget();
    if (!central) return;

    QVBoxLayout *central_layout = central->findChild<QVBoxLayout *>("verticalLayout");
    if (!central_layout) return;

    tab_container_ = new QWidget(central);
    tab_container_->setObjectName("JNCanvasTabBarContainer");

    QHBoxLayout *h_layout = new QHBoxLayout(tab_container_);
    h_layout->setContentsMargins(6, 2, 6, 2);
    h_layout->setSpacing(6);

    tab_bar_ = new QTabBar(tab_container_);
    tab_bar_->setObjectName("JNCanvasCentralTabBar");
    tab_bar_->addTab(tr("🖥️ Main (16:9)"));
    tab_bar_->addTab(tr("📱 Canvas 2 (9:16)"));
    tab_bar_->addTab(tr("🔀 Dual View"));
    tab_bar_->setExpanding(false);

    tab_bar_->setStyleSheet(R"(
        QTabBar#JNCanvasCentralTabBar {
            background: transparent;
        }
        QTabBar#JNCanvasCentralTabBar::tab {
            background-color: #21252b;
            color: #9da5b4;
            padding: 4px 14px;
            margin-right: 4px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-size: 11px;
            font-weight: 600;
        }
        QTabBar#JNCanvasCentralTabBar::tab:selected {
            background-color: #2b303a;
            color: #4d94ff;
            border-bottom: 2px solid #4d94ff;
        }
        QTabBar#JNCanvasCentralTabBar::tab:hover {
            background-color: #282c34;
            color: #abb2bf;
        }
    )");

    connect(tab_bar_, &QTabBar::currentChanged, this, &CanvasCentralView::on_tab_changed);

    h_layout->addWidget(tab_bar_);
    h_layout->addStretch(1);

    // Insert at index 0 of verticalLayout (right above canvasEditor)
    central_layout->insertWidget(0, tab_container_);
}

void CanvasCentralView::setup_vertical_container() {
    vertical_container_ = new QWidget(canvas_editor_);
    vertical_container_->setObjectName("JNCanvasVerticalCentralContainer");

    QVBoxLayout *v_layout = new QVBoxLayout(vertical_container_);
    v_layout->setContentsMargins(0, 0, 0, 0);
    v_layout->setSpacing(2);

    // Title header matching OBS previewLabel style
    QLabel *title_label = new QLabel(tr("Canvas 2 (9:16)"), vertical_container_);
    title_label->setObjectName("JNCanvasTitleLabel");
    title_label->setStyleSheet("font-weight: bold; font-size: 11px; color: #abb2bf; padding: 2px 6px;");
    v_layout->addWidget(title_label);

    vertical_display_ = new CanvasDisplayWidget(vertical_container_);
    vertical_display_->setObjectName("JNCanvasCentralDisplay");
    v_layout->addWidget(vertical_display_, 1);

    // Add into OBS QHBoxLayout (previewLayout) right next to main previewContainer
    preview_layout_->addWidget(vertical_container_);
}

void CanvasCentralView::on_tab_changed(int index) {
    CanvasViewMode mode = CanvasViewMode::MainOnly;
    if (index == 1) mode = CanvasViewMode::Canvas2Only;
    else if (index == 2) mode = CanvasViewMode::DualView;

    set_mode(mode);
}

void CanvasCentralView::set_mode(CanvasViewMode mode) {
    current_mode_ = mode;
    if (tab_bar_ && tab_bar_->currentIndex() != static_cast<int>(mode)) {
        tab_bar_->blockSignals(true);
        tab_bar_->setCurrentIndex(static_cast<int>(mode));
        tab_bar_->blockSignals(false);
    }
    apply_mode(mode);
}

void CanvasCentralView::apply_mode(CanvasViewMode mode) {
    switch (mode) {
    case CanvasViewMode::MainOnly:
        if (main_preview_container_) main_preview_container_->show();
        if (vertical_container_) vertical_container_->hide();
        break;
    case CanvasViewMode::Canvas2Only:
        if (main_preview_container_) main_preview_container_->hide();
        if (vertical_container_) vertical_container_->show();
        break;
    case CanvasViewMode::DualView:
        if (main_preview_container_) main_preview_container_->show();
        if (vertical_container_) vertical_container_->show();
        break;
    }
}

#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabBar>
#include <obs.h>

class CanvasControlDock : public QDockWidget {
    Q_OBJECT
public:
    explicit CanvasControlDock(QWidget *parent = nullptr);
    ~CanvasControlDock() override = default;

    void refresh_scenes();
    void refresh_sources();

private slots:
    void on_tab_changed(int index);
    void on_source_selection_changed();
    void on_scene_selection_changed();

    // Action buttons
    void on_add_clicked();
    void on_delete_clicked();
    void on_properties_clicked();
    void on_filters_clicked();
    void on_move_up_clicked();
    void on_move_down_clicked();

    // Clone & Context Menu
    void show_source_context_menu(const QPoint &pos);
    void show_scene_context_menu(const QPoint &pos);

    // Record & Backtrack
    void on_record_toggled();
    void on_backtrack_clicked();

private:
    void setup_ui();
    void setup_stylesheet();

    // Header Widgets
    QLabel *selected_source_label_ = nullptr;
    QPushButton *btn_properties_ = nullptr;
    QPushButton *btn_filters_ = nullptr;

    // Body Stacked Widget & Lists
    QStackedWidget *stacked_content_ = nullptr;
    QListWidget *scene_list_widget_ = nullptr;
    QListWidget *source_list_widget_ = nullptr;

    // Bottom Action Bar
    QPushButton *btn_add_ = nullptr;
    QPushButton *btn_delete_ = nullptr;
    QPushButton *btn_item_props_ = nullptr;
    QPushButton *btn_move_up_ = nullptr;
    QPushButton *btn_move_down_ = nullptr;

    // Bottom Tab Bar
    QTabBar *bottom_tab_bar_ = nullptr;

    // Controls
    QPushButton *btn_record_ = nullptr;
    QPushButton *btn_backtrack_ = nullptr;
};

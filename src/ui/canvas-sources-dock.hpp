#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <obs.h>

class CanvasSourcesDock : public QWidget {
    Q_OBJECT
public:
    explicit CanvasSourcesDock(QWidget *parent = nullptr);
    ~CanvasSourcesDock() override = default;

    void refresh_sources();

public slots:
    void on_scene_changed(obs_scene_t *scene);

private slots:
    void on_add_source_menu();
    void on_remove_source();
    void on_move_up();
    void on_move_down();
    void on_properties();
    void on_filters();
    void on_record_clicked();
    void on_backtrack_clicked();
    void on_selection_changed();
    void show_context_menu(const QPoint &pos);

private:
    QLabel *source_label_ = nullptr;
    QPushButton *btn_properties_ = nullptr;
    QPushButton *btn_filters_ = nullptr;

    QListWidget *source_list_ = nullptr;
    QPushButton *btn_add_ = nullptr;
    QPushButton *btn_remove_ = nullptr;
    QPushButton *btn_up_ = nullptr;
    QPushButton *btn_down_ = nullptr;

    QPushButton *btn_record_ = nullptr;
    QPushButton *btn_backtrack_ = nullptr;

    void setup_ui();
    void setup_stylesheet();
    obs_sceneitem_t *get_selected_sceneitem();
};

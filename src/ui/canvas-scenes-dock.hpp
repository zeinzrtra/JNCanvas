#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <obs.h>

class CanvasScenesDock : public QWidget {
    Q_OBJECT
public:
    explicit CanvasScenesDock(QWidget *parent = nullptr);
    ~CanvasScenesDock() override = default;

    void refresh_scenes();

signals:
    void scene_changed(obs_scene_t *scene);

private slots:
    void on_add_scene();
    void on_remove_scene();
    void on_move_up();
    void on_move_down();
    void on_selection_changed();
    void on_item_renamed(QListWidgetItem *item);
    void show_context_menu(const QPoint &pos);

private:
    QListWidget *scene_list_ = nullptr;
    QPushButton *btn_add_ = nullptr;
    QPushButton *btn_remove_ = nullptr;
    QPushButton *btn_up_ = nullptr;
    QPushButton *btn_down_ = nullptr;

    void setup_ui();
    void setup_stylesheet();
};


#include "canvas-scenes-dock.hpp"
#include "../canvas-engine.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>

CanvasScenesDock::CanvasScenesDock(QWidget *parent)
    : QWidget(parent) {
    setObjectName("JNCanvasScenesDockWidget");
    setup_ui();
    setup_stylesheet();
    refresh_scenes();
}

void CanvasScenesDock::setup_ui() {
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(4);

    scene_list_ = new QListWidget(this);
    scene_list_->setObjectName("JNCanvasSceneList");
    scene_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(scene_list_, &QListWidget::customContextMenuRequested,
            this, &CanvasScenesDock::show_context_menu);
    connect(scene_list_, &QListWidget::itemSelectionChanged,
            this, &CanvasScenesDock::on_selection_changed);
    connect(scene_list_, &QListWidget::itemChanged,
            this, &CanvasScenesDock::on_item_renamed);
    main_layout->addWidget(scene_list_, 1);

    // Bottom action bar (+, -, Up, Down)
    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setContentsMargins(0, 0, 0, 0);
    btn_layout->setSpacing(2);

    btn_add_ = new QPushButton("+", this);
    btn_add_->setFixedSize(28, 24);
    btn_add_->setToolTip(tr("Add Scene"));
    connect(btn_add_, &QPushButton::clicked, this, &CanvasScenesDock::on_add_scene);

    btn_remove_ = new QPushButton("🗑", this);
    btn_remove_->setFixedSize(28, 24);
    btn_remove_->setToolTip(tr("Remove Scene"));
    connect(btn_remove_, &QPushButton::clicked, this, &CanvasScenesDock::on_remove_scene);

    btn_up_ = new QPushButton("▲", this);
    btn_up_->setFixedSize(28, 24);
    btn_up_->setToolTip(tr("Move Up"));
    connect(btn_up_, &QPushButton::clicked, this, &CanvasScenesDock::on_move_up);

    btn_down_ = new QPushButton("▼", this);
    btn_down_->setFixedSize(28, 24);
    btn_down_->setToolTip(tr("Move Down"));
    connect(btn_down_, &QPushButton::clicked, this, &CanvasScenesDock::on_move_down);

    btn_layout->addWidget(btn_add_);
    btn_layout->addWidget(btn_remove_);
    btn_layout->addWidget(btn_up_);
    btn_layout->addWidget(btn_down_);
    btn_layout->addStretch(1);

    main_layout->addLayout(btn_layout);
}

void CanvasScenesDock::setup_stylesheet() {
    setStyleSheet(R"(
        QWidget#JNCanvasScenesDockWidget {
            background-color: #1e2227;
        }
        QListWidget#JNCanvasSceneList {
            background-color: #17191d;
            border: 1px solid #282c34;
            border-radius: 4px;
            color: #d7dae0;
            padding: 2px;
        }
        QListWidget#JNCanvasSceneList::item {
            padding: 5px 8px;
            border-radius: 3px;
        }
        QListWidget#JNCanvasSceneList::item:selected {
            background-color: #2c313a;
            color: #4d94ff;
            font-weight: 600;
        }
        QPushButton {
            background-color: #282c34;
            border: 1px solid #3e4451;
            border-radius: 3px;
            color: #abb2bf;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #353b45;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #21252b;
        }
    )");
}

void CanvasScenesDock::refresh_scenes() {
    scene_list_->blockSignals(true);
    scene_list_->clear();

    auto scenes = CanvasEngine::instance().get_all_scenes();
    obs_scene_t *current = CanvasEngine::instance().current_scene();

    for (obs_scene_t *scene : scenes) {
        obs_source_t *src = obs_scene_get_source(scene);
        const char *name = src ? obs_source_get_name(src) : "Scene";
        QListWidgetItem *item = new QListWidgetItem(QString::fromUtf8(name), scene_list_);
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void *>(scene)));

        if (scene == current) {
            scene_list_->setCurrentItem(item);
        }
    }

    scene_list_->blockSignals(false);
}

void CanvasScenesDock::on_add_scene() {
    bool ok = false;
    QString name = QInputDialog::getText(
        this, tr("Add Scene"), tr("Please enter the name of the scene:"),
        QLineEdit::Normal, tr("Scene %1").arg(scene_list_->count() + 1), &ok);

    if (ok && !name.trimmed().isEmpty()) {
        obs_scene_t *scene = CanvasEngine::instance().create_scene(name.toStdString());
        if (scene) {
            CanvasEngine::instance().set_current_scene(scene);
            refresh_scenes();
            emit scene_changed(scene);
        }
    }
}

void CanvasScenesDock::on_remove_scene() {
    QListWidgetItem *item = scene_list_->currentItem();
    if (!item) return;

    obs_scene_t *scene = static_cast<obs_scene_t *>(item->data(Qt::UserRole).value<void *>());
    if (!scene) return;

    if (scene_list_->count() <= 1) {
        QMessageBox::warning(this, tr("Notice"), tr("Cannot delete the only scene."));
        return;
    }

    auto res = QMessageBox::question(
        this, tr("Confirm Delete"),
        tr("Are you sure you want to remove '%1'?").arg(item->text()),
        QMessageBox::Yes | QMessageBox::No);

    if (res == QMessageBox::Yes) {
        CanvasEngine::instance().remove_scene(scene);
        refresh_scenes();
        emit scene_changed(CanvasEngine::instance().current_scene());
    }
}

void CanvasScenesDock::on_move_up() {
    int row = scene_list_->currentRow();
    if (row > 0) {
        QListWidgetItem *item = scene_list_->takeItem(row);
        scene_list_->insertItem(row - 1, item);
        scene_list_->setCurrentRow(row - 1);
    }
}

void CanvasScenesDock::on_move_down() {
    int row = scene_list_->currentRow();
    if (row >= 0 && row < scene_list_->count() - 1) {
        QListWidgetItem *item = scene_list_->takeItem(row);
        scene_list_->insertItem(row + 1, item);
        scene_list_->setCurrentRow(row + 1);
    }
}

void CanvasScenesDock::on_selection_changed() {
    QListWidgetItem *item = scene_list_->currentItem();
    if (!item) return;

    obs_scene_t *scene = static_cast<obs_scene_t *>(item->data(Qt::UserRole).value<void *>());
    if (scene) {
        CanvasEngine::instance().set_current_scene(scene);
        emit scene_changed(scene);
    }
}

void CanvasScenesDock::on_item_renamed(QListWidgetItem *item) {
    if (!item) return;
    obs_scene_t *scene = static_cast<obs_scene_t *>(item->data(Qt::UserRole).value<void *>());
    if (scene) {
        obs_source_t *src = obs_scene_get_source(scene);
        if (src) {
            obs_source_set_name(src, item->text().toUtf8().constData());
        }
    }
}

void CanvasScenesDock::show_context_menu(const QPoint &pos) {
    QMenu menu(this);
    QAction *act_add = menu.addAction(tr("Add Scene"));
    QAction *act_remove = menu.addAction(tr("Remove Scene"));
    QAction *act_rename = menu.addAction(tr("Rename"));

    QAction *selected = menu.exec(scene_list_->mapToGlobal(pos));
    if (selected == act_add) {
        on_add_scene();
    } else if (selected == act_remove) {
        on_remove_scene();
    } else if (selected == act_rename) {
        QListWidgetItem *item = scene_list_->currentItem();
        if (item) {
            scene_list_->editItem(item);
        }
    }
}

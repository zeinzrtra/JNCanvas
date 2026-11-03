#include "canvas-sources-dock.hpp"
#include "../canvas-engine.hpp"
#include "../record-engine.hpp"
#include <obs-frontend-api.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QTimer>
#include <cstring>

CanvasSourcesDock::CanvasSourcesDock(QWidget *parent)
    : QWidget(parent) {
    setObjectName("JNCanvasSourcesDockWidget");
    setup_ui();
    setup_stylesheet();
    refresh_sources();
}

void CanvasSourcesDock::setup_ui() {
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(4);

    // Top Header (Selected Source Label + Properties + Filters)
    QHBoxLayout *header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);

    source_label_ = new QLabel(tr("No source selected"), this);
    source_label_->setStyleSheet("font-weight: bold; font-size: 12px; color: #abb2bf;");

    btn_properties_ = new QPushButton(tr("⚙ Props"), this);
    btn_properties_->setObjectName("headerBtn");
    connect(btn_properties_, &QPushButton::clicked, this, &CanvasSourcesDock::on_properties);

    btn_filters_ = new QPushButton(tr("🎨 Filters"), this);
    btn_filters_->setObjectName("headerBtn");
    connect(btn_filters_, &QPushButton::clicked, this, &CanvasSourcesDock::on_filters);

    header_layout->addWidget(source_label_, 1);
    header_layout->addWidget(btn_properties_);
    header_layout->addWidget(btn_filters_);
    main_layout->addLayout(header_layout);

    // Source List
    source_list_ = new QListWidget(this);
    source_list_->setObjectName("JNCanvasSourceList");
    source_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(source_list_, &QListWidget::customContextMenuRequested,
            this, &CanvasSourcesDock::show_context_menu);
    connect(source_list_, &QListWidget::itemSelectionChanged,
            this, &CanvasSourcesDock::on_selection_changed);
    main_layout->addWidget(source_list_, 1);

    // Bottom Action Bar
    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setContentsMargins(0, 0, 0, 0);
    btn_layout->setSpacing(3);

    btn_add_ = new QPushButton("+", this);
    btn_add_->setFixedSize(28, 24);
    btn_add_->setToolTip(tr("Add Source"));
    connect(btn_add_, &QPushButton::clicked, this, &CanvasSourcesDock::on_add_source_menu);

    btn_remove_ = new QPushButton("🗑", this);
    btn_remove_->setFixedSize(28, 24);
    btn_remove_->setToolTip(tr("Remove Source"));
    connect(btn_remove_, &QPushButton::clicked, this, &CanvasSourcesDock::on_remove_source);

    btn_up_ = new QPushButton("▲", this);
    btn_up_->setFixedSize(28, 24);
    btn_up_->setToolTip(tr("Move Up"));
    connect(btn_up_, &QPushButton::clicked, this, &CanvasSourcesDock::on_move_up);

    btn_down_ = new QPushButton("▼", this);
    btn_down_->setFixedSize(28, 24);
    btn_down_->setToolTip(tr("Move Down"));
    connect(btn_down_, &QPushButton::clicked, this, &CanvasSourcesDock::on_move_down);

    btn_clone_ = new QPushButton(tr("📋 Clone Main"), this);
    btn_clone_->setFixedHeight(24);
    btn_clone_->setToolTip(tr("Clone all sources from main OBS scene to Canvas 2"));
    connect(btn_clone_, &QPushButton::clicked, this, &CanvasSourcesDock::on_clone_main);

    btn_layout->addWidget(btn_add_);
    btn_layout->addWidget(btn_remove_);
    btn_layout->addWidget(btn_up_);
    btn_layout->addWidget(btn_down_);
    btn_layout->addWidget(btn_clone_);
    btn_layout->addStretch(1);

    // Quick REC & CLIP buttons
    btn_record_ = new QPushButton(tr("⏺ REC"), this);
    btn_record_->setObjectName("recBtn");
    connect(btn_record_, &QPushButton::clicked, this, &CanvasSourcesDock::on_record_clicked);

    btn_backtrack_ = new QPushButton(tr("⚡ CLIP"), this);
    btn_backtrack_->setObjectName("clipBtn");
    connect(btn_backtrack_, &QPushButton::clicked, this, &CanvasSourcesDock::on_backtrack_clicked);

    btn_layout->addWidget(btn_record_);
    btn_layout->addWidget(btn_backtrack_);

    main_layout->addLayout(btn_layout);
}

void CanvasSourcesDock::setup_stylesheet() {
    setStyleSheet(R"(
        QWidget#JNCanvasSourcesDockWidget {
            background-color: #1e2227;
        }
        QListWidget#JNCanvasSourceList {
            background-color: #17191d;
            border: 1px solid #282c34;
            border-radius: 4px;
            color: #d7dae0;
            padding: 2px;
        }
        QListWidget#JNCanvasSourceList::item {
            padding: 4px 6px;
            border-radius: 3px;
        }
        QListWidget#JNCanvasSourceList::item:selected {
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
        QPushButton#headerBtn {
            padding: 3px 8px;
            font-size: 11px;
        }
        QPushButton#recBtn {
            background-color: #3e262a;
            border: 1px solid #d04550;
            color: #ff6b76;
            padding: 2px 8px;
        }
        QPushButton#clipBtn {
            background-color: #2e3524;
            border: 1px solid #789938;
            color: #98c379;
            padding: 2px 8px;
        }
    )");
}

void CanvasSourcesDock::on_scene_changed(obs_scene_t *scene) {
    Q_UNUSED(scene);
    refresh_sources();
}

void CanvasSourcesDock::refresh_sources() {
    source_list_->blockSignals(true);
    source_list_->clear();

    obs_scene_t *scene = CanvasEngine::instance().current_scene();
    if (scene) {
        obs_scene_enum_items(
            scene,
            [](obs_scene_t *s, obs_sceneitem_t *item, void *p) -> bool {
                Q_UNUSED(s);
                auto *list = static_cast<QListWidget *>(p);
                obs_source_t *src = obs_sceneitem_get_source(item);
                if (src) {
                    const char *name = obs_source_get_name(src);
                    bool visible = obs_sceneitem_visible(item);
                    bool locked = obs_sceneitem_locked(item);

                    QString prefix = QString("%1 %2 ").arg(visible ? "👁" : "🕶", locked ? "🔒" : "🔓");
                    QListWidgetItem *list_item = new QListWidgetItem(prefix + QString::fromUtf8(name));
                    list_item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void *>(item)));
                    list->insertItem(0, list_item);
                }
                return true;
            },
            source_list_);
    }

    source_list_->blockSignals(false);
    on_selection_changed();
}

obs_sceneitem_t *CanvasSourcesDock::get_selected_sceneitem() {
    QListWidgetItem *item = source_list_->currentItem();
    if (!item) return nullptr;
    return static_cast<obs_sceneitem_t *>(item->data(Qt::UserRole).value<void *>());
}

void CanvasSourcesDock::select_sceneitem(obs_sceneitem_t *target_item) {
    source_list_->blockSignals(true);
    if (!target_item) {
        source_list_->clearSelection();
    } else {
        for (int i = 0; i < source_list_->count(); ++i) {
            QListWidgetItem *witem = source_list_->item(i);
            obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(witem->data(Qt::UserRole).value<void *>());
            if (sitem == target_item) {
                source_list_->setCurrentItem(witem);
                break;
            }
        }
    }
    source_list_->blockSignals(false);
    on_selection_changed();
}

void CanvasSourcesDock::on_selection_changed() {
    obs_sceneitem_t *item = get_selected_sceneitem();
    emit sceneitem_selected(item);

    if (item) {
        obs_source_t *src = obs_sceneitem_get_source(item);
        if (src) {
            source_label_->setText(QString::fromUtf8(obs_source_get_name(src)));
            btn_properties_->setEnabled(true);
            btn_filters_->setEnabled(true);
            return;
        }
    }
    source_label_->setText(tr("No source selected"));
    btn_properties_->setEnabled(false);
    btn_filters_->setEnabled(false);
}

void CanvasSourcesDock::on_add_source_menu() {
    QMenu menu(this);

    // 1. Clone from Main OBS Scene
    QMenu *clone_menu = menu.addMenu(tr("📋 Clone from Main OBS Scene"));
    obs_source_t *main_scene_src = obs_frontend_get_current_scene();
    if (main_scene_src) {
        obs_scene_t *main_scene = obs_scene_from_source(main_scene_src);
        if (main_scene) {
            obs_scene_enum_items(
                main_scene,
                [](obs_scene_t *s, obs_sceneitem_t *item, void *p) -> bool {
                    Q_UNUSED(s);
                    auto *sub_menu = static_cast<QMenu *>(p);
                    obs_source_t *src = obs_sceneitem_get_source(item);
                    if (src) {
                        const char *name = obs_source_get_name(src);
                        QAction *act = sub_menu->addAction(QString::fromUtf8(name));
                        act->setData(QVariant::fromValue(static_cast<void *>(src)));
                    }
                    return true;
                },
                clone_menu);
        }
        obs_source_release(main_scene_src);
    }

    // 2. Add Existing OBS Source
    QMenu *existing_menu = menu.addMenu(tr("➕ Add Existing Source"));
    obs_enum_sources(
        [](void *data, obs_source_t *src) -> bool {
            auto *sub_menu = static_cast<QMenu *>(data);
            uint32_t caps = obs_source_get_output_flags(src);
            if ((caps & OBS_SOURCE_VIDEO) && (caps & OBS_SOURCE_INTERACTION || !(caps & OBS_SOURCE_CUSTOM_DRAW))) {
                const char *name = obs_source_get_name(src);
                if (name && strlen(name) > 0) {
                    QAction *act = sub_menu->addAction(QString::fromUtf8(name));
                    act->setData(QVariant::fromValue(static_cast<void *>(src)));
                }
            }
            return true;
        },
        existing_menu);

    QAction *selected = menu.exec(btn_add_->mapToGlobal(QPoint(0, btn_add_->height())));
    if (selected && selected->data().isValid()) {
        obs_source_t *src = static_cast<obs_source_t *>(selected->data().value<void *>());
        if (src) {
            obs_scene_t *current_scene = CanvasEngine::instance().current_scene();
            if (current_scene) {
                obs_scene_add(current_scene, src);
                refresh_sources();
            }
        }
    }
}

void CanvasSourcesDock::on_remove_source() {
    obs_sceneitem_t *item = get_selected_sceneitem();
    if (!item) return;

    obs_sceneitem_remove(item);
    refresh_sources();
}

void CanvasSourcesDock::on_move_up() {
    obs_sceneitem_t *item = get_selected_sceneitem();
    if (!item) return;

    obs_scene_t *scene = CanvasEngine::instance().current_scene();
    if (scene) {
        int order = (int)obs_sceneitem_get_order_position(item);
        obs_sceneitem_set_order_position(item, order + 1);
        refresh_sources();
    }
}

void CanvasSourcesDock::on_move_down() {
    obs_sceneitem_t *item = get_selected_sceneitem();
    if (!item) return;

    obs_scene_t *scene = CanvasEngine::instance().current_scene();
    if (scene) {
        int order = (int)obs_sceneitem_get_order_position(item);
        if (order > 0) {
            obs_sceneitem_set_order_position(item, order - 1);
            refresh_sources();
        }
    }
}

void CanvasSourcesDock::on_properties() {
    obs_sceneitem_t *item = get_selected_sceneitem();
    if (!item) return;
    obs_source_t *src = obs_sceneitem_get_source(item);
    if (src) {
        obs_frontend_open_source_properties(src);
    }
}

void CanvasSourcesDock::on_filters() {
    obs_sceneitem_t *item = get_selected_sceneitem();
    if (!item) return;
    obs_source_t *src = obs_sceneitem_get_source(item);
    if (src) {
        obs_frontend_open_source_filters(src);
    }
}

void CanvasSourcesDock::on_record_clicked() {
    if (RecordEngine::instance().is_recording()) {
        RecordEngine::instance().stop_recording();
        btn_record_->setText(tr("⏺ REC"));
        btn_record_->setStyleSheet("");
    } else {
        if (RecordEngine::instance().start_recording()) {
            btn_record_->setText(tr("⏹ STOP"));
            btn_record_->setStyleSheet("background-color: #d04550; color: #ffffff;");
        }
    }
}

void CanvasSourcesDock::on_backtrack_clicked() {
    if (RecordEngine::instance().save_replay_buffer()) {
        btn_backtrack_->setText(tr("SAVED!"));
        QTimer::singleShot(1500, this, [this]() {
            btn_backtrack_->setText(tr("⚡ CLIP"));
        });
    }
}

void CanvasSourcesDock::show_context_menu(const QPoint &pos) {
    obs_sceneitem_t *item = get_selected_sceneitem();
    QMenu menu(this);

    if (item) {
        bool visible = obs_sceneitem_visible(item);
        bool locked = obs_sceneitem_locked(item);

        QAction *act_vis = menu.addAction(visible ? tr("Hide") : tr("Show"));
        QAction *act_lock = menu.addAction(locked ? tr("Unlock") : tr("Lock"));
        menu.addSeparator();
        QAction *act_props = menu.addAction(tr("Properties"));
        QAction *act_filters = menu.addAction(tr("Filters"));
        menu.addSeparator();
        QAction *act_remove = menu.addAction(tr("Remove"));

        QAction *sel = menu.exec(source_list_->mapToGlobal(pos));
        if (sel == act_vis) {
            obs_sceneitem_set_visible(item, !visible);
            refresh_sources();
        } else if (sel == act_lock) {
            obs_sceneitem_set_locked(item, !locked);
            refresh_sources();
        } else if (sel == act_props) {
            on_properties();
        } else if (sel == act_filters) {
            on_filters();
        } else if (sel == act_remove) {
            on_remove_source();
        }
    } else {
        QAction *act_add = menu.addAction(tr("Add Source..."));
        QAction *sel = menu.exec(source_list_->mapToGlobal(pos));
        if (sel == act_add) {
            on_add_source_menu();
        }
    }
}

void CanvasSourcesDock::on_clone_main() {
    obs_scene_t *dest_scene = CanvasEngine::instance().current_scene();
    obs_source_t *main_scene_src = obs_frontend_get_current_scene();
    if (!dest_scene || !main_scene_src) return;

    obs_video_info ovi = {};
    uint32_t main_w = 1920, main_h = 1080;
    if (obs_get_video_info(&ovi)) {
        main_w = ovi.base_width;
        main_h = ovi.base_height;
    }
    uint32_t cw = CanvasEngine::instance().width();
    uint32_t ch = CanvasEngine::instance().height();
    if (cw == 0) cw = 1080;
    if (ch == 0) ch = 1920;

    float ratio = (main_w > 0) ? ((float)cw / (float)main_w) : 1.0f;
    float scaled_h = (float)main_h * ratio;
    float offset_y = ((float)ch - scaled_h) * 0.5f;

    struct CloneContext {
        obs_scene_t *dest;
        float ratio;
        float offset_y;
    } ctx = {dest_scene, ratio, offset_y};

    obs_scene_t *main_scene = obs_scene_from_source(main_scene_src);
    if (main_scene) {
        obs_scene_enum_items(
            main_scene,
            [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
                auto *c = static_cast<CloneContext *>(param);
                obs_source_t *src = obs_sceneitem_get_source(item);
                if (src) {
                    obs_sceneitem_t *new_item = obs_scene_add(c->dest, src);
                    if (new_item) {
                        struct vec2 pos, scale;
                        obs_sceneitem_get_pos(item, &pos);
                        obs_sceneitem_get_scale(item, &scale);

                        struct vec2 new_pos = {pos.x * c->ratio, c->offset_y + pos.y * c->ratio};
                        struct vec2 new_scale = {scale.x * c->ratio, scale.y * c->ratio};

                        obs_sceneitem_set_pos(new_item, &new_pos);
                        obs_sceneitem_set_scale(new_item, &new_scale);
                    }
                }
                return true;
            },
            &ctx);
        refresh_sources();
    }
    obs_source_release(main_scene_src);
}


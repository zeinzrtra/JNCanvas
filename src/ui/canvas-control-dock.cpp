#include "canvas-control-dock.hpp"
#include "../canvas-engine.hpp"
#include "../record-engine.hpp"
#include <obs-frontend-api.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>

CanvasControlDock::CanvasControlDock(QWidget *parent)
    : QDockWidget(tr("Canvas 2"), parent) {
    setObjectName("JNCanvasControlDock");
    setup_ui();
    setup_stylesheet();
    refresh_scenes();
    refresh_sources();
}

void CanvasControlDock::setup_ui() {
    QWidget *main_widget = new QWidget(this);
    QVBoxLayout *main_layout = new QVBoxLayout(main_widget);
    main_layout->setContentsMargins(6, 6, 6, 6);
    main_layout->setSpacing(4);

    // ========================================================================
    // 1. Top Header Bar (Source Name + Properties + Filters)
    // ========================================================================
    QHBoxLayout *header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);

    selected_source_label_ = new QLabel(tr("No source selected"), main_widget);
    selected_source_label_->setStyleSheet("font-weight: bold; font-size: 13px; color: #cfd6df;");

    btn_properties_ = new QPushButton(tr("⚙ Properties"), main_widget);
    btn_properties_->setObjectName("headerBtn");
    btn_filters_ = new QPushButton(tr("🎨 Filters"), main_widget);
    btn_filters_->setObjectName("headerBtn");

    header_layout->addWidget(selected_source_label_, 1);
    header_layout->addWidget(btn_properties_);
    header_layout->addWidget(btn_filters_);
    main_layout->addLayout(header_layout);

    // ========================================================================
    // 2. Central Content (Stacked: Scenes & Sources Lists)
    // ========================================================================
    stacked_content_ = new QStackedWidget(main_widget);

    // Scene List Page
    scene_list_widget_ = new QListWidget(stacked_content_);
    scene_list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(scene_list_widget_, &QListWidget::customContextMenuRequested,
            this, &CanvasControlDock::show_scene_context_menu);
    connect(scene_list_widget_, &QListWidget::itemSelectionChanged,
            this, &CanvasControlDock::on_scene_selection_changed);

    // Source List Page
    source_list_widget_ = new QListWidget(stacked_content_);
    source_list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(source_list_widget_, &QListWidget::customContextMenuRequested,
            this, &CanvasControlDock::show_source_context_menu);
    connect(source_list_widget_, &QListWidget::itemSelectionChanged,
            this, &CanvasControlDock::on_source_selection_changed);

    stacked_content_->addWidget(scene_list_widget_);  // Index 0: Scenes
    stacked_content_->addWidget(source_list_widget_); // Index 1: Sources
    stacked_content_->setCurrentIndex(1);            // Default show Sources
    main_layout->addWidget(stacked_content_, 1);

    // ========================================================================
    // 3. Middle Action Bar (+, Trash, Settings, Up, Down)
    // ========================================================================
    QHBoxLayout *action_bar_layout = new QHBoxLayout();
    action_bar_layout->setContentsMargins(0, 2, 0, 2);
    action_bar_layout->setSpacing(4);

    btn_add_ = new QPushButton("+", main_widget);
    btn_add_->setFixedSize(28, 24);
    btn_delete_ = new QPushButton("🗑", main_widget);
    btn_delete_->setFixedSize(28, 24);
    btn_item_props_ = new QPushButton("⚙", main_widget);
    btn_item_props_->setFixedSize(28, 24);
    btn_move_up_ = new QPushButton("▲", main_widget);
    btn_move_up_->setFixedSize(28, 24);
    btn_move_down_ = new QPushButton("▼", main_widget);
    btn_move_down_->setFixedSize(28, 24);

    action_bar_layout->addWidget(btn_add_);
    action_bar_layout->addWidget(btn_delete_);
    action_bar_layout->addWidget(btn_item_props_);
    action_bar_layout->addWidget(btn_move_up_);
    action_bar_layout->addWidget(btn_move_down_);
    action_bar_layout->addStretch(1);

    // Quick Record & Backtrack Action Buttons
    btn_record_ = new QPushButton(tr("⏺ REC"), main_widget);
    btn_record_->setObjectName("recBtn");
    btn_backtrack_ = new QPushButton(tr("⚡ CLIP"), main_widget);
    btn_backtrack_->setObjectName("clipBtn");

    action_bar_layout->addWidget(btn_record_);
    action_bar_layout->addWidget(btn_backtrack_);

    main_layout->addLayout(action_bar_layout);

    // ========================================================================
    // 4. Bottom Tab Bar ([Scenes] [Sources])
    // ========================================================================
    bottom_tab_bar_ = new QTabBar(main_widget);
    bottom_tab_bar_->addTab(tr("Scenes"));
    bottom_tab_bar_->addTab(tr("Sources"));
    bottom_tab_bar_->setCurrentIndex(1); // Default to Sources tab
    bottom_tab_bar_->setShape(QTabBar::RoundedSouth);

    connect(bottom_tab_bar_, &QTabBar::currentChanged, this, &CanvasControlDock::on_tab_changed);
    main_layout->addWidget(bottom_tab_bar_);

    setWidget(main_widget);

    // Connect button signals
    connect(btn_add_, &QPushButton::clicked, this, &CanvasControlDock::on_add_clicked);
    connect(btn_delete_, &QPushButton::clicked, this, &CanvasControlDock::on_delete_clicked);
    connect(btn_item_props_, &QPushButton::clicked, this, &CanvasControlDock::on_properties_clicked);
    connect(btn_properties_, &QPushButton::clicked, this, &CanvasControlDock::on_properties_clicked);
    connect(btn_filters_, &QPushButton::clicked, this, &CanvasControlDock::on_filters_clicked);
    connect(btn_move_up_, &QPushButton::clicked, this, &CanvasControlDock::on_move_up_clicked);
    connect(btn_move_down_, &QPushButton::clicked, this, &CanvasControlDock::on_move_down_clicked);
    connect(btn_record_, &QPushButton::clicked, this, &CanvasControlDock::on_record_toggled);
    connect(btn_backtrack_, &QPushButton::clicked, this, &CanvasControlDock::on_backtrack_clicked);
}

void CanvasControlDock::setup_stylesheet() {
    setStyleSheet(R"(
        QWidget {
            background-color: #1e2124;
            color: #d1d5db;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
        }
        QPushButton#headerBtn {
            background-color: #2b2f35;
            border: 1px solid #3e444e;
            border-radius: 4px;
            padding: 3px 8px;
            color: #d1d5db;
        }
        QPushButton#headerBtn:hover {
            background-color: #3b424c;
        }
        QPushButton#recBtn {
            background-color: #c53030;
            color: white;
            font-weight: bold;
            border-radius: 4px;
            padding: 2px 8px;
        }
        QPushButton#clipBtn {
            background-color: #2563eb;
            color: white;
            font-weight: bold;
            border-radius: 4px;
            padding: 2px 8px;
        }
        QListWidget {
            background-color: #181a1d;
            border: 1px solid #2d3239;
            border-radius: 4px;
            outline: none;
        }
        QListWidget::item {
            padding: 5px;
            border-bottom: 1px solid #23272d;
        }
        QListWidget::item:selected {
            background-color: #253347;
            color: #60a5fa;
        }
        QTabBar::tab {
            background: #25282e;
            color: #9ca3af;
            padding: 5px 14px;
            margin-right: 2px;
            border-bottom-left-radius: 4px;
            border-bottom-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background: #3b82f6;
            color: white;
            font-weight: bold;
        }
    )");
}

void CanvasControlDock::on_tab_changed(int index) {
    stacked_content_->setCurrentIndex(index);
    if (index == 0) {
        selected_source_label_->setText(tr("Scenes Management"));
        btn_properties_->setVisible(false);
        btn_filters_->setVisible(false);
    } else {
        on_source_selection_changed();
        btn_properties_->setVisible(true);
        btn_filters_->setVisible(true);
    }
}

void CanvasControlDock::refresh_scenes() {
    scene_list_widget_->clear();
    auto scenes = CanvasEngine::instance().get_all_scenes();
    for (auto *scene : scenes) {
        obs_source_t *src = obs_scene_get_source(scene);
        if (src) {
            const char *name = obs_source_get_name(src);
            QListWidgetItem *item = new QListWidgetItem(QString::fromUtf8(name), scene_list_widget_);
            if (scene == CanvasEngine::instance().current_scene()) {
                scene_list_widget_->setCurrentItem(item);
            }
        }
    }
}

static bool enum_scene_items_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
    Q_UNUSED(scene);
    QListWidget *list = static_cast<QListWidget *>(param);
    obs_source_t *src = obs_sceneitem_get_source(item);
    if (src) {
        const char *name = obs_source_get_name(src);
        bool visible = obs_sceneitem_visible(item);
        bool locked = obs_sceneitem_locked(item);

        QString status = QString("%1 %2  %3")
            .arg(visible ? "👁" : "🚫")
            .arg(locked ? "🔒" : "🔓")
            .arg(QString::fromUtf8(name));

        QListWidgetItem *list_item = new QListWidgetItem(status, list);
        list_item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void *>(item)));
    }
    return true;
}

void CanvasControlDock::refresh_sources() {
    source_list_widget_->clear();
    obs_scene_t *scene = CanvasEngine::instance().current_scene();
    if (!scene) {
        selected_source_label_->setText(tr("No active scene"));
        return;
    }

    obs_scene_enum_items(scene, enum_scene_items_cb, source_list_widget_);
    on_source_selection_changed();
}

void CanvasControlDock::on_scene_selection_changed() {
    QListWidgetItem *item = scene_list_widget_->currentItem();
    if (!item) return;

    std::string scene_name = item->text().toStdString();
    for (auto *scene : CanvasEngine::instance().get_all_scenes()) {
        obs_source_t *src = obs_scene_get_source(scene);
        if (src && scene_name == obs_source_get_name(src)) {
            CanvasEngine::instance().set_current_scene(scene);
            refresh_sources();
            break;
        }
    }
}

void CanvasControlDock::on_source_selection_changed() {
    QListWidgetItem *item = source_list_widget_->currentItem();
    if (!item) {
        selected_source_label_->setText(tr("No source selected"));
        return;
    }

    void *ptr = item->data(Qt::UserRole).value<void *>();
    obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
    if (sitem) {
        obs_source_t *src = obs_sceneitem_get_source(sitem);
        if (src) {
            selected_source_label_->setText(QString::fromUtf8(obs_source_get_name(src)));
            return;
        }
    }
    selected_source_label_->setText(tr("No source selected"));
}

void CanvasControlDock::on_add_clicked() {
    if (bottom_tab_bar_->currentIndex() == 0) {
        // Add Scene
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Add Scene"), tr("Scene Name:"),
                                             QLineEdit::Normal, tr("Vertical Scene"), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            CanvasEngine::instance().create_scene(name.trimmed().toStdString());
            refresh_scenes();
        }
    } else {
        // Add Source menu (including Clone from Canvas 1)
        QMenu add_menu(this);
        QMenu *clone_menu = add_menu.addMenu(tr("Clone from Canvas 1 (Main OBS)"));

        // Enumerate sources in OBS Main Scene
        obs_source_t *main_scene_src = obs_frontend_get_current_scene();
        if (main_scene_src) {
            obs_scene_t *main_scene = obs_scene_from_source(main_scene_src);
            if (main_scene) {
                struct EnumData {
                    QMenu *menu;
                    CanvasControlDock *dock;
                } data = {clone_menu, this};

                obs_scene_enum_items(main_scene, [](obs_scene_t *, obs_sceneitem_t *item, void *p) {
                    auto *d = static_cast<EnumData *>(p);
                    obs_source_t *src = obs_sceneitem_get_source(item);
                    if (src) {
                        const char *src_name = obs_source_get_name(src);
                        QAction *act = d->menu->addAction(QString::fromUtf8(src_name));
                        QObject::connect(act, &QAction::triggered, [d, src]() {
                            obs_scene_t *curr = CanvasEngine::instance().current_scene();
                            if (curr) {
                                obs_scene_add(curr, src);
                                d->dock->refresh_sources();
                            }
                        });
                    }
                    return true;
                }, &data);
            }
            obs_source_release(main_scene_src);
        }

        add_menu.exec(QCursor::pos());
    }
}

void CanvasControlDock::on_delete_clicked() {
    if (bottom_tab_bar_->currentIndex() == 0) {
        // Delete scene
        QListWidgetItem *item = scene_list_widget_->currentItem();
        if (!item) return;
        std::string scene_name = item->text().toStdString();
        for (auto *scene : CanvasEngine::instance().get_all_scenes()) {
            obs_source_t *src = obs_scene_get_source(scene);
            if (src && scene_name == obs_source_get_name(src)) {
                CanvasEngine::instance().remove_scene(scene);
                refresh_scenes();
                refresh_sources();
                break;
            }
        }
    } else {
        // Delete source
        QListWidgetItem *item = source_list_widget_->currentItem();
        if (!item) return;
        void *ptr = item->data(Qt::UserRole).value<void *>();
        obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
        if (sitem) {
            obs_sceneitem_remove(sitem);
            refresh_sources();
        }
    }
}

void CanvasControlDock::on_properties_clicked() {
    // Show properties dialog for source
    QListWidgetItem *item = source_list_widget_->currentItem();
    if (!item) return;
    void *ptr = item->data(Qt::UserRole).value<void *>();
    obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
    if (sitem) {
        obs_source_t *src = obs_sceneitem_get_source(sitem);
        if (src) {
            obs_frontend_open_source_properties(src);
        }
    }
}

void CanvasControlDock::on_filters_clicked() {
    // Show filters dialog for source
    QListWidgetItem *item = source_list_widget_->currentItem();
    if (!item) return;
    void *ptr = item->data(Qt::UserRole).value<void *>();
    obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
    if (sitem) {
        obs_source_t *src = obs_sceneitem_get_source(sitem);
        if (src) {
            obs_frontend_open_source_filters(src);
        }
    }
}

void CanvasControlDock::on_move_up_clicked() {
    QListWidgetItem *item = source_list_widget_->currentItem();
    if (!item) return;
    void *ptr = item->data(Qt::UserRole).value<void *>();
    obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
    if (sitem) {
        obs_sceneitem_set_order(sitem, OBS_ORDER_MOVE_UP);
        refresh_sources();
    }
}

void CanvasControlDock::on_move_down_clicked() {
    QListWidgetItem *item = source_list_widget_->currentItem();
    if (!item) return;
    void *ptr = item->data(Qt::UserRole).value<void *>();
    obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
    if (sitem) {
        obs_sceneitem_set_order(sitem, OBS_ORDER_MOVE_DOWN);
        refresh_sources();
    }
}

// ============================================================================
// Context Menu (Clone to Canvas 1 & Standard Actions)
// ============================================================================

void CanvasControlDock::show_source_context_menu(const QPoint &pos) {
    QListWidgetItem *item = source_list_widget_->itemAt(pos);
    if (!item) return;

    void *ptr = item->data(Qt::UserRole).value<void *>();
    obs_sceneitem_t *sitem = static_cast<obs_sceneitem_t *>(ptr);
    if (!sitem) return;
    obs_source_t *src = obs_sceneitem_get_source(sitem);
    if (!src) return;

    QMenu menu(this);

    // Visibility toggle
    bool is_vis = obs_sceneitem_visible(sitem);
    QAction *act_vis = menu.addAction(is_vis ? tr("Hide (Toggle Visibility)") : tr("Show (Toggle Visibility)"));
    connect(act_vis, &QAction::triggered, [sitem, is_vis, this]() {
        obs_sceneitem_set_visible(sitem, !is_vis);
        refresh_sources();
    });

    // Lock toggle
    bool is_lock = obs_sceneitem_locked(sitem);
    QAction *act_lock = menu.addAction(is_lock ? tr("Unlock") : tr("Lock"));
    connect(act_lock, &QAction::triggered, [sitem, is_lock, this]() {
        obs_sceneitem_set_locked(sitem, !is_lock);
        refresh_sources();
    });

    menu.addSeparator();

    // 🌟 KILLER FEATURE: "Clone to Canvas 1" -> [Submenu: All Main Scenes]
    QMenu *clone_to_c1_menu = menu.addMenu(tr("Clone to Canvas 1 (Main OBS)"));
    char **scene_names = obs_frontend_get_scene_names();
    if (scene_names) {
        for (char **name = scene_names; *name != nullptr; ++name) {
            QString sname = QString::fromUtf8(*name);
            QAction *act_clone = clone_to_c1_menu->addAction(sname);
            connect(act_clone, &QAction::triggered, [src, sname]() {
                CanvasEngine::instance().clone_source_to_main_scene(src, sname.toStdString());
            });
        }
        bfree(scene_names);
    }

    menu.addSeparator();
    QAction *act_props = menu.addAction(tr("Properties"));
    connect(act_props, &QAction::triggered, this, &CanvasControlDock::on_properties_clicked);

    QAction *act_filters = menu.addAction(tr("Filters"));
    connect(act_filters, &QAction::triggered, this, &CanvasControlDock::on_filters_clicked);

    menu.addSeparator();
    QAction *act_del = menu.addAction(tr("Remove"));
    connect(act_del, &QAction::triggered, this, &CanvasControlDock::on_delete_clicked);

    menu.exec(source_list_widget_->mapToGlobal(pos));
}

void CanvasControlDock::show_scene_context_menu(const QPoint &pos) {
    QListWidgetItem *item = scene_list_widget_->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *act_del = menu.addAction(tr("Remove Scene"));
    connect(act_del, &QAction::triggered, this, &CanvasControlDock::on_delete_clicked);
    menu.exec(scene_list_widget_->mapToGlobal(pos));
}

// ============================================================================
// Record & Backtrack Actions
// ============================================================================

void CanvasControlDock::on_record_toggled() {
    if (RecordEngine::instance().is_recording()) {
        RecordEngine::instance().stop_recording();
        btn_record_->setText(tr("⏺ REC"));
        btn_record_->setStyleSheet("background-color: #c53030; color: white;");
    } else {
        if (RecordEngine::instance().start_recording()) {
            btn_record_->setText(tr("⏹ STOP"));
            btn_record_->setStyleSheet("background-color: #10b981; color: white;");
        }
    }
}

void CanvasControlDock::on_backtrack_clicked() {
    // If not active yet, start buffer; if active, save clip
    if (!RecordEngine::instance().is_replay_buffer_active()) {
        RecordEngine::instance().start_replay_buffer(30);
        btn_backtrack_->setText(tr("💾 SAVE CLIP"));
    } else {
        RecordEngine::instance().save_replay_buffer();
    }
}


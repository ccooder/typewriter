/* typewriter-window.c
 *
 * Copyright 2025 King
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "typewriter-window.h"

#include <math.h>

#include "config.h"
#include "qq-group-item.h"
#include "qq-group-util.h"
#include "typewriter-input.h"
#include "typewriter-ui.h"

#if defined(__linux__)
#include "x11-util.h"
#include <gdk/x11/gdkx.h>
#endif

G_DEFINE_FINAL_TYPE(TypewriterWindow, typewriter_window,
                    GTK_TYPE_APPLICATION_WINDOW)

// GTK4在X11上notify::is-maximized不随WM外部改态触发，只能轮询diff写回；
// 在变化时刻即落盘，从而不依赖任何退出时机（尺寸同理，经绑定实时写回）
static gboolean poll_maximized(gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  gboolean maximized = gtk_window_is_maximized(GTK_WINDOW(self));
  if (maximized != g_settings_get_boolean(self->settings, "window-maximized")) {
    g_settings_set_boolean(self->settings, "window-maximized", maximized);
  }
  return G_SOURCE_CONTINUE;
}

#if defined(__linux__)
// GTK4移除窗口定位API，且映射前自设的USPosition提示会被GTK映射时的hints
// 重写覆盖——只能首次映射后经X11移到主显示器中央
static void center_window(TypewriterWindow *self) {
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(self));
  if (!GDK_IS_X11_DISPLAY(display)) return;
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(self));
  if (surface == NULL) return;

  GdkRectangle geom;
  gdk_monitor_get_geometry(gdk_x11_display_get_primary_monitor(display), &geom);
  // map时surface尚为1x1（首次configure未达），须用default_size计算
  gint w = 0, h = 0;
  gtk_window_get_default_size(GTK_WINDOW(self), &w, &h);
  XMoveWindow(GDK_DISPLAY_XDISPLAY(display), GDK_SURFACE_XID(surface),
              geom.x + (geom.width - w) / 2, geom.y + (geom.height - h) / 2);
}

static void on_first_map(GtkWidget *widget, gpointer user_data) {
  // 只在首次映射时居中，最小化还原不再移动
  g_signal_handlers_disconnect_by_func(widget, G_CALLBACK(on_first_map), NULL);
  center_window(TYPEWRITER_WINDOW(widget));
}
#endif

static void typewriter_window_dispose(GObject *object) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(object);

  if (self->update_timer_id > 0) {
    g_source_remove(self->update_timer_id);
    self->update_timer_id = 0;
  }
  if (self->maximized_poll_id > 0) {
    g_source_remove(self->maximized_poll_id);
    self->maximized_poll_id = 0;
  }
  g_clear_object(&self->settings);
  g_clear_object(&self->colors_provider);
  // 先解除ListView与模型的关联再释放store：若store先死，部件销毁时
  // gtk_list_item_manager还会查询模型，触发clear_model断言崩溃
  if (self->qq_group_list != NULL) {
    gtk_list_view_set_model(GTK_LIST_VIEW(self->qq_group_list), NULL);
    self->qq_group_list = NULL;  // 模板子部件交由模板销毁，二次dispose不再触碰
  }
  g_clear_object(&self->qq_group_list_store);
  // selected_group借用的是store内对象，store释放后置空
  self->selected_group = NULL;

  G_OBJECT_CLASS(typewriter_window_parent_class)->dispose(object);
}

static void clear_slow_items(TypewriterWindow *self) {
  while (self->slow_items != NULL) {
    TypewriterSlowItem *item = self->slow_items->data;
    g_free(item->text);
    g_free(item);
    self->slow_items = g_list_delete_link(self->slow_items, self->slow_items);
  }
}

static void typewriter_window_finalize(GObject *object) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(object);

  g_clear_pointer(&self->preedit_buffer, g_free);
  g_clear_pointer(&self->article_name, g_free);
  g_clear_pointer(&self->key_time_queue, g_queue_free);
  clear_slow_items(self);

  G_OBJECT_CLASS(typewriter_window_parent_class)->finalize(object);
}

static void typewriter_window_class_init(TypewriterWindowClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = typewriter_window_dispose;
  object_class->finalize = typewriter_window_finalize;

  gtk_widget_class_set_template_from_resource(
      widget_class, "/run/fenglu/typewriter/typewriter-window.ui");
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       qq_group_dropdown);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       qq_group_popover);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       qq_group_list);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       main_paned);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       control_scroll);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, control);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       follow_box);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, follow);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, timer);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, speed);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, stroke);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       code_len);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, words);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, info);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       mid_info);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       state_label);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       live_metrics);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       record_label);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow,
                                       progressbar);

  g_signal_new("TYPE_ENDED", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
               NULL, NULL, NULL, G_TYPE_NONE, 0);
}

// 当前指针的表面(窗口)坐标Y：窗口在拖拽中不动，作稳定参照系
static double mid_drag_cur_y(GtkGesture *gesture) {
  GdkEventSequence *seq =
      gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
  GdkEvent *event = gtk_gesture_get_last_event(gesture, seq);
  double x, y;
  gdk_event_get_position(event, &x, &y);
  (void)x;
  return y;
}

// 中间信息条拖拽把手：记录按下时的paned位置与指针表面Y。
// 偏移必须用表面坐标算——控件自身坐标会随set_position移动而漂移，
// 用GtkGestureDrag给的控件局部offset会形成"应用→坐标系平移→偏移回退
// →位置回跳"的锯齿闪烁
static void on_mid_drag_begin(GtkGestureDrag *gesture, double start_x,
                              double start_y, gpointer user_data) {
  (void)start_x;
  (void)start_y;
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  self->mid_drag_start_pos = gtk_paned_get_position(GTK_PANED(self->main_paned));
  self->mid_drag_begin_y = mid_drag_cur_y(GTK_GESTURE(gesture));
  self->mid_drag_last_off = 0.0;
  self->mid_drag_last_us = 0;
}

static void mid_drag_apply(TypewriterWindow *self, double offset_y) {
  gtk_paned_set_position(GTK_PANED(self->main_paned),
                         self->mid_drag_start_pos + (int)offset_y);
}

// 拖动超过3px抖动容忍才认领并调整比例，避免误吞慢字词指标等点击；
// 认领后松手不会再触发label上的点击手势。60ms节流：真实鼠标高频motion
// 会让每次重排都全量重绘两个文本区（松手补齐最终位置）
static void on_mid_drag_update(GtkGestureDrag *gesture, double offset_x,
                               double offset_y, gpointer user_data) {
  (void)offset_x;
  (void)offset_y;
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  double off = mid_drag_cur_y(GTK_GESTURE(gesture)) - self->mid_drag_begin_y;
  self->mid_drag_last_off = off;
  if (fabs(off) < 3) {
    return;
  }
  gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  gint64 now = g_get_monotonic_time();
  if (now - self->mid_drag_last_us >= 60000) {
    self->mid_drag_last_us = now;
    mid_drag_apply(self, off);
  }
}

// 松手时无条件应用最终偏移（节流期间丢弃的中间值不影响终值）
static void on_mid_drag_end(GtkGestureDrag *gesture, double offset_x,
                            double offset_y, gpointer user_data) {
  (void)gesture;
  (void)offset_x;
  (void)offset_y;
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  mid_drag_apply(self, self->mid_drag_last_off);
}

// 慢字词详情弹层关闭时解除挂靠并释放（引用已交由父部件持有）
static void on_slow_popover_closed(GtkPopover *pop, gpointer user_data) {
  (void)user_data;
  gtk_widget_unparent(GTK_WIDGET(pop));
}

// 结束后点击慢字词指标弹出详情：逐条显示慢的字词与耗时
static void on_slow_detail_clicked(GtkGestureClick *gesture, int n_press,
                                   double x, double y, gpointer user_data) {
  (void)gesture;
  (void)n_press;
  (void)x;
  (void)y;
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->state != TYPEWRITER_STATE_ENDED || self->slow_items == NULL) {
    return;
  }
  GString *detail = g_string_new("");
  for (GList *l = self->slow_items; l != NULL; l = l->next) {
    TypewriterSlowItem *item = l->data;
    g_string_append_printf(detail, "「%s」  %.1f 秒\n", item->text,
                           item->seconds);
  }
  GtkWidget *pop = gtk_popover_new();
  GtkWidget *sw = gtk_scrolled_window_new();
  gtk_widget_set_size_request(sw, 280, 200);
  GtkWidget *label = gtk_label_new(detail->str);
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(label), 0);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), label);
  gtk_popover_set_child(GTK_POPOVER(pop), sw);
  g_signal_connect(pop, "closed", G_CALLBACK(on_slow_popover_closed), NULL);
  // 部件构造为浮动引用，set_parent沉掉并接管；closed时unparent即销毁
  gtk_widget_set_parent(pop, self->record_label);
  gtk_popover_popup(GTK_POPOVER(pop));
  g_string_free(detail, TRUE);
}

static void typewriter_window_init(TypewriterWindow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  load_css_providers(self);

  gtk_widget_set_size_request(GTK_WIDGET(self->main_paned), 800, 400);
  gtk_paned_set_resize_start_child(GTK_PANED(self->main_paned), TRUE);
  gtk_paned_set_shrink_start_child(GTK_PANED(self->main_paned), FALSE);
  gtk_paned_set_resize_end_child(GTK_PANED(self->main_paned), FALSE);
  gtk_paned_set_shrink_end_child(GTK_PANED(self->main_paned), FALSE);

  gtk_paned_set_start_child(GTK_PANED(self->main_paned), self->control_scroll);
  gtk_paned_set_end_child(GTK_PANED(self->main_paned), self->follow_box);
  // 宽把手：细把手时GTK在分隔线四周外扩6px隐形拖拽热区，且paned的拖拽手势在
  // capture阶段认领事件——紧邻分隔线下方的中间信息条上半部分点击会被吃掉。
  // 宽把手的热区=可视分隔线本身，不再外扩；细线外观由style.css还原
  // gtk_paned_set_wide_handle(GTK_PANED(self->main_paned), TRUE);
  // 设置最小窗口大小
  gtk_widget_set_size_request(GTK_WIDGET(self->control_scroll), -1, 100);
  gtk_widget_set_size_request(GTK_WIDGET(self->follow_box), -1, 100);

  // 中间信息条整体作为拖拽把手：上下拖动调整对照区/跟打区比例；
  // 静止点击不受影响（真实拖动才认领，见on_mid_drag_update）
  GtkGesture *mid_drag = gtk_gesture_drag_new();
  g_signal_connect(mid_drag, "drag-begin", G_CALLBACK(on_mid_drag_begin), self);
  g_signal_connect(mid_drag, "drag-update", G_CALLBACK(on_mid_drag_update),
                   self);
  g_signal_connect(mid_drag, "drag-end", G_CALLBACK(on_mid_drag_end), self);
  gtk_widget_add_controller(self->mid_info, GTK_EVENT_CONTROLLER(mid_drag));
  gtk_widget_set_cursor_from_name(self->mid_info, "row-resize");

  // 窗口几何持久化：schema未安装（如buildDir直接运行）时静默关闭该功能。
  // GTK4已移除窗口定位API，位置由窗口管理器负责，此处只持久化尺寸与最大化
  GSettingsSchema *schema = g_settings_schema_source_lookup(
      g_settings_schema_source_get_default(), "run.fenglu.typewriter", FALSE);
  if (schema != NULL) {
    self->settings = g_settings_new("run.fenglu.typewriter");
    g_settings_schema_unref(schema);
    // 尺寸经default-width/height双向绑定，拖动即写回
    g_settings_bind(self->settings, "window-width", self, "default-width",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "window-height", self, "default-height",
                    G_SETTINGS_BIND_DEFAULT);
    // 最大化轮询写回（500ms粒度，diff才写）
    self->maximized_poll_id = g_timeout_add(500, poll_maximized, self);
    if (g_settings_get_boolean(self->settings, "window-maximized")) {
      gtk_window_maximize(GTK_WINDOW(self));
    }
  }

  // 初始化状态变量
  self->state = TYPEWRITER_STATE_READY;
  self->stats.start_time = 0;
  self->stats.end_time = 0;
  self->stats.pause_start_time = 0;
  self->stats.pause_duration = 0;
  self->stats.stroke_count = 0;
  self->stats.text_length = 0;
  self->stats.correct_char_count = 0;
  self->stats.total_char_count = 0;
  self->stats.type_char_count = 0;
  self->stats.type_word_count = 0;
  self->stats.backspace_count = 0;
  self->stats.enter_count = 0;
  self->stats.reform_count = 0;
  self->update_timer_id = 0;
  self->maximized_poll_id = 0;
  self->slow_items = NULL;
  self->last_commit_elapsed = 0;
  self->max_queue_size = 16;  // 存储最近16次击键时间
  self->key_time_queue = g_queue_new();
  self->qq_group_list_store = g_list_store_new(QQ_GROUP_TYPE_ITEM);

  // 对照区上色tag只创建一次，复用避免tag table无限膨胀
  GtkTextBuffer *control_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->control));
  self->correct_tag = gtk_text_buffer_create_tag(
      control_buffer, "correct-tag", "background", "#108144", NULL);
  self->incorrect_tag = gtk_text_buffer_create_tag(
      control_buffer, "incorrect-tag", "background", "red", "foreground",
      "white", NULL);

  // Create the factory and connect the setup/bind signals
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_cb), NULL);
  // Add some items to the list store
  QQGroupItem *item = qq_group_item_new(0, "潜水", TRUE);
  self->selected_group = item;
  g_list_store_append(self->qq_group_list_store, item);
  GtkSingleSelection *selection_model =
      gtk_single_selection_new(G_LIST_MODEL(self->qq_group_list_store));
  gtk_list_view_set_model(GTK_LIST_VIEW(self->qq_group_list),
                          GTK_SELECTION_MODEL(selection_model));

  // Create the ListView and set its model and factory
  gtk_list_view_set_factory(GTK_LIST_VIEW(self->qq_group_list), factory);

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->follow));
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(buffer, &end_iter);
  GtkTextMark *end_mark = gtk_text_mark_new("end", FALSE);

  gtk_text_buffer_add_mark(buffer, end_mark, &end_iter);

  GtkEventController *keyboard_controller = gtk_event_controller_key_new();
  g_signal_connect(keyboard_controller, "key-pressed", G_CALLBACK(on_key_press),
                   self);
  gtk_widget_add_controller(GTK_WIDGET(self->follow), keyboard_controller);
  GtkTextBuffer *follow_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->follow));
  g_signal_connect(follow_buffer, "changed",
                   G_CALLBACK(on_follow_buffer_changed), self);
  // 增量比对：中途编辑时把比对边界退回到编辑点
  g_signal_connect(follow_buffer, "insert-text",
                   G_CALLBACK(on_follow_insert_text), self);
  g_signal_connect(follow_buffer, "delete-range",
                   G_CALLBACK(on_follow_delete_range), self);
  g_signal_connect(self->follow, "preedit-changed",
                   G_CALLBACK(on_preedit_changed), self);
  GtkEventController *focus_controller = gtk_event_controller_focus_new();
  gtk_widget_add_controller(GTK_WIDGET(self), focus_controller);
  g_signal_connect(focus_controller, "enter", G_CALLBACK(on_window_focus_enter),
                   self);
  g_signal_connect(focus_controller, "leave", G_CALLBACK(on_window_focus_leave),
                   self);
  g_signal_connect(self, "TYPE_ENDED", G_CALLBACK(on_type_ended), NULL);
  // 慢字词指标点击弹详情（仅结束后生效）
  GtkGesture *slow_click = gtk_gesture_click_new();
  g_signal_connect(slow_click, "released", G_CALLBACK(on_slow_detail_clicked),
                   self);
  gtk_widget_add_controller(self->record_label,
                            GTK_EVENT_CONTROLLER(slow_click));
  g_signal_connect(self->qq_group_dropdown, "clicked",
                   G_CALLBACK(on_qq_group_dropdown_clicked), self);
  g_signal_connect(self->qq_group_popover, "closed",
                   G_CALLBACK(on_qq_group_popover_closed), self);
  g_signal_connect(self->qq_group_list, "activate",
                   G_CALLBACK(on_qq_group_selected), self);
  // g_signal_connect_after(self->qq_group_dropdown, "notify::selected",
  // G_CALLBACK(on_qq_group_activate), NULL);

#if defined(__linux__)
  g_signal_connect(self, "map", G_CALLBACK(on_first_map), NULL);
#endif
}

TypewriterWindow *typewriter_window_new(TypewriterApplication *app) {
  return g_object_new(TYPEWRITER_TYPE_WINDOW, "application", app, NULL);
}

void typewriter_window_open(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
  char *welcome =
      "欢迎您使用牛逢路的Linux版跟打器，快捷键如下：F3重打，F5从QQ群载文，Alt+"
      "E从剪贴板载文，F6从本地文件载文，Ctrl+Q退出。";

  g_free(win->article_name);
  win->article_name = g_strdup("欢迎语");
  glong welcome_length = g_utf8_strlen(welcome, -1);
  win->stats.text_length = welcome_length;
  label_set_printf(GTK_LABEL(win->words), "共%ld字", welcome_length);
  update_mid_info(win);

  gtk_text_buffer_set_text(buffer, welcome, -1);
  gtk_window_set_focus(GTK_WINDOW(win), GTK_WIDGET(win->follow));
}

static void on_window_focus_enter(GtkEventControllerFocus *self,
                                  gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  GtkTextView *follow = GTK_TEXT_VIEW(win->follow);
  gtk_widget_set_visible(GTK_WIDGET(follow), TRUE);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(follow);
  if (buffer != NULL) {
    GtkTextIter end_iter;

    // Get an iterator pointing to the end of the buffer
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
    gtk_text_buffer_place_cursor(buffer, &end_iter);
    GtkTextMark *end_mark = gtk_text_buffer_get_mark(buffer, "end");
    gtk_text_buffer_move_mark(buffer, end_mark, &end_iter);

    GtkTextIter markIter;
    gtk_text_buffer_get_iter_at_mark(buffer, &markIter, end_mark);
    int offset = gtk_text_iter_get_offset(&markIter);

    gtk_text_view_scroll_mark_onscreen(follow, end_mark);
  }
  gtk_widget_grab_focus(win->follow);
}

static void on_window_focus_leave(GtkEventControllerFocus *self,
                                  gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  if (win->state == TYPEWRITER_STATE_TYPING) {
    typewriter_pause(win);
  }
}

// ###############################
// 编写异步临时方法
// ###############################
static void calculation_done_cb(GObject *source_object, GAsyncResult *res,
                                gpointer user_data) {
  g_print("cal finished");
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  GtkTextBuffer *follow_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->follow));
  gtk_text_buffer_set_text(follow_buffer, "Calculation finished", -1);
}

static void long_calculation_thread(GTask *task, gpointer source_object,
                                    gpointer task_data,
                                    GCancellable *cancellable) {
  g_print("source_object: %d\n", source_object == NULL);
  g_print("task_data: %d\n", task_data == NULL);
  g_print("Worker thread: Starting heavy calculation...\n");

  // Simulate a long calculation.
  sleep(3);

  // Create a pointer for the result. The GTask will take ownership.
  gint *result = g_new(gint, 1);
  *result = 42;

  g_print("Worker thread: Calculation finished. Returning result...\n");

  // Return the result to the main thread.
  // g_free will be called on the pointer when the task is destroyed.
  g_task_return_pointer(task, result, g_free);
}

static void start_calculation_cb(gpointer user_data) {
  GTask *task;

  // Update UI to give feedback and prevent double-clicks.

  // Create a new GTask We pass our widgets and the completion callback.
  task = g_task_new(user_data, NULL, calculation_done_cb, user_data);

  // Run our worker function in a background thread from GLib's thread pool.
  // [1.4.1]
  g_task_run_in_thread(task, long_calculation_thread);

  // We can now release our reference to the task. The thread owns it now.
  g_object_unref(task);
}

// 成绩单里的输入法名：用户设置优先；留空则按环境变量探测框架
// （框架内的具体方案如98五笔无法探测，需用户自行填写）
static gchar *get_ime_label(TypewriterWindow *win) {
  if (win->settings != NULL) {
    gchar *custom = g_settings_get_string(win->settings, "ime-name");
    if (custom != NULL && *custom != '\0') {
      return custom;
    }
    g_free(custom);
  }
  const char *module = g_getenv("GTK_IM_MODULE");
  if (module == NULL || *module == '\0') {
    // XMODIFIERS形如@im=fcitx
    const char *xmodifiers = g_getenv("XMODIFIERS");
    if (xmodifiers != NULL && g_str_has_prefix(xmodifiers, "@im=")) {
      module = xmodifiers + 4;
    }
  }
  if (g_strcmp0(module, "fcitx") == 0) return g_strdup("Fcitx");
  if (g_strcmp0(module, "fcitx5") == 0) return g_strdup("Fcitx5");
  if (g_strcmp0(module, "ibus") == 0) return g_strdup("iBus");
  return g_strdup("系统输入法");
}

static void on_type_ended(TypewriterWindow *win, gpointer user_data) {
  // 空文章结束时可能从未启动过计时器
  if (win->update_timer_id > 0) {
    g_source_remove(win->update_timer_id);
    win->update_timer_id = 0;
  }
  win->stats.end_time = g_get_monotonic_time();

  gchar *ime = get_ime_label(win);
  TypewriterGrade g = typewriter_build_grade(win->article_name, &win->stats, ime);
  g_free(ime);

  label_set_printf(GTK_LABEL(win->speed), "%.2f", g.speed);
  label_set_printf(GTK_LABEL(win->stroke), "%.2f", g.stroke);
  label_set_printf(GTK_LABEL(win->code_len), "%.2f", g.code_len);
  label_set_printf(GTK_LABEL(win->timer), "%02u:%02u.%03u", g.minutes,
                   g.seconds, g.milliseconds);
  update_mid_info(win);
  if (win->slow_items != NULL) {
    gtk_widget_set_tooltip_text(win->record_label, "点击查看慢字词详情");
  }

  send_to_qq_group(win, g.grade);

  g_free(g.grade);
}

static void load_css_providers(TypewriterWindow *self) {
  GdkDisplay *display;

  display = gtk_widget_get_display(GTK_WIDGET(self));

  self->colors_provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(self->colors_provider,
                                      "/run/fenglu/typewriter/style.css");
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(self->colors_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
}

void typewriter_pause(TypewriterWindow *self) {
  g_assert(TYPEWRITER_IS_WINDOW(self));
  self->state = TYPEWRITER_STATE_PAUSING;
  // 停止打字计时器
  if (self->update_timer_id > 0) {
    g_source_remove(self->update_timer_id);
    self->update_timer_id = 0;
  }
  // 记录暂停开始时间
  self->stats.pause_start_time = g_get_monotonic_time();
  update_mid_info(self);
}

void typewriter_window_retype(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));

  // 停止计时器
  if (win->update_timer_id > 0) {
    g_source_remove(win->update_timer_id);
    win->update_timer_id = 0;
  }

  // 清空击键时间队列与慢字词记录
  g_queue_clear(win->key_time_queue);
  clear_slow_items(win);
  win->last_commit_elapsed = 0;
  gtk_widget_set_tooltip_text(win->record_label, NULL);

  // 重置计数器
  win->state = TYPEWRITER_STATE_RETYPE_READY;
  win->stats.start_time = 0;
  win->stats.end_time = 0;
  win->stats.pause_start_time = 0;
  win->stats.pause_duration = 0;
  win->stats.stroke_count = 0;
  win->stats.correct_char_count = 0;
  win->stats.total_char_count = 0;
  win->stats.type_char_count = 0;
  win->stats.type_word_count = 0;
  win->stats.word_char_count = 0;
  win->stats.backspace_count = 0;
  win->stats.enter_count = 0;
  win->stats.reform_count = 0;
  win->update_timer_id = 0;

  // 重置UI显示
  gtk_label_set_text(GTK_LABEL(win->timer), "00:00.000");
  gtk_label_set_text(GTK_LABEL(win->speed), "0.0");
  gtk_label_set_text(GTK_LABEL(win->stroke), "0.0");
  gtk_label_set_text(GTK_LABEL(win->code_len), "0.0");
  update_mid_info(win);

  // 清空跟打区
  GtkTextBuffer *follow_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->follow));
  gtk_text_buffer_set_text(follow_buffer, "", -1);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(win->follow), TRUE);

  // 移除对照区的所有标记
  GtkTextBuffer *control_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(control_buffer, &start);
  gtk_text_buffer_get_end_iter(control_buffer, &end);
  gtk_text_buffer_remove_all_tags(control_buffer, &start, &end);
  gtk_widget_grab_focus(win->follow);

  GtkTextMark *control_start_mark =
      gtk_text_buffer_create_mark(control_buffer, NULL, &start, TRUE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(win->control), control_start_mark,
                               0.1, TRUE, 0, 0);
  // gtk_text_view_backward_display_line_start(GTK_TEXT_VIEW(win->control),
  // &start);
  gtk_text_buffer_delete_mark(control_buffer, control_start_mark);
}
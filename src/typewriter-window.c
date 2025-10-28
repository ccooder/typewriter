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

#include "config.h"
#include "qq-group-item.h"
#include "qq-group-util.h"
#include "typewriter-input.h"
#include "typewriter-ui.h"
#include "x11-util.h"

G_DEFINE_FINAL_TYPE(TypewriterWindow, typewriter_window,
                    GTK_TYPE_APPLICATION_WINDOW)

static void typewriter_window_class_init(TypewriterWindowClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

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
                                       progressbar);

  g_signal_new("TYPE_ENDED", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
               NULL, NULL, NULL, G_TYPE_NONE, 0);
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
  // 设置最小窗口大小
  gtk_widget_set_size_request(GTK_WIDGET(self->control_scroll), -1, 100);
  gtk_widget_set_size_request(GTK_WIDGET(self->follow_box), -1, 100);

  // GdkCursor *move_cursor = gdk_cursor_new_from_name("row-resize", NULL);
  //
  // // Set the cursor on the button
  // gtk_widget_set_cursor(self->mid_info, move_cursor);
  // g_object_unref(move_cursor);

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
  self->max_queue_size = 16;  // 存储最近16次击键时间
  self->key_time_queue = g_queue_new();
  self->qq_group_list_store = g_list_store_new(QQ_GROUP_TYPE_ITEM);

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
  g_signal_connect(self->follow, "preedit-changed",
                   G_CALLBACK(on_preedit_changed), self);
  GtkEventController *focus_controller = gtk_event_controller_focus_new();
  gtk_widget_add_controller(GTK_WIDGET(self), focus_controller);
  g_signal_connect(focus_controller, "enter", G_CALLBACK(on_window_focus_enter),
                   self);
  g_signal_connect(focus_controller, "leave", G_CALLBACK(on_window_focus_leave),
                   self);
  g_signal_connect(self, "TYPE_ENDED", G_CALLBACK(on_type_ended), NULL);
  g_signal_connect(self->qq_group_dropdown, "clicked",
                   G_CALLBACK(on_qq_group_dropdown_clicked), self);
  g_signal_connect(self->qq_group_popover, "closed",
                   G_CALLBACK(on_qq_group_popover_closed), self);
  g_signal_connect(self->qq_group_list, "activate",
                   G_CALLBACK(on_qq_group_selected), self);
  // g_signal_connect_after(self->qq_group_dropdown, "notify::selected",
  // G_CALLBACK(on_qq_group_activate), NULL);
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

  win->article_name = "欢迎语";
  glong welcome_length = g_utf8_strlen(welcome, -1);
  win->stats.text_length = welcome_length;
  gtk_label_set_text(GTK_LABEL(win->words),
                     g_strdup_printf("共%ld字", welcome_length));

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

static void on_type_ended(TypewriterWindow *win, gpointer user_data) {
  g_source_remove(win->update_timer_id);
  win->update_timer_id = 0;
  gint64 current_time = g_get_monotonic_time();
  gint64 elapsed_time_ms =
      (current_time - win->stats.start_time - win->stats.pause_duration) /
      1000.0;

  // 计算平均指标
  double overall_typing_speed = 0.0;
  if (elapsed_time_ms > 0 && win->stats.total_char_count > 0) {
    // 转换为分钟并计算每分钟字数
    overall_typing_speed =
        (win->stats.total_char_count * 60000.0) / elapsed_time_ms;
    gtk_label_set_text(GTK_LABEL(win->speed),
                       g_strdup_printf("%.2f", overall_typing_speed));
  }

  // 显示击键与码长信息
  double stroke = (double)win->stats.stroke_count * 1000.0 / elapsed_time_ms;
  gtk_label_set_text(GTK_LABEL(win->stroke), g_strdup_printf("%.2f", stroke));
  g_strdup_printf("%d", win->stats.total_char_count);
  double avg_code_len =
      (double)win->stats.stroke_count / win->stats.total_char_count;
  gtk_label_set_text(GTK_LABEL(win->code_len),
                     g_strdup_printf("%.2f", avg_code_len));

  // 显示用时
  guint seconds = elapsed_time_ms / 1000;
  guint minutes = seconds / 60;
  seconds = seconds % 60;
  guint milliseconds = elapsed_time_ms % 1000;

  gtk_label_set_text(
      GTK_LABEL(win->timer),
      g_strdup_printf("%02u:%02u.%03u", minutes, seconds, milliseconds));

  char *grade = nullptr;
  grade = g_malloc(1024);

  // 打印成绩
  sprintf(
      grade,
      "%s 速度%.2f 击键%.2f 码长%.2f 字数%d 错字%d 时间%02u:%02u.%03u 回改%d "
      "退格%d 回车%d 键数%d 打词%.2f%% 输入法:Rime·98五笔 NFLinux跟打器\n",
      win->article_name, overall_typing_speed, stroke, avg_code_len,
      win->stats.total_char_count,
      win->stats.total_char_count - win->stats.correct_char_count, minutes,
      seconds, milliseconds, win->stats.reform_count,
      win->stats.backspace_count, win->stats.enter_count,
      win->stats.stroke_count,
      win->stats.type_word_count * 100.0 /
          (win->stats.type_char_count + win->stats.type_word_count));

  send_to_qq_group(win, grade);
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
  g_source_remove(self->update_timer_id);
  self->update_timer_id = 0;
  // 记录暂停开始时间
  self->stats.pause_start_time = g_get_monotonic_time();
}

void typewriter_window_retype(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));

  // 停止计时器
  if (win->update_timer_id > 0) {
    g_source_remove(win->update_timer_id);
    win->update_timer_id = 0;
  }

  // 清空击键时间队列
  g_queue_clear(win->key_time_queue);

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
  win->stats.backspace_count = 0;
  win->stats.enter_count = 0;
  win->stats.reform_count = 0;
  win->update_timer_id = 0;

  // 重置UI显示
  gtk_label_set_text(GTK_LABEL(win->timer), "00:00.000");
  gtk_label_set_text(GTK_LABEL(win->speed), "0.0");
  gtk_label_set_text(GTK_LABEL(win->stroke), "0.0");
  gtk_label_set_text(GTK_LABEL(win->code_len), "0.0");

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
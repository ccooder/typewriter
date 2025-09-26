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

enum TypeFlag {
  RETYPE_READY = 0,
  READY = 1,
  TYPING = 2,
  PAUSING = 3,
  ENDED = 4
};

struct _TypewriterWindow {
  GtkApplicationWindow parent_instance;
  GtkCssProvider *colors_provider;

  /* Template widgets */
  // 对照区
  GtkWidget *control;
  // 跟打区
  GtkWidget *follow;
  // 顶部状态区
  // 用时
  GtkWidget *timer;
  // 速度
  GtkWidget *speed;
  // 击键
  GtkWidget *stroke;
  // 总字数
  GtkWidget *words;

  // 顶部状态区
  GtkWidget *info;

  // preedit buffer
  gchar *preedit_buffer;

  // 开始时间
  gint flag;
  guint64 start_time;
  guint stroke_count;
  guint correct_char_count;
  guint total_char_count;
  guint update_timer_id;

  // 实时击键速度
  GQueue *key_time_queue;
  guint max_queue_size;
};

G_DEFINE_FINAL_TYPE(TypewriterWindow, typewriter_window,
                    GTK_TYPE_APPLICATION_WINDOW)

static void typewriter_window_class_init(TypewriterWindowClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  gtk_widget_class_set_template_from_resource(
      widget_class, "/run/fenglu/typewriter/typewriter-window.ui");
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, control);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, follow);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, timer);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, speed);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, stroke);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, words);
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, info);
}

static void typewriter_window_init(TypewriterWindow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  load_css_providers(self);

  // 初始化状态变量
  self->stroke_count = 0;
  self->flag = READY;
  self->correct_char_count = 0;
  self->start_time = 0;
  self->update_timer_id = 0;
  self->max_queue_size = 10;  // 存储最近10次击键时间
  self->key_time_queue = g_queue_new();

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
}

TypewriterWindow *typewriter_window_new(TypewriterApplication *app) {
  return g_object_new(TYPEWRITER_TYPE_WINDOW, "application", app, NULL);
}

void typewriter_window_open(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
  char *welcome =
      "欢迎您使用牛逢路的Linux版跟打器，快捷键如下：F3重打，Alt+"
      "E从剪贴板载文，F6从本地文件载文，Ctrl+Q退出。";

  glong welcome_length = g_utf8_strlen(welcome, -1);
  gtk_label_set_text(GTK_LABEL(win->words),
                     g_strdup_printf("共%ld字", welcome_length));

  gtk_text_buffer_set_text(buffer, welcome, -1);
}

static gboolean on_key_press(GtkEventControllerKey *controller, guint keyval,
                             guint keycode, GdkModifierType state,
                             gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->flag == RETYPE_READY) {
    self->flag = READY;
  }
  if ((self->preedit_buffer == NULL || strlen(self->preedit_buffer) <= 0) &&
      (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
    // Return TRUE to indicate that the event has been handled and
    // should not be propagated further (preventing default newline insertion)
    if (self->flag == TYPING) {
      self->flag = PAUSING;
      g_source_remove(self->update_timer_id);
      self->update_timer_id = 0;
    }
    return TRUE;
  }

  if (self->flag == PAUSING) {
    self->flag = TYPING;
    self->update_timer_id = g_timeout_add(100, update_stat_ui, self);
  }

  if (self->flag == TYPING) {
    self->stroke_count++;
    guint64 current_time = g_get_monotonic_time();
    g_queue_push_tail(self->key_time_queue, GSIZE_TO_POINTER(current_time));

    // 保持队列大小不超过max_queue_size
    if (g_queue_get_length(self->key_time_queue) > self->max_queue_size) {
      g_queue_pop_head(self->key_time_queue);
    }
  }

  if (self->preedit_buffer != NULL) {
    g_free(self->preedit_buffer);
    self->preedit_buffer = NULL;
  }
  // For other keys, return FALSE to allow default processing
  return FALSE;
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

  // Create a new GTask. We pass our widgets and the completion callback.
  task = g_task_new(user_data, NULL, calculation_done_cb, user_data);

  // Run our worker function in a background thread from GLib's thread pool.
  // [1.4.1]
  g_task_run_in_thread(task, long_calculation_thread);

  // We can now release our reference to the task. The thread owns it now.
  g_object_unref(task);
}

static void on_follow_buffer_changed(GtkTextBuffer *follow_buffer,
                                     gpointer user_data) {
  // start_calculation_cb(user_data);

  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->flag == READY) {
    self->flag = TYPING;
    self->start_time = g_get_monotonic_time();
    self->update_timer_id = g_timeout_add(100, update_stat_ui, self);
  }

  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(follow_buffer, &start);
  gtk_text_buffer_get_end_iter(follow_buffer, &end);
  gchar *follow_text =
      gtk_text_buffer_get_text(follow_buffer, &start, &end, FALSE);

  GtkTextView *control = GTK_TEXT_VIEW(self->control);
  GtkTextBuffer *control_buffer = gtk_text_view_get_buffer(control);
  gtk_text_buffer_get_start_iter(control_buffer, &start);
  gtk_text_buffer_get_end_iter(control_buffer, &end);
  gchar *control_text =
      gtk_text_buffer_get_text(control_buffer, &start, &end, FALSE);

  gtk_text_buffer_remove_all_tags(control_buffer, &start, &end);

  GtkTextTag *correct_tag = gtk_text_buffer_create_tag(
      control_buffer, NULL, "background", "green", NULL);
  GtkTextTag *incorrect_tag = gtk_text_buffer_create_tag(
      control_buffer, NULL, "background", "red", NULL);

  GtkTextIter char_iter;
  gtk_text_buffer_get_start_iter(follow_buffer, &char_iter);
  for (int i = 0;
       !gtk_text_iter_is_end(&char_iter) && control_text[i] != '\0';) {
    GtkTextIter start_iter = char_iter;
    gtk_text_iter_forward_char(&char_iter);

    // Get the character from the typed buffer and reference text
    char *typed_char =
        gtk_text_buffer_get_text(follow_buffer, &start_iter, &char_iter, FALSE);
    gunichar typed_unichar = g_utf8_get_char(typed_char);
    gunichar ref_unichar = g_utf8_get_char(&control_text[i]);

    // Compare and apply the correct tag
    int next = i + g_unichar_to_utf8(ref_unichar, NULL);
    GtkTextIter control_start_iter, control_end_iter;
    gtk_text_buffer_get_iter_at_offset(control_buffer, &control_start_iter,
                                       gtk_text_iter_get_offset(&start_iter));
    gtk_text_buffer_get_iter_at_offset(control_buffer, &control_end_iter,
                                       gtk_text_iter_get_offset(&char_iter));
    if (typed_unichar == ref_unichar) {
      gtk_text_buffer_apply_tag(control_buffer, correct_tag,
                                &control_start_iter, &control_end_iter);
    } else {
      gtk_text_buffer_apply_tag(control_buffer, incorrect_tag,
                                &control_start_iter, &control_end_iter);
    }

    // Move to the next character in the reference text
    i += g_unichar_to_utf8(ref_unichar, NULL);
    g_free(typed_char);
  }
  // Handle cases where one string is longer than the other
  // ... apply tags for remaining characters if needed
  g_free(follow_text);
  g_free(control_text);
}

static void on_preedit_changed(GtkTextView *self, gchar *preedit,
                               gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  win->preedit_buffer = g_malloc(sizeof(char) * (strlen(preedit) + 1));
  strcpy(win->preedit_buffer, preedit);
}

static void load_css_providers(TypewriterWindow *self) {
  GdkDisplay *display;

  display = gtk_widget_get_display(GTK_WIDGET(self));

  /* Calendar olors */
  self->colors_provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(self->colors_provider,
                                      "/run/fenglu/typewriter/style.css");
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(self->colors_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
}

static void load_clipboard_text(GdkClipboard *clipboard, GAsyncResult *result,
                                gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  GError *error = NULL;

  // 读取文本内容
  char *text = gdk_clipboard_read_text_finish(clipboard, result, &error);

  if (error != NULL) {
    g_printerr("Error reading clipboard: %s\n", error->message);
    g_error_free(error);
    return;
  }

  if (text != NULL) {
    // 成功获取到文本，可以在这里进行处理
    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
    gtk_text_buffer_set_text(buffer, text, -1);
    gtk_label_set_text(GTK_LABEL(win->info), "来自剪贴板");
    // 计算文本长度

    int text_length = g_utf8_strlen(text, -1);
    // 更新总字数标签
    gtk_label_set_text(GTK_LABEL(win->words),
                       g_strdup_printf("共%d字", text_length));

    // 不要忘记释放文本资源
    g_free(text);
  }
}

void load_file(TypewriterWindow *win) { g_assert(TYPEWRITER_IS_WINDOW(win)); }

void load_clipboard(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));

  GdkDisplay *display;

  display = gtk_widget_get_display(GTK_WIDGET(win));
  GdkClipboard *clipboard = gdk_display_get_clipboard(display);
  gdk_clipboard_read_text_async(clipboard, NULL,
                                (GAsyncReadyCallback)load_clipboard_text, win);
}

static gboolean update_stat_ui(gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);

  if (self->start_time == 0) {
    return G_SOURCE_CONTINUE;
  }

  // 计算已用时间（毫秒）
  gint64 current_time = g_get_monotonic_time();
  gint64 elapsed_time_ms = (current_time - self->start_time) / 1000;

  // 显示用时
  guint seconds = elapsed_time_ms / 1000;
  guint minutes = seconds / 60;
  seconds = seconds % 60;
  guint milliseconds = elapsed_time_ms % 1000;

  gtk_label_set_text(
      GTK_LABEL(self->timer),
      g_strdup_printf("%02u:%02u.%03u", minutes, seconds, milliseconds));

  // 计算实时击键速度（最近几次击键的平均速度）
  double realtime_stroke_speed = 0.0;

  if (g_queue_get_length(self->key_time_queue) >= 2) {
    gint64 first_time =
        GPOINTER_TO_SIZE(g_queue_peek_head(self->key_time_queue));
    gint64 time_diff_ms = (current_time - first_time) / 1000;

    // 确保时间差不为0
    if (time_diff_ms > 0) {
      // 计算每秒击键数
      realtime_stroke_speed = (g_queue_get_length(self->key_time_queue) - 1) *
                              1000.0 / time_diff_ms;
    }
  }

  // 计算整体打字速度（正确字符数/时间，每分钟字数）
  double overall_typing_speed = 0.0;

  if (elapsed_time_ms > 0 && self->total_char_count > 0) {
    // 转换为分钟并计算每分钟字数
    overall_typing_speed = (self->total_char_count * 60000.0) / elapsed_time_ms;
  }

  // 显示速度与击键信息
  gtk_label_set_text(GTK_LABEL(self->speed),
                     g_strdup_printf("%.2f", overall_typing_speed));
  gtk_label_set_text(GTK_LABEL(self->stroke),
                     g_strdup_printf("%.1f", realtime_stroke_speed));

  return G_SOURCE_CONTINUE;
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
  win->flag = RETYPE_READY;
  win->stroke_count = 0;
  win->correct_char_count = 0;
  win->total_char_count = 0;
  win->start_time = 0;

  // 重置UI显示
  gtk_label_set_text(GTK_LABEL(win->timer), "00:00.000");
  gtk_label_set_text(GTK_LABEL(win->speed), "0.0");
  gtk_label_set_text(GTK_LABEL(win->stroke), "0.0");

  // 清空跟打区
  GtkTextBuffer *follow_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->follow));
  gtk_text_buffer_set_text(follow_buffer, "", -1);

  // 移除对照区的所有标记
  GtkTextBuffer *control_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(control_buffer, &start);
  gtk_text_buffer_get_end_iter(control_buffer, &end);
  gtk_text_buffer_remove_all_tags(control_buffer, &start, &end);
}

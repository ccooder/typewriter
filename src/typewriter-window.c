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

  size_t welcome_length = utf8_strlen(welcome);
  gtk_label_set_text(GTK_LABEL(win->words),
                     g_strdup_printf("共%ld字", welcome_length));

  gtk_text_buffer_set_text(buffer, welcome, -1);
}

static gboolean on_key_press(GtkEventControllerKey *controller, guint keyval,
                             guint keycode, GdkModifierType state,
                             gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);

  if ((self->preedit_buffer == NULL || strlen(self->preedit_buffer) <= 0) &&
      (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
    // Return TRUE to indicate that the event has been handled and
    // should not be propagated further (preventing default newline insertion)
    return TRUE;
  }
  // For other keys, return FALSE to allow default processing
  return FALSE;
}

static void on_follow_buffer_changed(GtkTextBuffer *follow_buffer,
                                     gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
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
  win->preedit_buffer = preedit;
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

static size_t utf8_strlen(const char *s) {
  size_t count = 0;
  while (*s != '\0') {
    if ((*s & 0xC0) != 0x80) {  // If it's not a continuation byte
      count++;
    }
    s++;
  }
  return count;
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
    int text_length = utf8_strlen(text);
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

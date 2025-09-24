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

  /* Template widgets */
  // 对照区
  GtkTextView *control;
  // 跟打区
  // GtkTextView *follow;
  // 状态区
  // GtkStatusbar *statusbar;
};

G_DEFINE_FINAL_TYPE(TypewriterWindow, typewriter_window,
                    GTK_TYPE_APPLICATION_WINDOW)

static void typewriter_window_class_init(TypewriterWindowClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  gtk_widget_class_set_template_from_resource(
      widget_class, "/run/fenglu/typewriter/typewriter-window.ui");
  gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, control);
  // gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, follow);
  // gtk_widget_class_bind_template_child(widget_class, TypewriterWindow, statusbar);
}

static void typewriter_window_init(TypewriterWindow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

TypewriterWindow *typewriter_window_new(TypewriterApplication *app) {
  return g_object_new(TYPEWRITER_TYPE_WINDOW, "application", app, NULL);
}

void typewriter_window_open(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
  g_print("buffer: %d\n", buffer != NULL);
  GtkTextIter start_iter, end_iter;


  gtk_text_buffer_set_text(buffer, "Hello, world!", -1);
  gtk_text_buffer_get_start_iter(buffer, &start_iter);
  gtk_text_buffer_get_end_iter(buffer, &end_iter);

  g_print("buffer: %s\n", gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE));
}

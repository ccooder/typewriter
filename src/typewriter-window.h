/* typewriter-window.h
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

#pragma once

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "typewriter-application.h"

G_BEGIN_DECLS

#define TYPEWRITER_TYPE_WINDOW (typewriter_window_get_type())

G_DECLARE_FINAL_TYPE(TypewriterWindow, typewriter_window, TYPEWRITER, WINDOW,
                     GtkApplicationWindow)

TypewriterWindow *typewriter_window_new(TypewriterApplication *app);
void typewriter_window_open(TypewriterWindow *win);
static gboolean on_key_press(GtkEventControllerKey *controller, guint keyval,
                             guint keycode, GdkModifierType state,
                             gpointer user_data);
static void on_follow_buffer_changed(GtkTextBuffer *self, gpointer user_data);
static void load_css_providers(TypewriterWindow *self);
static size_t utf8_strlen(const char *s);
void load_file(TypewriterWindow *win);
void load_clipboard(TypewriterWindow *win);
static void load_clipboard_text(GdkClipboard *clipboard, GAsyncResult *result,
                                gpointer user_data);

G_END_DECLS

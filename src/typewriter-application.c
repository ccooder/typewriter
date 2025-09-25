/* typewriter-application.c
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

#include "typewriter-application.h"

#include <glib/gi18n.h>

#include "config.h"
#include "typewriter-window.h"

struct _TypewriterApplication {
  GtkApplication parent_instance;
};

G_DEFINE_FINAL_TYPE(TypewriterApplication, typewriter_application,
                    GTK_TYPE_APPLICATION)

TypewriterApplication *typewriter_application_new(const char *application_id,
                                                  GApplicationFlags flags) {
  g_return_val_if_fail(application_id != NULL, NULL);

  return g_object_new(TYPEWRITER_TYPE_APPLICATION, "application-id",
                      application_id, "flags", flags, "resource-base-path",
                      "/run/fenglu/typewriter", NULL);
}

static void typewriter_application_activate(GApplication *app) {
  TypewriterWindow *win;

  g_assert(TYPEWRITER_IS_APPLICATION(app));

  win = TYPEWRITER_WINDOW(
      gtk_application_get_active_window(GTK_APPLICATION(app)));

  if (win == NULL) {
    win = typewriter_window_new(TYPEWRITER_APPLICATION(app));
  }

  typewriter_window_open(win);

  gtk_window_present(GTK_WINDOW(win));
}

static void typewriter_application_class_init(
    TypewriterApplicationClass *klass) {
  GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

  app_class->activate = typewriter_application_activate;
}

static void typewriter_application_about_action(GSimpleAction *action,
                                                GVariant *parameter,
                                                gpointer user_data) {
  static const char *authors[] = {"King", NULL};
  TypewriterApplication *self = user_data;
  GtkWindow *window = NULL;

  g_assert(TYPEWRITER_IS_APPLICATION(self));

  window = gtk_application_get_active_window(GTK_APPLICATION(self));

  gtk_show_about_dialog(window, "program-name", "typewriter", "logo-icon-name",
                        "run.fenglu.typewriter", "authors", authors,
                        "translator-credits", _("translator-credits"),
                        "version", "0.1.0", "copyright", "© 2025 King", NULL);
}

static void typewriter_application_quit_action(GSimpleAction *action,
                                               GVariant *parameter,
                                               gpointer user_data) {
  TypewriterApplication *self = user_data;

  g_assert(TYPEWRITER_IS_APPLICATION(self));

  g_application_quit(G_APPLICATION(self));
}

static void typewriter_application_load_file_action(GSimpleAction *action,
                                                    GVariant *parameter,
                                                    gpointer user_data) {
  TypewriterApplication *self = user_data;

  g_assert(TYPEWRITER_IS_APPLICATION(self));

  TypewriterWindow *win = TYPEWRITER_WINDOW(
      gtk_application_get_active_window(GTK_APPLICATION(self)));

  load_file(win);
}

static void typewriter_application_load_clipboard_action(GSimpleAction *action,
                                                         GVariant *parameter,
                                                         gpointer user_data) {
  TypewriterApplication *self = user_data;

  g_assert(TYPEWRITER_IS_APPLICATION(self));
  TypewriterWindow *win = TYPEWRITER_WINDOW(
      gtk_application_get_active_window(GTK_APPLICATION(self)));
  load_clipboard(win);
}

static const GActionEntry app_actions[] = {
    {"load_file", typewriter_application_load_file_action},
    {"load_clipboard", typewriter_application_load_clipboard_action},
    {"quit", typewriter_application_quit_action},
    {"about", typewriter_application_about_action},
};

static void typewriter_application_init(TypewriterApplication *self) {
  g_action_map_add_action_entries(G_ACTION_MAP(self), app_actions,
                                  G_N_ELEMENTS(app_actions), self);
  gtk_application_set_accels_for_action(GTK_APPLICATION(self), "app.quit",
                                        (const char *[]){"<primary>q", NULL});
  gtk_application_set_accels_for_action(GTK_APPLICATION(self), "app.load_file",
                                        (const char *[]){"F6", NULL});
  gtk_application_set_accels_for_action(GTK_APPLICATION(self),
                                        "app.load_clipboard",
                                        (const char *[]){"<Alt>e", NULL});
}

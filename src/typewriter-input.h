//
// Created by king on 9/28/25.
//

#ifndef NFL_TYPEWRITER_TYPEWRITER_INPUT_H
#define NFL_TYPEWRITER_TYPEWRITER_INPUT_H
#include <gtk/gtk.h>
void on_preedit_changed(GtkTextView *self, gchar *preedit,
                               gpointer user_data);
gboolean on_key_press(GtkEventControllerKey *controller, guint keyval,
                             guint keycode, GdkModifierType state,
                             gpointer user_data);
void on_follow_buffer_changed(GtkTextBuffer *follow_buffer, gpointer user_data);
void on_follow_insert_text(GtkTextBuffer *buffer, GtkTextIter *location,
                           gchar *text, gint length, gpointer user_data);
void on_follow_delete_range(GtkTextBuffer *buffer, GtkTextIter *start,
                            GtkTextIter *end, gpointer user_data);
#endif  // NFL_TYPEWRITER_TYPEWRITER_INPUT_H

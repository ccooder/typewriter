//
// Created by king on 9/17/25.
//

#ifndef NFL_TYPEWRITER_TYPEWRITERWIN_H
#define NFL_TYPEWRITER_TYPEWRITERWIN_H
#include <gtk/gtk.h>
#include "typewriterapp.h"

#define TYPEWRITER_APP_WINDOW_TYPE (typewriter_app_window_get_type ())
G_DECLARE_FINAL_TYPE (TypewriterAppWindow, typewriter_app_window, TYPEWRITER, APP_WINDOW, GtkApplicationWindow)

TypewriterAppWindow *typewriter_app_window_new(TypewriterApp *app);
void typewriter_app_window_open(TypewriterAppWindow *win,
                             GFile *file);
#endif //NFL_TYPEWRITER_TYPEWRITERWIN_H
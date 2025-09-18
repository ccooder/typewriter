//
// Created by king on 9/17/25.
//

#ifndef NFL_TYPEWRITER_TYPEWRITERAPP_H
#define NFL_TYPEWRITER_TYPEWRITERAPP_H

#include <gtk/gtk.h>

#define TYPEWRITER_APP_TYPE (typewriter_app_get_type ())
G_DECLARE_FINAL_TYPE (TypewriterApp, typewriter_app, TYPEWRITER, APP, GtkApplication)

TypewriterApp *typewriter_app_new(void);

#endif //NFL_TYPEWRITER_TYPEWRITERAPP_H
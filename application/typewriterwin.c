//
// Created by king on 9/17/25.
//

#include "typewriterwin.h"

struct _TypewriterAppWindow {
    GtkApplicationWindow parent;
};

G_DEFINE_TYPE(TypewriterAppWindow, typewriter_app_window, GTK_TYPE_APPLICATION_WINDOW);

static void typewriter_app_window_init(TypewriterAppWindow *win) {
}

static void typewriter_app_window_class_init(TypewriterAppWindowClass *klass) {
}
TypewriterAppWindow *typewriter_app_window_new(TypewriterApp *app) {
    return g_object_new(TYPEWRITER_APP_WINDOW_TYPE,
                        "application", app,
                        "title", "Typewriter",
                        NULL);
}
void typewriter_app_window_open(TypewriterAppWindow *win, GFile *file) {

}

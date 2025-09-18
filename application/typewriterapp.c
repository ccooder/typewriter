//
// Created by king on 9/17/25.
//
#include <gtk/gtk.h>
#include "typewriterapp.h"
#include "typewriterwin.h"

struct _TypewriterApp
{
    GtkApplication parent;
};

G_DEFINE_TYPE(TypewriterApp, typewriter_app, GTK_TYPE_APPLICATION);

static void typewriter_app_init(TypewriterApp* app)
{
}

static void typewriter_app_activate(GApplication* app)
{
    TypewriterAppWindow* win;

    win = typewriter_app_window_new(TYPEWRITER_APP(app));
    gtk_window_present(GTK_WINDOW(win));
}

static void typewriter_app_open(GApplication* app, GFile** files, int n_files, const char* hint)
{
    GList* windows;
    TypewriterAppWindow* win;

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if (windows)
        win = TYPEWRITER_APP_WINDOW(windows->data);
    else
        win = typewriter_app_window_new(TYPEWRITER_APP(app));

    for (int i = 0; i < n_files; i++)
    {
        typewriter_app_window_open(win, files[i]);
    }

    gtk_window_present(GTK_WINDOW(win));
}


static void quit_activated(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    g_application_quit(G_APPLICATION(app));
}

static GActionEntry app_entries[] =
    {{"quit", quit_activated, NULL, NULL, NULL}};

static void typewriter_app_startup(GApplication* app)
{
    GtkBuilder* builder;
    GMenuModel* app_menu;
    const char* quit_accels[2] = {"<Ctrl>Q", NULL};

    G_APPLICATION_CLASS(typewriter_app_parent_class)->startup(app);
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", quit_accels);
}

static void typewriter_app_class_init(TypewriterAppClass* klass)
{
    G_APPLICATION_CLASS(klass)->activate = typewriter_app_activate;
    // G_APPLICATION_CLASS (klass)->open = typewriter_app_open;
    G_APPLICATION_CLASS(klass)->startup = typewriter_app_startup;
}

TypewriterApp* typewriter_app_new(void)
{
    return g_object_new(TYPEWRITER_APP_TYPE,
                        "application-id", "run.fenglu.typewriterapp",
                        "flags", G_APPLICATION_HANDLES_OPEN,
                        NULL);
}

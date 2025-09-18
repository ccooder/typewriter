#include <gtk/gtk.h>
#include "application/typewriterapp.h"

int main(int argc, char **argv) {
    return g_application_run(G_APPLICATION (typewriter_app_new()), argc, argv);
}
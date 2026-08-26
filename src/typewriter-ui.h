//
// Created by king on 9/28/25.
//

#ifndef NFL_TYPEWRITER_TYPEWRITER_UI_H
#define NFL_TYPEWRITER_TYPEWRITER_UI_H
#include <gtk/gtk.h>
gboolean update_stat_ui(gpointer user_data);
// 格式化并设置label文本，内部释放格式化字符串
void label_set_printf(GtkLabel *label, const char *format, ...);
#endif  // NFL_TYPEWRITER_TYPEWRITER_UI_H

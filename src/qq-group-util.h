//
// Created by King on 2025/10/25.
//

#ifndef NFL_TYPEWRITER_QQ_GROUP_UTIL_H
#define NFL_TYPEWRITER_QQ_GROUP_UTIL_H
#include <stdio.h>

#include "typewriter-window.h"
#include "x11-util.h"

void list_qq_group_window(TypewriterWindow *win);
void send_to_qq_group(TypewriterWindow *win, char *grade);
void on_qq_group_dropdown_clicked(GtkButton *button, gpointer user_data);
void on_qq_group_popover_closed(GtkPopover *popover, gpointer user_data);
void bind_cb(GtkListItemFactory *factory, GtkListItem *list_item);
void setup_cb(GtkListItemFactory *factory, GtkListItem *list_item);
void on_qq_group_selected(GtkListView *list_view, guint position,
                          gpointer user_data);
#endif  // NFL_TYPEWRITER_QQ_GROUP_UTIL_H



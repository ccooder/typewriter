//
// Created by king on 10/24/25.
//

#ifndef NFL_TYPEWRITER_QQ_GROUP_ITEM_H
#define NFL_TYPEWRITER_QQ_GROUP_ITEM_H

#include <X11/Xlib.h>
#include <glib-object.h>

#define QQ_GROUP_TYPE_ITEM qq_group_item_get_type()
G_DECLARE_FINAL_TYPE(QQGroupItem, qq_group_item, QQ_GROUP, ITEM, GObject)

// QQGroupItem definition
struct _QQGroupItem {
  GObject parent_instance;
  Window win;
  char *name;
  gboolean is_selected;
};

enum { PROP_WIN = 1, PROP_NAME, PROP_IS_SELECTED, N_PROPERTIES };

QQGroupItem *qq_group_item_new(const Window win, const char *name, gboolean is_selected);

#endif  // NFL_TYPEWRITER_QQ_GROUP_ITEM_H

//
// Created by king on 10/24/25.
//

#include "qq-group-item.h"

G_DEFINE_FINAL_TYPE(QQGroupItem, qq_group_item, G_TYPE_OBJECT)

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

// Get/set properties
static void
qq_group_item_set_property(GObject *object, guint property_id,
                          const GValue *value, GParamSpec *pspec) {
  QQGroupItem *self = QQ_GROUP_ITEM(object);
  switch (property_id) {
    case PROP_WIN:
      self->win = g_value_get_ulong(value);
      break;
    case PROP_NAME:
      g_free(self->name);
      self->name = g_value_dup_string(value);
      break;
    case PROP_IS_SELECTED:
      self->is_selected = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
qq_group_item_get_property(GObject *object, guint property_id,
                          GValue *value, GParamSpec *pspec) {
  QQGroupItem *self = QQ_GROUP_ITEM(object);
  switch (property_id) {
    case PROP_WIN:
      g_value_set_ulong(value, self->win);
      break;
    case PROP_NAME:
      g_value_set_string(value, self->name);
      break;
    case PROP_IS_SELECTED:
      g_value_set_boolean(value, self->is_selected);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}



static void
qq_group_item_class_init(QQGroupItemClass *class) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(class);
  gobject_class->set_property = qq_group_item_set_property;
  gobject_class->get_property = qq_group_item_get_property;

  obj_properties[PROP_WIN] =
    g_param_spec_ulong("win", "Win", "The item's window.",
                     0, G_MAXULONG, 0,
                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_NAME] =
    g_param_spec_string("name", "Name", "The item's name.",
                        NULL,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_IS_SELECTED] =
    g_param_spec_boolean("is_selected", "Is Selected", "Whether the item is selected.",
                         FALSE,
                         G_PARAM_READWRITE);

  g_object_class_install_properties(gobject_class, N_PROPERTIES, obj_properties);
}

static void
qq_group_item_init(QQGroupItem *self) {
  self->name = NULL;
}

QQGroupItem *
qq_group_item_new(const Window win, const char *name, gboolean is_selected) {
  return g_object_new(QQ_GROUP_TYPE_ITEM, "win", win, "name", name,
                      "is_selected", is_selected, NULL);
}
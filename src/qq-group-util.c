//
// Created by King on 2025/10/25.
//

#include "qq-group-util.h"

static QQGroupItem **list_qq_group_window_macos(TypewriterWindow *win,
                                                guint *win_count,
                                                gboolean *has_selected) {
  FILE *fp;
  char buffer[1024];
  char command[2048];
  QQGroupItem **items = nullptr;

  // Your AppleScript to be executed, returning a string

  // Construct the osascript command to execute the AppleScript and redirect its
  // output
  snprintf(command, sizeof(command), "osascript list_qq_win.scpt");

  // Execute the command and capture its output
  fp = popen(command, "r");
  if (fp == NULL) {
    perror("Failed to run command");
    return items;
  }

  // Read the output line by line
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    // Process the return value (e.g., print it)
    printf("AppleScript returned: %s", buffer);
  }

  // Close the file pointer
  pclose(fp);

  gchar *trimmed_buffer = g_strstrip(buffer);
  if (g_utf8_strlen(trimmed_buffer, -1) == 0) {
    g_print("不存在QQ群窗口\n");
    return items;
  }
  if (g_strcmp0(trimmed_buffer, "error") == 0) {
    g_print("获取QQ群窗口失败,请检查是否运行了QQ程序呢？\n");
    return items;
  }
  char **lines = g_strsplit(buffer, ", ", -1);
  for (int i = 0; lines[i] != NULL; i++) {
    g_print("%s\n", lines[i]);
    (*win_count)++;
    QQGroupItem *item = nullptr;
    if (g_strcmp0(win->selected_group->name, lines[i]) == 0) {
      item = qq_group_item_new(i, lines[i], TRUE);
      *has_selected = TRUE;
    } else {
      item = qq_group_item_new(i, lines[i], FALSE);
    }
    items = g_realloc(items, (*win_count) * sizeof(QQGroupItem *));
    items[(*win_count) - 1] = item;
  }

  // 释放资源
  g_strfreev(lines);

  return items;
}

static QQGroupItem **list_qq_group_window_linux(TypewriterWindow *win,
                                                guint *win_count,
                                                gboolean *has_selected) {
  QQGroupItem **items = nullptr;
  if (init_x11() != 0) {
    g_print("Cannot open display\n");
    return items;
  }

  int window_count;
  Window *windows = get_all_windows(&window_count, "qq");

  if (window_count == 0) {
    g_print("不存在QQ群窗口\n");
    return items;
  }
  for (int i = 0; i < window_count; i++) {
    char *window_name = NULL;
    get_window_title(windows[i], &window_name);
    if (window_name == NULL) {
      continue;
    }
    QQGroupItem *item = nullptr;
    if (g_strcmp0(win->selected_group->name, window_name) == 0) {
      item = qq_group_item_new(windows[i], window_name, TRUE);
      *has_selected = TRUE;
    } else {
      item = qq_group_item_new(windows[i], window_name, FALSE);
    }
    g_free(window_name);

    // 用紧凑计数器存放，跳过无标题窗口不留空洞
    items = g_realloc(items, (*win_count + 1) * sizeof(QQGroupItem *));
    items[(*win_count)++] = item;
  }
  free(windows);
  cleanup();
  return items;
}

// 粘贴阶段：激活QQ窗口并Ctrl+V。
// 此时主循环空闲，GDK才能响应QQ的selection请求把正文交出去
static gboolean paste_ready = FALSE;

static gboolean send_paste_cb(gpointer data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(data);
  paste_ready = FALSE;
  if (init_x11() != 0) {
    g_print("Cannot open display\n");
    return G_SOURCE_REMOVE;
  }
  g_print("\n=== 激活QQ窗口并粘贴 ===\n");
  activate_window(win->selected_group->win);
  send_qq_msg();
  paste_ready = TRUE;
  return G_SOURCE_REMOVE;
}

// 发送阶段：等正文落进输入框后再回车，否则发出去的是框里的旧内容
static gboolean send_enter_cb(gpointer data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(data);
  if (paste_ready) {
    gboolean ctrl_enter =
        win->settings != NULL &&
        g_settings_get_boolean(win->settings, "ctrl-enter-send");
    send_enter(ctrl_enter);
    cleanup();
    paste_ready = FALSE;
  }
  return G_SOURCE_REMOVE;
}

static void send_to_qq_group_linux(TypewriterWindow *win) {
  // 必须分阶段异步执行：GDK在主循环里才响应QQ的剪贴板selection请求，
  // 同步做完粘贴+回车会导致回车先到、正文后到
  g_timeout_add(300, send_paste_cb, win);
  g_timeout_add(800, send_enter_cb, win);
}

static void send_to_qq_group_macos(TypewriterWindow *win) {
  // 发送成绩到QQ
  if (init_x11() != 0) {
    g_print("Cannot open display\n");
    return;
  }
  Window qq_win = win->selected_group->win;
  if (qq_win != None) {
    g_print("\n=== 激活QQ窗口 ===\n");
    activate_window(qq_win);
    send_qq_msg();
  } else {
    g_print("未找到QQ窗口\n");
  }
  cleanup();
}

void list_qq_group_window(TypewriterWindow *win) {
  guint win_count = 0;
  gboolean has_selected = FALSE;
  QQGroupItem **items = nullptr;

#if defined(__APPLE__)
  items = list_qq_group_window_macos(win, &win_count, &has_selected);
#elif defined(__linux__)
  items = list_qq_group_window_linux(win, &win_count, &has_selected);
#else
  printf("Hello from another platform!\n");
#endif
  guint n_items =
      g_list_model_get_n_items(G_LIST_MODEL(win->qq_group_list_store));
  QQGroupItem *dive_item =
      g_list_model_get_item(G_LIST_MODEL(win->qq_group_list_store), 0);

  // splice会销毁旧的群窗口项，selected_group不能继续指向它们：
  // 未选中同名窗口（或未获取到窗口）时回到潜水项，否则splice后重指向新项
  dive_item->is_selected = !has_selected;
  if (!has_selected) {
    win->selected_group = dive_item;
    // 更新按钮文本
    GtkWidget *button_content =
        gtk_button_get_child(GTK_BUTTON(win->qq_group_dropdown));
    if (GTK_IS_BOX(button_content)) {
      GtkWidget *label = gtk_widget_get_first_child(button_content);
      if (GTK_IS_LABEL(label)) {
        gtk_label_set_text(GTK_LABEL(label), dive_item->name);
      }
    }
  }

  g_list_store_splice(win->qq_group_list_store, 1, n_items - 1,
                      (gpointer *)items, items != nullptr ? win_count : 0);

  if (has_selected && items != nullptr) {
    for (guint i = 0; i < win_count; i++) {
      if (items[i]->is_selected) {
        win->selected_group = items[i];
        break;
      }
    }
  }

  // 释放资源
  g_object_unref(dive_item);
  g_free(items);
}

void send_to_qq_group(TypewriterWindow *win, char *grade) {
  GdkDisplay *display;

  display = gtk_widget_get_display(GTK_WIDGET(win));
  GdkClipboard *clipboard = gdk_display_get_clipboard(display);
  gdk_clipboard_set_text(clipboard, grade);
  Window qq_win = win->selected_group->win;
  if (qq_win == 0) {
    g_print("当前处于潜水状态\n");
    return;
  }
#if defined(__APPLE__)
  send_to_qq_group_macos(win);
#elif defined(__linux__)
  send_to_qq_group_linux(win);
#else
  printf("Hello from another platform!\n");
#endif
}

void on_qq_group_dropdown_clicked(GtkButton *button, gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  // 设置Popover的位置相对于按钮
  gtk_popover_set_pointing_to(GTK_POPOVER(win->qq_group_popover),
                              NULL);  // 使用默认位置
  // 设置Popover的父控件
  gtk_widget_set_parent(win->qq_group_popover, GTK_WIDGET(button));

  // 获取QQ群窗口列表
  list_qq_group_window(win);

  // 显示Popover
  gtk_popover_popup(GTK_POPOVER(win->qq_group_popover));
}

void on_qq_group_popover_closed(GtkPopover *popover, gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  gtk_widget_unparent(win->qq_group_popover);
}

void bind_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkWidget *box = gtk_list_item_get_child(list_item);
  GtkWidget *label = gtk_widget_get_first_child(box);
  GtkWidget *check = gtk_widget_get_next_sibling(label);

  QQGroupItem *data = QQ_GROUP_ITEM(gtk_list_item_get_item(list_item));

  if (data && GTK_IS_LABEL(label)) {
    gtk_label_set_text(GTK_LABEL(label), data->name);
    gtk_widget_set_visible(check, data->is_selected);
  }
}

void setup_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *label = gtk_label_new(nullptr);
  GtkWidget *check = gtk_image_new_from_icon_name("object-select-symbolic");

  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_set_hexpand(label, TRUE);
  gtk_widget_set_visible(check, FALSE);

  gtk_box_append(GTK_BOX(box), label);
  gtk_box_append(GTK_BOX(box), check);
  gtk_list_item_set_child(list_item, box);
}

void on_qq_group_selected(GtkListView *list_view, guint position,
                          gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  // 先取消之前选中的项
  if (win->selected_group) {
    win->selected_group->is_selected = FALSE;
  }

  QQGroupItem *item = QQ_GROUP_ITEM(
      g_list_model_get_item(G_LIST_MODEL(win->qq_group_list_store), position));
  if (item) {
    item->is_selected = TRUE;
    win->selected_group = item;

    // 更新按钮文本
    GtkWidget *button_content =
        gtk_button_get_child(GTK_BUTTON(win->qq_group_dropdown));
    if (GTK_IS_BOX(button_content)) {
      GtkWidget *label = gtk_widget_get_first_child(button_content);
      if (GTK_IS_LABEL(label)) {
        gtk_label_set_text(GTK_LABEL(label), item->name);
      }
    }

    // 关闭popover
    gtk_popover_popdown(GTK_POPOVER(win->qq_group_popover));

    g_object_unref(item);
  }
}

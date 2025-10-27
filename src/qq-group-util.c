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
  *win_count = window_count;
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

    items = g_realloc(items, (i + 1) * sizeof(QQGroupItem *));
    items[i] = item;
  }
  free(windows);
  cleanup();
  return items;
}

static void send_to_qq_group_linux(TypewriterWindow *win) {
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
  if (items == nullptr) {
    return;
  }
  guint n_items =
      g_list_model_get_n_items(G_LIST_MODEL(win->qq_group_list_store));
  QQGroupItem *dive_item =
      g_list_model_get_item(G_LIST_MODEL(win->qq_group_list_store), 0);
  if (has_selected) {
    dive_item->is_selected = FALSE;
  } else {
    dive_item->is_selected = TRUE;
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
                      (gpointer *)items, win_count);
  // 释放资源
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
  send_to_qq_group_linux(win);
#elif defined(__linux__)
  send_to_qq_group_linux(win);
#else
  printf("Hello from another platform!\n");
#endif
  free(grade);
}

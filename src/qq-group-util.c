//
// Created by King on 2025/10/25.
//

#include "qq-group-util.h"

static void list_qq_group_window_macos(TypewriterWindow *win) {
  FILE *fp;
  char buffer[1024];
  char command[2048];

  // Your AppleScript to be executed, returning a string

  // Construct the osascript command to execute the AppleScript and redirect its output
  snprintf(command, sizeof(command), "osascript list_qq_win.scpt");

  // Execute the command and capture its output
  fp = popen(command, "r");
  if (fp == NULL) {
    perror("Failed to run command");
    return;
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
    return;
  }

  guint n_items = g_list_model_get_n_items(G_LIST_MODEL(win->qq_group_list_store));
  char **lines = g_strsplit(buffer, ", ", -1);
  guint win_count = 0;
  QQGroupItem **items = nullptr;
  gboolean selected = FALSE;
  for (int i = 0; lines[i] != NULL; i++) {
    g_print("%s\n", lines[i]);
    win_count++;
    QQGroupItem *item = nullptr;
    if (g_strcmp0(win->selected_group->name, lines[i]) == 0) {
      item = qq_group_item_new(i, lines[i], TRUE);
      selected = TRUE;
    } else {
      item = qq_group_item_new(i, lines[i], FALSE);
    }

    items = g_realloc(items, win_count * sizeof(QQGroupItem *));
    items[win_count - 1] = item;
  }
  if (selected) {
    QQGroupItem *item = g_list_model_get_item(G_LIST_MODEL(win->qq_group_list_store),0);
    item->is_selected = FALSE;
  }

  g_list_store_splice(win->qq_group_list_store, 1, n_items - 1, (gpointer *)items, win_count);

  // 释放资源
  g_strfreev(lines);
  g_free(items);
}

static void list_qq_group_window_linux(TypewriterWindow *win) {
  if (init_x11() != 0) {
    g_print("Cannot open display\n");
    return;
  }

  int window_count;
  Window *windows = get_all_windows(&window_count, "qq");

  guint n_items = g_list_model_get_n_items(G_LIST_MODEL(win->qq_group_list_store));

  for (int i = 0; i < window_count; i++) {
    print_window_info(windows[i]);

    QQGroupItem *item = qq_group_item_new(windows[i], "", FALSE);
    g_print("%s\n",item->name);
  }
}

void list_qq_group_window(TypewriterWindow *win) {

#if defined(__APPLE__)
  list_qq_group_window_macos(win);
#elif defined(__linux__)
  list_qq_group_window_linux(win);
#else
  printf("Hello from another platform!\n");
#endif

}
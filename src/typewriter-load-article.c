//
// Created by king on 9/28/25.
//

#include "typewriter-load-article.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <x11-util.h>

#include "qq-group-item.h"
#include "typewriter-window.h"

static void load_text(TypewriterWindow *win, char *text) {
  // 成功获取到文本，可以在这里进行处理
  gtk_label_set_text(GTK_LABEL(win->info), "来自剪贴板");
  win->article_name = "来自剪贴板";
  // 首先检测是否赛文格式
  GError *regex_error = NULL;
  GRegex *regex = g_regex_new("(.*?)\\n(.*?)\\n-----(第\\d+段).*?",
                              G_REGEX_DEFAULT, 0, &regex_error);
  if (regex_error != NULL) {
    g_printerr("Error compiling regex: %s\n", regex_error->message);
    g_error_free(regex_error);
  }

  if (regex_error == NULL) {
    GMatchInfo *match_info = NULL;
    g_regex_match(regex, text, 0, &match_info);
    if (g_match_info_matches(match_info)) {
      win->article_name = g_match_info_fetch(match_info, 3);
      gtk_label_set_label(
          GTK_LABEL(win->info),
          g_strdup_printf("%s-%s", g_match_info_fetch(match_info, 1),
                          win->article_name));
      text = g_match_info_fetch(match_info, 2);
      g_free(match_info);
      g_free(regex);
    }
  }

  regex = g_regex_new("\\s", G_REGEX_DEFAULT, 0, &regex_error);
  if (regex_error != NULL) {
    g_printerr("Error compiling regex: %s\n", regex_error->message);
    g_error_free(regex_error);
  } else {
    text = g_regex_replace(regex, text, -1, 0, "", 0, &regex_error);
  }
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->control));
  gtk_text_buffer_set_text(buffer, text, -1);

  // 计算文本长度
  int text_length = g_utf8_strlen(text, -1);
  win->stats.text_length = text_length;
  // 更新总字数标签
  gtk_label_set_text(GTK_LABEL(win->words),
                     g_strdup_printf("共%d字", text_length));

}

static void load_clipboard_text(GdkClipboard *clipboard, GAsyncResult *result,
                                gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  GError *error = NULL;

  // 读取文本内容
  char *text = gdk_clipboard_read_text_finish(clipboard, result, &error);

  if (error != NULL) {
    g_printerr("Error reading clipboard: %s\n", error->message);
    g_error_free(error);
    return;
  }

  if (text == NULL) {
    return;
  }

  load_text(win, text);

  typewriter_window_retype(win);

  g_free(text);

}

static void load_article_from_qq_group_linux(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));
  g_print("加载QQ群文章\n");

  if (init_x11() != 0) {
    g_print("Cannot open display\n");
    return;
  }

  Window qq_win = win->selected_group->win;
  if (qq_win == 0) {
    g_print("当前处理潜水状态\n");
    return;
  }

  if (qq_win != None) {
    g_print("\n=== 激活QQ窗口 ===\n");
    activate_window(qq_win);

    // 等待窗口激活
    usleep(100000);

    g_print("=== 尝试获取窗口文本 ===\n");

    char *text = get_window_text(qq_win);
    if (text) {
      g_print("获取到的文本:\n%s\n", text);
      load_text(win, text);
      g_free(text);
    } else {
      g_print("获取文本失败\n");
    }

    // 切回Typewriter
    Window typewriter_win = find_window_by_title("Typewriter", "typewriter");
    if (typewriter_win != None) {
      activate_window(typewriter_win);
    }
  } else {
    g_print("未找到QQ窗口\n");
  }

  cleanup();
}

static int load_article_from_qq_group_macos(TypewriterWindow *win) {
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
    return 1;
  }

  // Read the output line by line
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    // Process the return value (e.g., print it)
    printf("AppleScript returned: %s", buffer);
  }

  // Close the pipe
  pclose(fp);

  return 0;
}

void load_article_from_file(TypewriterWindow *win) { g_assert(TYPEWRITER_IS_WINDOW(win)); }

void load_article_from_clipboard(TypewriterWindow *win) {
  g_assert(TYPEWRITER_IS_WINDOW(win));

  GdkDisplay *display;

  display = gtk_widget_get_display(GTK_WIDGET(win));
  GdkClipboard *clipboard = gdk_display_get_clipboard(display);
  gdk_clipboard_read_text_async(clipboard, NULL,
                                (GAsyncReadyCallback)load_clipboard_text, win);
}

void load_article_from_qq_group(TypewriterWindow *win) {

#if defined(__APPLE__)
  load_article_from_qq_group_macos(win);
#elif defined(__linux__)
  load_article_from_qq_group_linux(win);
#else
  printf("Hello from another platform!\n");
#endif

}

//
// Created by king on 9/28/25.
//

#include "typewriter-load-article.h"
#include "typewriter-window.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>

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

  // 不要忘记释放文本资源
  g_free(text);

  typewriter_window_retype(win);
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
  g_assert(TYPEWRITER_IS_WINDOW(win));
  g_print("加载QQ群文章");
  Display *display = XOpenDisplay(NULL);
  if (!display) {
    g_print("Cannot open display\n");
  }
  Window root = DefaultRootWindow(display);

  Atom net_client_list = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False);
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *prop_return = NULL;

  XGetWindowProperty(display, root, net_client_list, 0, (~0L), False,
                     XA_WINDOW, &actual_type, &actual_format,
                     &nitems, &bytes_after, &prop_return);

  Window *windows = (Window *)prop_return;
  Window qq_window = None;
  for (unsigned int i = 0; i < nitems; ++i) {
    char *window_name;
    if (XFetchName(display, windows[i], &window_name)) {
      printf("Window ID: %lu, Name: %s\n", windows[i], window_name);
      if (g_str_has_prefix(window_name, "Calculator")) {
        printf("找到QQ窗口: %lu, Name: %s\n", windows[i], window_name);
        qq_window = windows[i];
      }
      XFree(window_name);
    }
  }
  if (qq_window != None) {
    printf("QQ窗口ID: %lu\n", qq_window);
    XRaiseWindow(display, qq_window);
    XSetInputFocus(display, qq_window, RevertToParent, CurrentTime);
    XFlush(display);
  }
}

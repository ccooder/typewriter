//
// Created by king on 10/15/25.
//

#include "x11-util.h"

// 全局变量
Display *x11display;
Window root_window;
// 全局变量用于选择处理
static Atom target_atom;
static char *selection_text = NULL;
static int selection_received = 0;

int init_x11() {
  x11display = XOpenDisplay(NULL);
  if (x11display == NULL) {
    fprintf(stderr, "无法打开X11显示\n");
    return -1;
  }
  root_window = DefaultRootWindow(x11display);
  return 0;
}

// 清理资源
void cleanup() {
  if (x11display) {
    XCloseDisplay(x11display);
    x11display = NULL;
  }
}

static gboolean is_window_minimized(Window win) {
  Atom wm_state = XInternAtom(x11display, "_NET_WM_STATE", False);
  Atom wm_hidden = XInternAtom(x11display, "_NET_WM_STATE_HIDDEN", False);
  Atom actual_type;
  int actual_format;
  unsigned long num_items, bytes_after;
  Atom *states = NULL;

  if (XGetWindowProperty(x11display, win, wm_state, 0, sizeof(Atom), False,
                         XA_ATOM, &actual_type, &actual_format, &num_items,
                         &bytes_after, (unsigned char **)&states) == Success) {
    for (unsigned int j = 0; j < num_items; j++) {
      if (states[j] == wm_hidden) {
        for (unsigned long i = 0; i < num_items; ++i) {
          if (states[i] == wm_hidden) {
            XFree(states);
            return TRUE;
          }
        }
      }
    }
    XFree(states);
  }

  return FALSE;
}

// 获取所有窗口的递归函数
void get_windows_recursive(Window w, Window **windows, int *count,
                           const char *class_name) {
  Window parent;
  Window *children = NULL;
  unsigned int nchildren;

  if (XQueryTree(x11display, w, &w, &parent, &children, &nchildren)) {
    for (unsigned int i = 0; i < nchildren; i++) {
      // 获取窗口类
      char *win_class_name = NULL;
      XClassHint class_hint;
      XWindowAttributes attrs;
      if (XGetClassHint(x11display, children[i], &class_hint)) {
        win_class_name = class_hint.res_name;
      }
      if (win_class_name && strcmp(win_class_name, class_name) != 0) {
        XFree(class_hint.res_name);
        XFree(class_hint.res_class);
        continue;
      }
      // 检查窗口是否映射（可见）
      if (win_class_name &&
          XGetWindowAttributes(x11display, children[i], &attrs)) {
        if (attrs.map_state == IsViewable || is_window_minimized(children[i])) {
          // 重新分配内存并添加窗口
          *windows = realloc(*windows, (*count + 1) * sizeof(Window));
          (*windows)[*count] = children[i];
          (*count)++;
        }
      }
      // 递归获取子窗口
      get_windows_recursive(children[i], windows, count, class_name);
      // XGetClassHint返回的res_name/res_class均由X11分配，所有路径都要释放
      if (win_class_name) {
        XFree(class_hint.res_name);
        XFree(class_hint.res_class);
      }
    }
    if (children) XFree(children);
  }
}

// 枚举期间窗口可能被销毁，XGet*调用会触发BadWindow并导致默认处理器退出进程；
// 枚举路径统一忽略X错误（各调用失败时返回0，调用方已按无属性处理）
static int ignore_x_errors(Display *display, XErrorEvent *event) {
  (void)display;
  (void)event;
  return 0;
}

// 获取所有打开的窗口
Window *get_all_windows(int *window_count, const char *class_name) {
  Window *windows = NULL;
  XErrorHandler old_handler;

  *window_count = 0;

  XSync(x11display, False);
  old_handler = XSetErrorHandler(ignore_x_errors);
  get_windows_recursive(root_window, &windows, window_count, class_name);
  XSync(x11display, False);
  XSetErrorHandler(old_handler);
  return windows;
}

void get_window_title(Window win, char **title) {
  XTextProperty text_prop;
  if (XGetWMName(x11display, win, &text_prop) && text_prop.value) {
    if (text_prop.encoding == XA_STRING) {
      *title = strdup((char *)text_prop.value);
    } else {
      char **list = NULL;
      int count = 0;
      if (XmbTextPropertyToTextList(x11display, &text_prop, &list, &count) ==
          Success) {
        if (count > 0) {
          *title = strdup(list[0]);
        }
        XFreeStringList(list);
      }
    }
    XFree(text_prop.value);
  }
}

// 激活窗口（使其获得焦点）
int activate_window(Window win) {
  XRaiseWindow(x11display, win);
  XSetInputFocus(x11display, win, RevertToParent, CurrentTime);

  XFlush(x11display);
  return 0;
}

// 根据窗口标题查找窗口
Window find_window_by_title(const char *title_pattern, char *class_name) {
  int window_count;
  Window *windows = get_all_windows(&window_count, class_name);
  Window found_window = None;

  for (int i = 0; i < window_count; i++) {
    char *window_title = NULL;
    get_window_title(windows[i], &window_title);
    if (window_title != NULL && strstr(window_title, title_pattern) != NULL) {
      found_window = windows[i];
    }
    free(window_title);
    if (found_window != None) {
      break;
    }
  }

  free(windows);
  return found_window;
}

// 选择事件处理函数
static void handle_selection_notify(XEvent event) {
  if (event.type == SelectionNotify) {
    // Check if the selection conversion was successful
    if (event.xselection.property != None) {
      // Read the selection data from the property
      Atom actual_type;
      int actual_format;
      unsigned long nitems;
      unsigned long bytes_after;
      unsigned char *prop_data = NULL;

      XGetWindowProperty(x11display, event.xselection.requestor,
                         event.xselection.property, 0, ~0L, False,
                         AnyPropertyType, &actual_type, &actual_format, &nitems,
                         &bytes_after, &prop_data);

      if (prop_data) {
        if (actual_type == XInternAtom(x11display, "UTF8_STRING", False) ||
            actual_type == XA_STRING) {
          selection_text = malloc(nitems + 1);
          memcpy(selection_text, prop_data, nitems);
          selection_text[nitems] = '\0';
          selection_received = 1;
        }
        XFree(prop_data);
      }
      XDeleteProperty(x11display, event.xselection.requestor,
                      event.xselection.property);
    }
  }
}

// 获取窗口中的文本（通过X11 selection机制）
char *get_window_text(Window win) {
  Atom clipboard = XInternAtom(x11display, "CLIPBOARD", False);
  selection_text = NULL;
  selection_received = 0;
  target_atom = XInternAtom(x11display, "UTF8_STRING", False);

  // 首先尝试选择窗口中的所有文本（模拟Ctrl+A）
  // 发送Ctrl+A按键事件
  KeyCode keycode_a = XKeysymToKeycode(x11display, XK_A);
  KeyCode keycode_c = XKeysymToKeycode(x11display, XK_C);
  KeyCode keycode_control = XKeysymToKeycode(x11display, XK_Control_L);
  KeyCode keycode_shift = XKeysymToKeycode(x11display, XK_Shift_L);
  KeyCode keycode_tab = XKeysymToKeycode(x11display, XK_Tab);

  // 按下Shift
  XTestFakeKeyEvent(x11display, keycode_shift, True, CurrentTime);
  // 按下Tab
  XTestFakeKeyEvent(x11display, keycode_tab, True, CurrentTime);
  // 释放Tab
  XTestFakeKeyEvent(x11display, keycode_tab, False, CurrentTime);
  // 释放Shift
  XTestFakeKeyEvent(x11display, keycode_shift, False, CurrentTime);

  // 按下Control
  XTestFakeKeyEvent(x11display, keycode_control, True, CurrentTime);
  // 按下A
  XTestFakeKeyEvent(x11display, keycode_a, True, CurrentTime);
  // 释放A
  XTestFakeKeyEvent(x11display, keycode_a, False, CurrentTime);
  // 释放Control
  XTestFakeKeyEvent(x11display, keycode_control, False, CurrentTime);

  // 按下Control
  XTestFakeKeyEvent(x11display, keycode_control, True, CurrentTime);
  // 按下C
  XTestFakeKeyEvent(x11display, keycode_c, True, CurrentTime);
  // 释放C
  XTestFakeKeyEvent(x11display, keycode_c, False, CurrentTime);
  // 释放Control
  XTestFakeKeyEvent(x11display, keycode_control, False, CurrentTime);

  XFlush(x11display);
  usleep(100000);  // 等待100ms让选择复制完成

  // 现在尝试从剪贴板获取文本
  // 创建临时窗口用于接收选择数据
  Window temp_win = XCreateSimpleWindow(
      x11display, DefaultRootWindow(x11display), 0, 0, 1, 1, 0, 0, 0);

  // 首先尝试CLIPBOARD
  Atom selection = clipboard;
  Window selection_owner = XGetSelectionOwner(x11display, selection);

  if (selection_owner == None) {
    // 尝试PRIMARY selection
    selection = XInternAtom(x11display, "PRIMARY", False);
    selection_owner = XGetSelectionOwner(x11display, selection);
  }

  if (selection_owner != None) {
    XConvertSelection(x11display, selection, target_atom,
                      XInternAtom(x11display, "XSEL_DATA", False), temp_win,
                      CurrentTime);
    XFlush(x11display);

    // 等待选择数据（带超时的事件循环）
    int timeout = 0;
    while (!selection_received && timeout < 50) {  // 5秒超时
      if (XPending(x11display)) {
        XEvent event;
        XNextEvent(x11display, &event);
        handle_selection_notify(event);
      } else {
        usleep(100000);  // 100ms
        timeout++;
      }
    }

    if (!selection_received) {
      g_print("获取选择数据超时\n");
    }
  } else {
    g_print("没有找到选择所有者\n");
  }
  XDestroyWindow(x11display, temp_win);

  return selection_text;
}

// 给QQ窗口粘贴文本（Ctrl+V）。
// 正文落进输入框依赖GDK在主循环里响应QQ的selection请求，
// 调用后必须回到主循环等待，不能紧接着发回车
void send_qq_msg() {
  KeyCode keycode_v = XKeysymToKeycode(x11display, XK_V);
  KeyCode keycode_control = XKeysymToKeycode(x11display, XK_Control_L);

  XTestFakeKeyEvent(x11display, keycode_control, True, CurrentTime);
  XTestFakeKeyEvent(x11display, keycode_v, True, CurrentTime);
  XTestFakeKeyEvent(x11display, keycode_v, False, CurrentTime);
  XTestFakeKeyEvent(x11display, keycode_control, False, CurrentTime);
  XFlush(x11display);
}

// 发送，ctrl_enter为真时按Ctrl+Enter（QQ发送键可配置，两种都要支持）
void send_enter(gboolean ctrl_enter) {
  KeyCode keycode_enter = XKeysymToKeycode(x11display, XK_Return);
  KeyCode keycode_control = XKeysymToKeycode(x11display, XK_Control_L);

  if (ctrl_enter) {
    XTestFakeKeyEvent(x11display, keycode_control, True, CurrentTime);
  }
  XTestFakeKeyEvent(x11display, keycode_enter, True, CurrentTime);
  XTestFakeKeyEvent(x11display, keycode_enter, False, CurrentTime);
  if (ctrl_enter) {
    XTestFakeKeyEvent(x11display, keycode_control, False, CurrentTime);
  }
  XFlush(x11display);
}

//
// Created by king on 10/15/25.
//

#ifndef NFL_TYPEWRITER_X11_UTIL_H
#define NFL_TYPEWRITER_X11_UTIL_H

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <gdk/gdk.h>

// 初始化X11连接
int init_x11();
// 清理资源
void cleanup();
// 获取所有打开的窗口
Window *get_all_windows(int *window_count, const char *class_name);
// 打印窗口信息
void print_window_info(Window win);
// 激活窗口（使其获得焦点）
int activate_window(Window win);
// 根据窗口标题查找窗口
Window find_window_by_title(const char *title_pattern, char *class_name);
// 获取窗口中的文本（通过X11 selection机制）
char* get_window_text(Window win);
// 给QQ发送消息
void send_qq_msg();
void send_enter();

#endif  // NFL_TYPEWRITER_X11_UTIL_H

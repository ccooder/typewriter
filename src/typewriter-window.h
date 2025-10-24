/* typewriter-window.h
 *
 * Copyright 2025 King
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "typewriter-application.h"

G_BEGIN_DECLS

#define TYPEWRITER_TYPE_WINDOW (typewriter_window_get_type())

// UI 刷新间隔 单位毫秒
#define REFRESH_INTERVAL 7

G_DECLARE_FINAL_TYPE(TypewriterWindow, typewriter_window, TYPEWRITER, WINDOW,
                     GtkApplicationWindow)

TypewriterWindow *typewriter_window_new(TypewriterApplication *app);
void typewriter_window_open(TypewriterWindow *win);
static void on_window_focus_enter(GtkEventControllerFocus *self,
                                  gpointer user_data);
static void on_window_focus_leave(GtkEventControllerFocus *self,
                                  gpointer user_data);
static void on_type_ended(TypewriterWindow *win, gpointer user_data);
static void on_qq_group_dropdown_clicked(GtkButton *button, gpointer user_data);
static void on_qq_group_popover_closed(GtkPopover *popover, gpointer user_data);
static void bind_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void setup_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void load_css_providers(TypewriterWindow *self);
void typewriter_pause(TypewriterWindow *self);
void typewriter_window_retype(TypewriterWindow *win);

// 建议改为：
typedef enum {
  TYPEWRITER_STATE_RETYPE_READY,
  TYPEWRITER_STATE_READY,
  TYPEWRITER_STATE_TYPING,
  TYPEWRITER_STATE_PAUSING,
  TYPEWRITER_STATE_ENDED
} TypewriterState;

// 在结构体中创建状态和统计数据结构
typedef struct {
  guint64 start_time;
  guint64 end_time;
  guint64 pause_start_time;
  guint64 pause_duration;
  guint stroke_count;
  guint correct_char_count;
  guint text_length;
  guint total_char_count;
  guint type_char_count;
  guint type_word_count;
  guint backspace_count;
  guint enter_count;
  guint reform_count;
} TypewriterStats;

struct _TypewriterWindow {
  GtkApplicationWindow parent_instance;
  GtkCssProvider *colors_provider;
  /* Template widgets */
  // QQ 群选择器
  GtkWidget *qq_group_dropdown;
  // QQ 群选择器 Popover
  GtkWidget *qq_group_popover;
  GtkWidget *qq_group_list;
  // 主区域
  GtkWidget *main_paned;
  // 对照区
  GtkWidget *control_scroll;
  GtkWidget *control;
  // 跟打区
  GtkWidget *follow_box;
  GtkWidget *follow;
  // 顶部状态区
  // 用时
  GtkWidget *timer;
  // 速度
  GtkWidget *speed;
  // 击键
  GtkWidget *stroke;
  // 码长
  GtkWidget *code_len;
  // 总字数
  GtkWidget *words;

  // 跟打信息区
  GtkWidget *info;

  // 中间信息区
  GtkWidget *mid_info;

  // 进度条
  GtkWidget *progressbar;

  // preedit buffer
  gchar *preedit_buffer;
  char *article_name;

  guint update_timer_id;
  // 实时击键速度
  GQueue *key_time_queue;
  guint max_queue_size;
  // 跟打状态
  TypewriterState state;
  // 跟打统计数据
  TypewriterStats stats;
};

G_END_DECLS

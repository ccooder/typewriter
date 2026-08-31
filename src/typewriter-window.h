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

#include "qq-group-item.h"
#include "typewriter-application.h"

G_BEGIN_DECLS

#define TYPEWRITER_TYPE_WINDOW (typewriter_window_get_type())

// UI 刷新间隔 单位毫秒
#define REFRESH_INTERVAL 17

// 慢字词阈值：距上次上屏超过该时长（微秒，打字钟已扣暂停）即计入
#define SLOW_THRESHOLD_US G_USEC_PER_SEC

// 慢字词记录：一次上屏超过阈值即记一条，text为对照区对应切片（多字=词）
typedef struct {
  gchar *text;
  double seconds;
} TypewriterSlowItem;

G_DECLARE_FINAL_TYPE(TypewriterWindow, typewriter_window, TYPEWRITER, WINDOW,
                     GtkApplicationWindow)

TypewriterWindow *typewriter_window_new(TypewriterApplication *app);
void typewriter_window_open(TypewriterWindow *win);
static void on_window_focus_enter(GtkEventControllerFocus *self,
                                  gpointer user_data);
static void on_window_focus_leave(GtkEventControllerFocus *self,
                                  gpointer user_data);
static void on_type_ended(TypewriterWindow *win, gpointer user_data);
static void load_css_providers(TypewriterWindow *self);
void typewriter_pause(TypewriterWindow *self);
void typewriter_window_retype(TypewriterWindow *win);

// 跟打状态枚举
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
  guint word_char_count;
  guint backspace_count;
  guint enter_count;
  guint reform_count;
} TypewriterStats;

typedef struct {
  gchar *segment;
  gchar *code;
  guint stroke_count;
  guint segment_duration;


} TypewriterSegment;

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
  // 状态/错字/正确率（左）
  GtkWidget *state_label;
  // 峰值击键/节奏稳定性（中）
  GtkWidget *live_metrics;
  // 进度/剩余/预计还需（右）
  GtkWidget *record_label;

  // 进度条
  GtkWidget *progressbar;

  // preedit buffer
  gchar *preedit_buffer;
  char *article_name;
  // 对照区上色tag（init时创建一次，归buffer的tag table所有）
  GtkTextTag *correct_tag;
  GtkTextTag *incorrect_tag;

  guint update_timer_id;
  // 最大化状态轮询定时器
  guint maximized_poll_id;
  // 窗口几何持久化（schema未安装时为NULL，功能静默关闭）
  GSettings *settings;
  // 实时击键速度
  GQueue *key_time_queue;
  guint max_queue_size;
  // 跟打状态
  TypewriterState state;
  // 慢字词记录链表（TypewriterSlowItem，结束时点击指标可看详情）
  GList *slow_items;
  // 上次上屏时的打字钟（微秒，已扣暂停），0=尚无上屏
  gint64 last_commit_elapsed;
  // 中间信息条拖拽起始时的paned位置（拖拽调整上下框比例用）
  gint mid_drag_start_pos;
  // 拖拽起始时的指针表面Y与最新偏移：偏移必须用表面坐标算——控件自身
  // 坐标会随set_position移动而漂移，用它算会形成"应用→坐标系平移→
  // 偏移回退→位置回跳"的锯齿闪烁
  double mid_drag_begin_y;
  double mid_drag_last_off;
  // 中间信息条拖拽上次应用时刻（微秒），节流防高频重排闪烁
  gint64 mid_drag_last_us;
  // 跟打统计数据
  TypewriterStats stats;
  // QQ群选择器
  GListStore *qq_group_list_store;
  QQGroupItem *selected_group;
};

G_END_DECLS

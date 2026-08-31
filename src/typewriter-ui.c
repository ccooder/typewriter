//
// Created by king on 9/28/25.
//

#include "typewriter-ui.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

void label_set_printf(GtkLabel *label, const gchar *format, ...) {
  va_list args;
  va_start(args, format);
  gchar *text = g_strdup_vprintf(format, args);
  va_end(args);
  gtk_label_set_text(label, text);
  g_free(text);
}

// 与label_set_printf同型，但文本按Pango标记解析（中间信息区的暗字/亮值排版）
static void label_set_markup_printf(GtkLabel *label, const char *format, ...)
    G_GNUC_PRINTF(2, 3);

static void label_set_markup_printf(GtkLabel *label, const char *format, ...) {
  va_list args;
  va_start(args, format);
  gchar *text = g_strdup_vprintf(format, args);
  va_end(args);
  gtk_label_set_markup(label, text);
  g_free(text);
}

// 纯函数：由统计值计算成绩指标并生成成绩单（不碰任何widget）
TypewriterGrade typewriter_build_grade(const char *article_name,
                                       const TypewriterStats *stats,
                                       const char *ime) {
  TypewriterGrade g = {0};
  g.elapsed_ms =
      ((gint64)stats->end_time - (gint64)stats->start_time -
       (gint64)stats->pause_duration) / 1000;
  if (g.elapsed_ms < 0) {
    g.elapsed_ms = 0;
  }

  // 分母为0时记0，避免inf/nan
  g.speed = (g.elapsed_ms > 0 && stats->total_char_count > 0)
                ? stats->total_char_count * 60000.0 / g.elapsed_ms
                : 0.0;
  g.stroke = g.elapsed_ms > 0
                 ? (double)stats->stroke_count * 1000.0 / g.elapsed_ms
                 : 0.0;
  g.code_len = stats->total_char_count > 0
                   ? (double)stats->stroke_count / stats->total_char_count
                   : 0.0;
  g.word_ratio = stats->total_char_count > 0
                     ? stats->word_char_count * 100.0 /
                           stats->total_char_count
                     : 0.0;

  guint total_seconds = (guint)(g.elapsed_ms / 1000);
  g.minutes = total_seconds / 60;
  g.seconds = total_seconds % 60;
  g.milliseconds = (guint)(g.elapsed_ms % 1000);

  g.grade = g_strdup_printf(
      "%s 速度%.2f 击键%.2f 码长%.2f 字数%d 错字%d 时间%02u:%02u.%03u 回改%d "
      "退格%d 回车%d 键数%d 打词%.2f%% 输入法:%s NFLinux跟打器\n",
      article_name, g.speed, g.stroke, g.code_len, stats->total_char_count,
      stats->total_char_count - stats->correct_char_count, g.minutes,
      g.seconds, g.milliseconds, stats->reform_count,
      stats->backspace_count, stats->enter_count, stats->stroke_count,
      g.word_ratio, ime);
  return g;
}

// 成绩计算的assert自检：./typewriter --self-test 或 meson test
void typewriter_stats_self_test(void) {
  TypewriterStats st = {0};
  st.start_time = 1000000;
  st.end_time = 4000000;  // 3s
  st.total_char_count = 6;
  st.correct_char_count = 5;
  st.stroke_count = 9;
  st.word_char_count = 4;

  TypewriterGrade g = typewriter_build_grade("第1段", &st, "TestIME");
  assert(g.elapsed_ms == 3000);
  assert(g.speed == 120.0);  // 6*60000/3000
  assert(g.stroke == 3.0);   // 9*1000/3000
  assert(g.code_len == 1.5); // 9/6
  assert(g.minutes == 0 && g.seconds == 3 && g.milliseconds == 0);
  assert(g.word_ratio > 66.66 && g.word_ratio < 66.67);  // 4*100/6
  assert(strstr(g.grade, "第1段 速度120.00"));
  assert(strstr(g.grade, "字数6 错字1"));
  assert(strstr(g.grade, "时间00:03.000"));
  assert(strstr(g.grade, "输入法:TestIME"));
  g_free(g.grade);

  // 暂停从用时中扣除
  st.pause_duration = 1000000;  // 3s-1s
  g = typewriter_build_grade("第1段", &st, "TestIME");
  assert(g.elapsed_ms == 2000);
  assert(g.speed == 180.0);
  g_free(g.grade);

  // 全0：分母为0的分支不得产生inf/nan/负数
  memset(&st, 0, sizeof(st));
  g = typewriter_build_grade("空", &st, "X");
  assert(g.speed == 0.0 && g.stroke == 0.0 && g.code_len == 0.0 &&
         g.word_ratio == 0.0);
  assert(g.minutes == 0 && g.seconds == 0 && g.milliseconds == 0);
  assert(strstr(g.grade, "速度0.00"));
  g_free(g.grade);

  // 峰值击键与节奏稳定性：间隔100/100/90ms → 峰值=1/0.09s≈11.11键/秒，
  // 变异系数≈0.0488 → 稳定性≈95.1%
  GQueue *q = g_queue_new();
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)0));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)100000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)200000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)290000));
  double peak = 0.0, stability = 0.0;
  typewriter_compute_peak_stability(q, &peak, &stability);
  assert(peak > 11.10 && peak < 11.12);
  assert(stability > 94.0 && stability < 96.0);
  // 不足两个时间戳（无间隔）时为0
  g_queue_clear(q);
  typewriter_compute_peak_stability(q, &peak, &stability);
  assert(peak == 0.0 && stability == 0.0);
  g_queue_free(q);
}

// 纯函数：峰值击键=最近击键间隔的最短者折算键/秒；节奏稳定性=1-间隔变异系数。
// 队列不足两个时间戳（无间隔）时两者置0
void typewriter_compute_peak_stability(GQueue *key_times, double *peak,
                                       double *stability) {
  *peak = 0.0;
  *stability = 0.0;
  if (key_times == NULL || g_queue_get_length(key_times) < 2) {
    return;
  }
  gint64 min_iv_us = G_MAXINT64;
  double sum = 0.0, sum_sq = 0.0;
  guint iv_count = 0;
  for (GList *l = key_times->head; l != NULL && l->next != NULL; l = l->next) {
    gint64 iv = (gint64)GPOINTER_TO_SIZE(l->next->data) -
                (gint64)GPOINTER_TO_SIZE(l->data);
    if (iv <= 0) {
      continue;
    }
    if (iv < min_iv_us) {
      min_iv_us = iv;
    }
    sum += (double)iv;
    sum_sq += (double)iv * (double)iv;
    iv_count++;
  }
  if (iv_count == 0) {
    return;
  }
  *peak = 1000000.0 / (double)min_iv_us;
  if (iv_count >= 2) {
    double mean = sum / iv_count;
    double variance = sum_sq / iv_count - mean * mean;
    if (variance < 0.0) {  // 浮点舍入可能出负
      variance = 0.0;
    }
    double s = (1.0 - sqrt(variance) / mean) * 100.0;
    *stability = s > 0.0 ? s : 0.0;
  }
}

// 中间信息区Pango配色：暗色说明字/亮色数值
#define MID_DIM "<span foreground='#93a5a1'>"
#define MID_VAL "<span foreground='#f2f7f6'>"
#define MID_END "</span>"

// 状态徽章配色css类，下标对应TypewriterState枚举值
static const char *const STATE_CSS[] = {"st-wait", "st-wait", "st-typing",
                                        "st-pause", "st-ended"};

// 中间信息区渲染，数据全部来自现有stats与击键时间队列。
// 左=状态徽章+回改/退格（成绩单字段中唯一过程中不可见的），中=峰值击键/
// 节奏稳定性（monkeytype的burst/consistency同款），右=进度/剩余/预计还需
void update_mid_info(TypewriterWindow *self) {
  const char *state_text = "等待跟打";
  switch (self->state) {
    case TYPEWRITER_STATE_RETYPE_READY:
    case TYPEWRITER_STATE_READY:
      break;
    case TYPEWRITER_STATE_TYPING:
      state_text = "跟打中";
      break;
    case TYPEWRITER_STATE_PAUSING:
      state_text = "已暂停";
      break;
    case TYPEWRITER_STATE_ENDED:
      state_text = "已结束";
      break;
    default:
      break;
  }
  // 徽章圆点与状态词随状态变色，其余文字由Pango标记固定配色
  for (gsize i = 0; i < G_N_ELEMENTS(STATE_CSS); i++) {
    gtk_widget_remove_css_class(GTK_WIDGET(self->state_label), STATE_CSS[i]);
  }
  gtk_widget_add_css_class(GTK_WIDGET(self->state_label),
                           STATE_CSS[self->state]);
  label_set_markup_printf(
      GTK_LABEL(self->state_label),
      "● %s  " MID_DIM "回改" MID_END " " MID_VAL "<b>%u</b>" MID_END
      " " MID_DIM "退格" MID_END " " MID_VAL "<b>%u</b>" MID_END,
      state_text, self->stats.reform_count, self->stats.backspace_count);

  double peak = 0.0, stability = 0.0;
  typewriter_compute_peak_stability(self->key_time_queue, &peak, &stability);
  if (peak > 0.0) {
    label_set_markup_printf(
        GTK_LABEL(self->live_metrics),
        MID_DIM "峰值击键" MID_END " " MID_VAL "<b>%.1f</b>" MID_END
        " " MID_DIM "键/秒" MID_END " · " MID_DIM "节奏稳定性" MID_END
        " " MID_VAL "<b>%.0f%%</b>" MID_END,
        peak, stability);
  } else {
    gtk_label_set_markup(GTK_LABEL(self->live_metrics),
                         MID_DIM "峰值击键" MID_END " " MID_VAL "<b>--</b>"
                                 MID_END " " MID_DIM "键/秒" MID_END
                                         " · " MID_DIM "节奏稳定性" MID_END
                                         " " MID_VAL "<b>--%</b>" MID_END);
  }

  // 慢字词计数：距上次上屏超1秒的字/词实时累加，进度条仍提供进度感
  label_set_markup_printf(GTK_LABEL(self->record_label),
                          MID_DIM "慢" MID_END " " MID_VAL "<b>%u</b>" MID_END,
                          g_list_length(self->slow_items));
  // 结束后可点击进入详情（下划线作可点击提示，tooltip在on_type_ended设置）
  if (self->state == TYPEWRITER_STATE_ENDED && self->slow_items != NULL) {
    gtk_widget_add_css_class(GTK_WIDGET(self->record_label), "slow-link");
  } else {
    gtk_widget_remove_css_class(GTK_WIDGET(self->record_label), "slow-link");
  }
}

gboolean update_stat_ui(gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->state != TYPEWRITER_STATE_TYPING) {
    return G_SOURCE_CONTINUE;
  }

  // 计算已用时间（毫秒）
  gint64 current_time = g_get_monotonic_time();
  gint64 elapsed_time_ms =
      (current_time - self->stats.start_time - self->stats.pause_duration) / 1000.0;

  // 显示用时
  guint seconds = elapsed_time_ms / 1000;
  guint minutes = seconds / 60;
  seconds = seconds % 60;
  guint milliseconds = elapsed_time_ms % 1000;

  label_set_printf(GTK_LABEL(self->timer), "%02u:%02u.%03u", minutes, seconds,
                   milliseconds);

  // 计算实时击键速度（最近几次击键的平均速度）
  double realtime_stroke_speed = 0.0;

  if (g_queue_get_length(self->key_time_queue) >= 2) {
    gint64 first_time =
        GPOINTER_TO_SIZE(g_queue_peek_head(self->key_time_queue));
    gint64 time_diff_us = (current_time - first_time);

    // 确保时间差不为0
    if (time_diff_us > 0) {
      // 计算每秒击键数
      realtime_stroke_speed = (g_queue_get_length(self->key_time_queue) - 1) *
                              1000000.0 / time_diff_us;
    }

    // 计算码长
    if (self->stats.total_char_count > 0) {
      gdouble code_len = self->stats.stroke_count * 1.0 / self->stats.total_char_count;
      label_set_printf(GTK_LABEL(self->code_len), "%.2f", code_len);
    }
  }

  // 计算整体打字速度（正确字符数/时间，每分钟字数）
  double overall_typing_speed = 0.0;

  if (elapsed_time_ms > 0 && self->stats.total_char_count > 0) {
    // 转换为分钟并计算每分钟字数
    overall_typing_speed = (self->stats.total_char_count * 60000.0) / elapsed_time_ms;
  }

  // 显示速度与击键信息
  label_set_printf(GTK_LABEL(self->speed), "%.2f", overall_typing_speed);
  label_set_printf(GTK_LABEL(self->stroke), "%.2f", realtime_stroke_speed);

  update_mid_info(self);

  return G_SOURCE_CONTINUE;
}
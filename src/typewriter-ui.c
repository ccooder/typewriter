//
// Created by king on 9/28/25.
//

#include "typewriter-ui.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

void label_set_printf(GtkLabel *label, const char *format, ...) {
  va_list args;
  va_start(args, format);
  gchar *text = g_strdup_vprintf(format, args);
  va_end(args);
  gtk_label_set_text(label, text);
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

  return G_SOURCE_CONTINUE;
}
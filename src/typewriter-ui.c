//
// Created by king on 9/28/25.
//

#include "typewriter-ui.h"
#include "typewriter-window.h"

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

  gtk_label_set_text(
      GTK_LABEL(self->timer),
      g_strdup_printf("%02u:%02u.%03u", minutes, seconds, milliseconds));

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
      gtk_label_set_text(GTK_LABEL(self->code_len),
                         g_strdup_printf("%.2f", code_len));
    }
  }

  // 计算整体打字速度（正确字符数/时间，每分钟字数）
  double overall_typing_speed = 0.0;

  if (elapsed_time_ms > 0 && self->stats.total_char_count > 0) {
    // 转换为分钟并计算每分钟字数
    overall_typing_speed = (self->stats.total_char_count * 60000.0) / elapsed_time_ms;
  }

  // 显示速度与击键信息
  gtk_label_set_text(GTK_LABEL(self->speed),
                     g_strdup_printf("%.2f", overall_typing_speed));
  gtk_label_set_text(GTK_LABEL(self->stroke),
                     g_strdup_printf("%.2f", realtime_stroke_speed));

  return G_SOURCE_CONTINUE;
}
//
// Created by king on 9/28/25.
//

#include "typewriter-ui.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static gint gint64_cmp(const void *a, const void *b) {
  gint64 x = *(const gint64 *)a, y = *(const gint64 *)b;
  return (x > y) - (x < y);
}

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

  // 峰值击键与节奏稳定性：时间戳0/90/190/290ms → 窗口平均间隔
  // =(290-0)/3≈96.7ms → 峰值≈10.3键/秒；中位数=100ms，MAD=0 → 稳定性=100%
  GQueue *q = g_queue_new();
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)0));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)90000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)190000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)290000));
  double peak = 0.0, stability = 0.0;
  typewriter_compute_peak(q, &peak);
  typewriter_compute_stability(q, &stability);
  assert(peak > 10.3 && peak < 10.4);
  assert(stability == 100.0);

  // 稳定打字中间夹一个1s停顿：停顿(>3x中位数)被剔除，稳定性不受影响
  g_queue_clear(q);
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)0));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)100000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)200000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)1200000));  // 1s停顿
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)1300000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)1400000));
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)1500000));
  typewriter_compute_peak(q, &peak);
  typewriter_compute_stability(q, &stability);
  // 剔除停顿后 5x100ms 间隔，中位数100、MAD=0 → 100%
  assert(stability > 99.0);

  // 不足两个时间戳（无间隔）时两者为0
  g_queue_clear(q);
  typewriter_compute_peak(q, &peak);
  typewriter_compute_stability(q, &stability);
  assert(peak == 0.0 && stability == 0.0);

  // 峰值按最近16键窗口计算：17个键里最早那个(0us)被挤出窗口，
  // 窗口内最短间隔仍是100ms → 峰值=1/0.1s=10键/秒
  g_queue_clear(q);
  g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)0));
  for (int i = 0; i < 16; i++) {
    g_queue_push_tail(q, GSIZE_TO_POINTER((gsize)(i+1) * 100000));
  }
  typewriter_compute_peak(q, &peak);
  assert(peak > 9.9 && peak < 10.1);  // 1/0.1s=10键/秒
  g_queue_free(q);
}

// 纯函数：峰值击键=最近16键平均间隔折算键/秒（1/平均间隔）。
// 队列不足两个时间戳（无间隔）时置0
void typewriter_compute_peak(GQueue *key_times, double *peak) {
  *peak = 0.0;
  if (key_times == NULL || g_queue_get_length(key_times) < 2) {
    return;
  }
  guint64 first_time = GPOINTER_TO_SIZE(key_times->head->data);
  guint64 last_time = GPOINTER_TO_SIZE(key_times->tail->data);
  guint count = g_queue_get_length(key_times);
  if (last_time > first_time && count >= 2) {
    // 平均间隔 = 总时差 / (键数-1)
    *peak = (double)(count - 1) * 1000000.0 / (double)(last_time - first_time);
  }
}

// 纯函数：节奏稳定性=1-MAD/中位数。用中位数绝对偏差（MAD）而非标准差：
// 个别稍长的停顿对稳定性影响很小（MAD对离群不敏感），只有持续的不均匀
// 才拉低稳定性；超过3倍中位数的间隔视为停顿直接剔除。队列存打字钟
// （暂停段时钟冻结），暂停已排除在间隔计算外。
// 队列不足两个时间戳（无间隔）时置0
void typewriter_compute_stability(GQueue *key_times, double *stability) {
  *stability = 0.0;
  if (key_times == NULL || g_queue_get_length(key_times) < 2) {
    return;
  }
  GPtrArray *ivs = g_ptr_array_new();
  for (GList *l = key_times->head; l != NULL && l->next != NULL; l = l->next) {
    gint64 iv = (gint64)GPOINTER_TO_SIZE(l->next->data) -
                (gint64)GPOINTER_TO_SIZE(l->data);
    if (iv > 0) {
      g_ptr_array_add(ivs, GINT_TO_POINTER((gint)iv));
    }
  }
  guint n = ivs->len;
  if (n >= 3) {
    // 中位数
    gint64 *arr = g_new(gint64, n);
    for (guint i = 0; i < n; i++) {
      arr[i] = GPOINTER_TO_SIZE(g_ptr_array_index(ivs, i));
    }
    qsort(arr, n, sizeof(gint64), gint64_cmp);
    gint64 median = arr[n / 2];
    gint64 cutoff = median * 3;
    g_free(arr);

    // 剔除停顿（>3x中位数）后，对剩余求各值偏离中位数的绝对值
    gint64 *devs = g_new(gint64, n);
    guint dev_count = 0;
    for (guint i = 0; i < n; i++) {
      gint64 iv = GPOINTER_TO_SIZE(g_ptr_array_index(ivs, i));
      if (iv > cutoff) {
        continue;
      }
      dev_count++;
      gint64 d = iv - median;
      devs[dev_count - 1] = d >= 0 ? d : -d;
    }
    // MAD = |devs|的中位数
    qsort(devs, dev_count, sizeof(gint64), gint64_cmp);
    gint64 mad = devs[dev_count / 2];
    g_free(devs);
    g_ptr_array_free(ivs, TRUE);
    if (median > 0 && dev_count >= 2) {
      double s = (1.0 - (double)mad / (double)median) * 100.0;
      *stability = s > 0.0 ? s : 0.0;
    }
    return;
  }
  if (n == 2) {
    double a = GPOINTER_TO_SIZE(g_ptr_array_index(ivs, 0));
    double b = GPOINTER_TO_SIZE(g_ptr_array_index(ivs, 1));
    double s = (1.0 - fabs(a - b) / (a + b)) * 100.0;
    *stability = s > 0.0 ? s : 0.0;
    g_ptr_array_free(ivs, TRUE);
    return;
  }
  g_ptr_array_free(ivs, TRUE);
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

  // 峰值击键：当前16键窗口速度与历史峰值取较大者，只增不减
  double current_peak = 0.0, stability = 0.0;
  typewriter_compute_peak(self->key_time_queue, &current_peak);
  typewriter_compute_stability(self->stability_queue, &stability);
  if (current_peak > self->peak_stroke) {
    self->peak_stroke = current_peak;
  }
  double peak = self->peak_stroke;
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

  // 计算已用时间（毫秒，打字钟：暂停段已扣除）
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
    // 队列存打字钟（已扣暂停），比较须同在打字钟空间
    gint64 current_clock = current_time - (gint64)self->stats.pause_duration;
    gint64 first_time =
        GPOINTER_TO_SIZE(g_queue_peek_head(self->key_time_queue));
    gint64 time_diff_us = (current_clock - first_time);

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
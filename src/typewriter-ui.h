//
// Created by king on 9/28/25.
//

#ifndef NFL_TYPEWRITER_TYPEWRITER_UI_H
#define NFL_TYPEWRITER_TYPEWRITER_UI_H
#include <gtk/gtk.h>

#include "typewriter-window.h"

gboolean update_stat_ui(gpointer user_data);
// 中间信息区渲染：左=状态/回改/退格，中=峰值击键/节奏稳定性，右=慢字词计数。
// 17ms刷新与状态切换（暂停/结束/重打/载文）时调用
void update_mid_info(TypewriterWindow *self);
// 纯函数：由最近击键时间队列算峰值击键（键/秒，最短间隔折算）。
// 队列不足两个时间戳时置0
void typewriter_compute_peak(GQueue *key_times, double *peak);
// 纯函数：由全量击键队列算节奏稳定性（%，MAD/中位数）。
// 队列不足两个时间戳时置0
void typewriter_compute_stability(GQueue *key_times, double *stability);
// 格式化并设置label文本，内部释放格式化字符串
// 格式属性：既让调用点获得格式检查，也豁免对g_strdup_vprintf透传的-Wformat-nonliteral
void label_set_printf(GtkLabel *label, const char *format, ...)
    G_GNUC_PRINTF(2, 3);

// 结束时的成绩指标（grade为成绩单文本，用完g_free）
typedef struct {
  gint64 elapsed_ms;  // 用时（毫秒，已扣除暂停）
  double speed;       // 速度（字/分）
  double stroke;      // 击键（键/秒）
  double code_len;    // 码长（键/字）
  double word_ratio;  // 打词（%）
  guint minutes, seconds, milliseconds;
  gchar *grade;
} TypewriterGrade;

// 纯函数：由统计值计算成绩指标并生成成绩单（不碰任何widget）
TypewriterGrade typewriter_build_grade(const char *article_name,
                                       const TypewriterStats *stats,
                                       const char *ime);
// 成绩计算的assert自检：./typewriter --self-test 或 meson test
void typewriter_stats_self_test(void);
#endif  // NFL_TYPEWRITER_TYPEWRITER_UI_H

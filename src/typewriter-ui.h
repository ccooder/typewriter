//
// Created by king on 9/28/25.
//

#ifndef NFL_TYPEWRITER_TYPEWRITER_UI_H
#define NFL_TYPEWRITER_TYPEWRITER_UI_H
#include <gtk/gtk.h>

#include "typewriter-window.h"

gboolean update_stat_ui(gpointer user_data);
// 格式化并设置label文本，内部释放格式化字符串
void label_set_printf(GtkLabel *label, const char *format, ...);

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

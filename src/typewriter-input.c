//
// Created by king on 9/28/25.
//

#include "typewriter-input.h"

#include "typewriter-ui.h"
#include "typewriter-window.h"

void on_preedit_changed(GtkTextView *self, gchar *preedit, gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  g_free(win->preedit_buffer);
  win->preedit_buffer = g_strdup(preedit);
}

static gboolean handle_special_keys(TypewriterWindow *self, guint keyval) {
  // 处理特殊按键逻辑
  if ((self->preedit_buffer == NULL || strlen(self->preedit_buffer) <= 0) &&
      (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
    // 只有跟打中的回车（用于暂停）才计入成绩
    if (self->state == TYPEWRITER_STATE_TYPING) {
      self->stats.enter_count++;
      typewriter_pause(self);
    }
    return TRUE;
  }

  return FALSE;
}

gboolean is_special_key(guint keyval) {
  // 修饰键keysym连续区段Shift_L(0xffe1)~Hyper_R(0xffee)，左右两侧统一判定
  if ((keyval >= GDK_KEY_Shift_L && keyval <= GDK_KEY_Hyper_R) ||
      keyval == GDK_KEY_ISO_Level3_Shift || keyval == GDK_KEY_ISO_Level5_Shift) {
    return TRUE;
  }
  switch (keyval) {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Escape:
    case GDK_KEY_Tab:
    case GDK_KEY_ISO_Left_Tab:
    case GDK_KEY_Num_Lock:
    case GDK_KEY_Scroll_Lock:
    // 导航键不产生文本，不计击键（退格/删除仍计，属编辑动作）
    case GDK_KEY_Left:
    case GDK_KEY_Right:
    case GDK_KEY_Up:
    case GDK_KEY_Down:
    case GDK_KEY_Home:
    case GDK_KEY_End:
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Left:
    case GDK_KEY_KP_Right:
    case GDK_KEY_KP_Up:
    case GDK_KEY_KP_Down:
    case GDK_KEY_KP_Home:
    case GDK_KEY_KP_End:
    case GDK_KEY_KP_Page_Up:
    case GDK_KEY_KP_Page_Down:
    case GDK_KEY_KP_Begin:
      return TRUE;
    default:
      return keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12;
  }
}

static void update_typing_state(TypewriterWindow *self, guint keyval) {
  // 更新打字状态
  if ((self->state == TYPEWRITER_STATE_PAUSING ||
       self->state == TYPEWRITER_STATE_READY) &&
      !is_special_key(keyval)) {
    gint64 current_time = g_get_monotonic_time();
    if (self->state == TYPEWRITER_STATE_READY) {
      self->stats.start_time = current_time;
    }
    if (self->state == TYPEWRITER_STATE_PAUSING) {
      // 计算暂停时长
      self->stats.pause_duration +=
          (current_time - self->stats.pause_start_time);
      self->stats.pause_start_time = 0;
    }
    self->state = TYPEWRITER_STATE_TYPING;

    // 防止重复设置定时器
    if (self->update_timer_id == 0) {
      self->update_timer_id =
          g_timeout_add(REFRESH_INTERVAL, update_stat_ui, self);
    }
  }
}

static void record_keystroke(TypewriterWindow *self, guint keyval) {
  // 记录击键信息
  if (self->state == TYPEWRITER_STATE_TYPING) {
    if (keyval == GDK_KEY_BackSpace) {
      self->stats.backspace_count++;
    }
    self->stats.stroke_count++;

    // 更新击键时间队列
    guint64 current_time = g_get_monotonic_time();
    g_queue_push_tail(self->key_time_queue, GSIZE_TO_POINTER(current_time));

    // 保持队列大小不超过max_queue_size
    while (g_queue_get_length(self->key_time_queue) > self->max_queue_size) {
      g_queue_pop_head(self->key_time_queue);
    }
  }
}

// 主处理函数
gboolean on_key_press(GtkEventControllerKey *controller, guint keyval,
                      guint keycode, GdkModifierType state,
                      gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);

  if (self->state == TYPEWRITER_STATE_ENDED) {
    return TRUE;
  }

  if (self->state == TYPEWRITER_STATE_RETYPE_READY) {
    self->state = TYPEWRITER_STATE_READY;
  }

  // 处理特殊按键
  if (handle_special_keys(self, keyval)) {
    return TRUE;
  }

  // 更新状态
  update_typing_state(self, keyval);

  // 记录击键
  record_keystroke(self, keyval);

  // 清理preedit buffer
  if (self->preedit_buffer != NULL) {
    g_free(self->preedit_buffer);
    self->preedit_buffer = NULL;
  }

  return FALSE;
}

// 撤销前from_chars字之后的比对结果：清上色、扣回正确数。
// 中途插入/删除会使后续比对整体错位，从编辑点起全部失效重比。
// 边界用字符数（=stats.total_char_count），两缓冲在边界处字符数相同；
// 字节偏移在打过字节长不同的错字时两缓冲会错开，不能用
static void invalidate_from(TypewriterWindow *self, gint from_chars) {
  if (from_chars >= (gint)self->stats.total_char_count) {
    return;
  }
  GtkTextBuffer *control_buffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->control));
  GtkTextIter from, to;
  gtk_text_buffer_get_start_iter(control_buffer, &from);
  gtk_text_iter_forward_chars(&from, from_chars);
  to = from;
  gtk_text_iter_forward_chars(&to, self->stats.total_char_count - from_chars);

  gint removed_correct = 0, removed_chars = 0;
  GtkTextIter cit = from;
  while (gtk_text_iter_compare(&cit, &to) < 0) {
    if (gtk_text_iter_has_tag(&cit, self->correct_tag)) {
      removed_correct++;
    }
    gtk_text_iter_forward_char(&cit);
    removed_chars++;
  }
  gtk_text_buffer_remove_tag(control_buffer, self->correct_tag, &from, &to);
  gtk_text_buffer_remove_tag(control_buffer, self->incorrect_tag, &from, &to);

  self->stats.correct_char_count -= removed_correct;
  self->stats.total_char_count = from_chars;
}

// 跟打区中途插入：插入点之前的比对仍有效，其后失效
void on_follow_insert_text(GtkTextBuffer *buffer, GtkTextIter *location,
                           gchar *text, gint length, gpointer user_data) {
  (void)buffer;
  (void)text;
  (void)length;
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->state == TYPEWRITER_STATE_ENDED) {
    return;
  }
  invalidate_from(self, gtk_text_iter_get_offset(location));
}

// 跟打区删除：回改按实际删除字数计，删除点之前的比对仍有效、其后失效
void on_follow_delete_range(GtkTextBuffer *buffer, GtkTextIter *start,
                            GtkTextIter *end, gpointer user_data) {
  (void)buffer;
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->state == TYPEWRITER_STATE_ENDED) {
    return;
  }
  gint deleted = 0;
  GtkTextIter it = *start;
  while (gtk_text_iter_compare(&it, end) < 0) {
    gtk_text_iter_forward_char(&it);
    deleted++;
  }
  self->stats.reform_count += deleted;
  invalidate_from(self, gtk_text_iter_get_offset(start));
}

void on_follow_buffer_changed(GtkTextBuffer *follow_buffer,
                              gpointer user_data) {
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->state == TYPEWRITER_STATE_ENDED) {
    return;
  }
  GtkTextView *control = GTK_TEXT_VIEW(self->control);
  GtkTextBuffer *control_buffer = gtk_text_view_get_buffer(control);
  guint old_total = self->stats.total_char_count;

  // 增量比对：只处理上次边界之后的新字符（删除/中途插入已由
  // delete-range/insert-text把边界退回到编辑点），上色只动新区间。
  // 迭代器按字符数推进（get_offset/get_iter_at_offset分别是字符/字节单位，勿混用）
  GtkTextIter f_it, c_it, c_next;
  gtk_text_buffer_get_start_iter(follow_buffer, &f_it);
  gtk_text_buffer_get_start_iter(control_buffer, &c_it);
  gtk_text_iter_forward_chars(&f_it, old_total);
  gtk_text_iter_forward_chars(&c_it, old_total);
  guint ccc = self->stats.correct_char_count;
  guint compared = 0;
  while (!gtk_text_iter_is_end(&f_it) && !gtk_text_iter_is_end(&c_it)) {
    c_next = c_it;
    gtk_text_iter_forward_char(&c_next);
    if (gtk_text_iter_get_char(&f_it) == gtk_text_iter_get_char(&c_it)) {
      gtk_text_buffer_apply_tag(control_buffer, self->correct_tag, &c_it,
                                &c_next);
      ccc++;
    } else {
      gtk_text_buffer_apply_tag(control_buffer, self->incorrect_tag, &c_it,
                                &c_next);
    }
    gtk_text_iter_forward_char(&f_it);
    gtk_text_iter_forward_char(&c_it);
    compared++;
  }
  self->stats.correct_char_count = ccc;
  self->stats.total_char_count = old_total + compared;

  // 滚动跟随：打到当前位置前7字处，出视野才滚
  GtkTextIter look;
  gtk_text_buffer_get_start_iter(control_buffer, &look);
  gtk_text_iter_forward_chars(&look, self->stats.total_char_count + 7);
  GdkRectangle location;
  gtk_text_view_get_iter_location(control, &look, &location);
  GdkRectangle visible_rect;
  gtk_text_view_get_visible_rect(control, &visible_rect);
  if (!gdk_rectangle_contains_point(&visible_rect, location.x + location.width,
                                    location.y + location.height)) {
    gtk_text_view_scroll_to_iter(control, &look, 0.1, TRUE, 0.5, 0.5);
  }

  // 打字/打词计数（一次变更多字=输入法整词上屏）
  if (self->stats.total_char_count - old_total > 1) {
    self->stats.type_word_count++;
    self->stats.word_char_count += self->stats.total_char_count - old_total;
  } else if (self->stats.total_char_count - old_total == 1) {
    self->stats.type_char_count++;
  }

  // 慢字词：距上次上屏超过阈值（打字钟，已扣暂停，删除重打的时间也计入
  // 该字的耗时）即记一条，文本取对照区对应切片
  if (self->stats.start_time > 0 && compared > 0) {
    gint64 elapsed_us = g_get_monotonic_time() - self->stats.start_time -
                        (gint64)self->stats.pause_duration;
    if (elapsed_us - self->last_commit_elapsed > SLOW_THRESHOLD_US) {
      GtkTextIter s_it, e_it;
      gtk_text_buffer_get_start_iter(control_buffer, &s_it);
      gtk_text_iter_forward_chars(&s_it, old_total);
      gtk_text_buffer_get_start_iter(control_buffer, &e_it);
      gtk_text_iter_forward_chars(&e_it, self->stats.total_char_count);
      TypewriterSlowItem *item = g_new(TypewriterSlowItem, 1);
      item->text = gtk_text_buffer_get_slice(control_buffer, &s_it, &e_it, TRUE);
      item->seconds = (elapsed_us - self->last_commit_elapsed) / 1000000.0;
      self->slow_items = g_list_append(self->slow_items, item);
    }
    self->last_commit_elapsed = elapsed_us;
  }

  // 更新进度条（文章为空时不更新，避免除零得到nan）
  if (self->stats.text_length > 0) {
    double progress =
        (double)self->stats.total_char_count / self->stats.text_length;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->progressbar),
                                  progress);
  }

  // 未开打（start_time为0，如空文章）不得结束，否则空文会立即结束并发出垃圾成绩
  if (gtk_text_iter_is_end(&c_it) && self->stats.start_time > 0) {
    self->state = TYPEWRITER_STATE_ENDED;
    g_signal_emit_by_name(self, "TYPE_ENDED");
  }
}
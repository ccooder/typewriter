//
// Created by king on 9/28/25.
//

#include "typewriter-input.h"
#include "typewriter-window.h"
#include "typewriter-ui.h"

void on_preedit_changed(GtkTextView *self, gchar *preedit,
                               gpointer user_data) {
  TypewriterWindow *win = TYPEWRITER_WINDOW(user_data);
  win->preedit_buffer = g_malloc(sizeof(char) * (strlen(preedit) + 1));
  strcpy(win->preedit_buffer, preedit);
}

static gboolean handle_special_keys(TypewriterWindow *self, guint keyval) {
    // 处理特殊按键逻辑
    if (self->state == TYPEWRITER_STATE_ENDED) {
        return TRUE;
    }
    
    if ((self->preedit_buffer == NULL || strlen(self->preedit_buffer) <= 0) &&
        (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
        if (self->state == TYPEWRITER_STATE_TYPING) {
            typewriter_pause(self);
        }
        return TRUE;
    }
    
    return FALSE;
}

gboolean is_special_key(guint keyval) {
  return keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter ||
    keyval == GDK_KEY_Escape || keyval == GDK_KEY_Shift_L ||
    keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Alt_L ||
    keyval == GDK_KEY_Super_L || keyval == GDK_KEY_Caps_Lock ||
    keyval == GDK_KEY_Tab;
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
            self->stats.pause_duration += (current_time - self->stats.pause_start_time);
            self->stats.pause_start_time = 0;
        }
        self->state = TYPEWRITER_STATE_TYPING;
        
        // 防止重复设置定时器
        if (self->update_timer_id == 0) {
            self->update_timer_id = g_timeout_add(REFRESH_INTERVAL, update_stat_ui, self);
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
gboolean on_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, 
                      GdkModifierType state, gpointer user_data) {
    TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
    
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

void on_follow_buffer_changed(GtkTextBuffer *follow_buffer,
                                     gpointer user_data) {
  // start_calculation_cb(user_data);
  TypewriterWindow *self = TYPEWRITER_WINDOW(user_data);
  if (self->state == TYPEWRITER_STATE_ENDED) {
    return;
  }
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(follow_buffer, &start);
  gtk_text_buffer_get_end_iter(follow_buffer, &end);
  gchar *follow_text =
      gtk_text_buffer_get_text(follow_buffer, &start, &end, FALSE);

  GtkTextView *control = GTK_TEXT_VIEW(self->control);
  GtkTextBuffer *control_buffer = gtk_text_view_get_buffer(control);
  gtk_text_buffer_get_start_iter(control_buffer, &start);
  gtk_text_buffer_get_end_iter(control_buffer, &end);
  gchar *control_text =
      gtk_text_buffer_get_text(control_buffer, &start, &end, FALSE);

  gtk_text_buffer_remove_all_tags(control_buffer, &start, &end);

  GtkTextTag *correct_tag = gtk_text_buffer_create_tag(
      control_buffer, NULL, "background", "green", NULL);
  GtkTextTag *incorrect_tag = gtk_text_buffer_create_tag(
      control_buffer, NULL, "background", "red", "foreground", "white", NULL);

  GtkTextIter char_iter;
  gtk_text_buffer_get_start_iter(follow_buffer, &char_iter);
  // 临时变量,ccc为正确打字数，tcc为总打字数
  guint ccc = 0;
  guint tcc = 0;
  int i;
  for (i = 0; !gtk_text_iter_is_end(&char_iter) && control_text[i] != '\0';) {
    tcc++;
    GtkTextIter start_iter = char_iter;
    gtk_text_iter_forward_char(&char_iter);

    // Get the character from the typed buffer and reference text
    char *typed_char =
        gtk_text_buffer_get_text(follow_buffer, &start_iter, &char_iter, FALSE);
    gunichar typed_unichar = g_utf8_get_char(typed_char);
    gunichar ref_unichar = g_utf8_get_char(&control_text[i]);

    // Compare and apply the correct tag
    GtkTextIter control_start_iter, control_end_iter;
    gtk_text_buffer_get_iter_at_offset(control_buffer, &control_start_iter,
                                       gtk_text_iter_get_offset(&start_iter));
    gtk_text_buffer_get_iter_at_offset(control_buffer, &control_end_iter,
                                       gtk_text_iter_get_offset(&char_iter));
    if (typed_unichar == ref_unichar) {
      gtk_text_buffer_apply_tag(control_buffer, correct_tag,
                                &control_start_iter, &control_end_iter);
      ccc++;
    } else {
      gtk_text_buffer_apply_tag(control_buffer, incorrect_tag,
                                &control_start_iter, &control_end_iter);
    }

    // Move to the next character in the reference text
    i += g_unichar_to_utf8(ref_unichar, NULL);
    g_free(typed_char);
  }
  gtk_text_buffer_get_iter_at_offset(control_buffer, &char_iter,
                                     gtk_text_iter_get_offset(&char_iter));
  gtk_text_iter_forward_chars(&char_iter, 7);
  GdkRectangle location;
  gtk_text_view_get_iter_location(control, &char_iter, &location);
  GdkRectangle visible_rect;
  gtk_text_view_get_visible_rect(control, &visible_rect);
  if (!gdk_rectangle_contains_point(&visible_rect, location.x + location.width,
                                    location.y + location.height)) {
    gtk_text_view_scroll_to_iter(control, &char_iter, 0.0, TRUE, 0.5, 0.5);
  }

  if (tcc - self->stats.total_char_count > 1) {
    self->stats.type_word_count++;
  }
  self->stats.reform_count +=
      self->stats.total_char_count > tcc ? self->stats.total_char_count - tcc : 0;
  self->stats.correct_char_count = ccc;
  self->stats.total_char_count = tcc;
  if (control_text[i] == '\0') {
    self->state = TYPEWRITER_STATE_ENDED;
    g_signal_emit_by_name(self, "TYPE_ENDED");
  }

  // Handle cases where one string is longer than the other
  // ... apply tags for remaining characters if needed
  g_free(follow_text);
  g_free(control_text);
}
/*
  Copyright 2007 Google Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <algorithm>
#include <cstdlib>
#include <string>
#include <cairo.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <ggadget/common.h>
#include <ggadget/math_utils.h>
#include <ggadget/logger.h>
#include <ggadget/texture.h>
#include <ggadget/main_loop_interface.h>
#include <ggadget/view.h>
#include <ggadget/gtk/cairo_graphics.h>
#include <ggadget/gtk/cairo_canvas.h>
#include <ggadget/gtk/cairo_font.h>
#include "gtk_edit_impl.h"
#include "gtk_edit_element.h"

#ifndef PANGO_VERSION_CHECK
#define PANGO_VERSION_CHECK(a,b,c) 0
#endif

namespace ggadget {
namespace gtk {

static const int kInnerBorderX = 2;
static const int kInnerBorderY = 1;
static const int kCursorBlinkTimeout = 400;
static const char *kDefaultFontFamily = "Sans";
static const int kDefaultFontSize = 10;
static const double kStrongCursorWidth = 1.2;
static const double kWeakCursorWidth = 1.0;
static const Color kStrongCursorColor(0, 0, 0);
static const Color kWeakCursorColor(0.5, 0.5, 0.5);
static const Color kDefaultTextColor(0, 0, 0);
static const Color kDefaultBackgroundColor(1, 1, 1);
static const Color kDefaultSelectionBackgroundColor(0.5, 0.5, 0.5);
static const Color kDefaultSelectionTextColor(1, 1, 1);

GtkEditImpl::GtkEditImpl(GtkEditElement *owner,
                         MainLoopInterface *main_loop,
                         int width, int height)
    : owner_(owner),
      main_loop_(main_loop),
      graphics_(owner->GetView()->GetGraphics()),
      canvas_(NULL),
      im_context_(NULL),
      cached_layout_(NULL),
      preedit_attrs_(NULL),
      width_(width),
      height_(height),
      cursor_(0),
      preedit_cursor_(0),
      selection_bound_(0),
      text_length_(0),
      scroll_offset_x_(0),
      scroll_offset_y_(0),
      refresh_timer_(0),
      cursor_blink_timer_(0),
      cursor_blink_status_(0),
      visible_(true),
      focused_(false),
      need_im_reset_(false),
      overwrite_(false),
      select_words_(false),
      select_lines_(false),
      button_(false),
      bold_(false),
      underline_(false),
      strikeout_(false),
      italic_(false),
      multiline_(false),
      wrap_(false),
      cursor_visible_(true),
      readonly_(false),
      content_modified_(false),
      font_family_(kDefaultFontFamily),
      font_size_(kDefaultFontSize),
      background_(new Texture(kDefaultBackgroundColor, 1)),
      text_color_(kDefaultTextColor) {
  ASSERT(main_loop_);
  ASSERT(graphics_);
  InitImContext();
}

GtkEditImpl::~GtkEditImpl() {
  if (canvas_)
    canvas_->Destroy();
  if (im_context_)
    g_object_unref(im_context_);
  delete background_;

  if (cursor_blink_timer_)
    main_loop_->RemoveWatch(cursor_blink_timer_);
  if (refresh_timer_)
    main_loop_->RemoveWatch(refresh_timer_);

  ResetPreedit();
  ResetLayout();
}

void GtkEditImpl::Draw(CanvasInterface *canvas) {
  CairoCanvas *edit_canvas = EnsureCanvas();

  if (content_modified_) {
    // If no background is set, then use transparent background.
    edit_canvas->ClearCanvas();
    edit_canvas->IntersectRectClipRegion(kInnerBorderX - 1,
                                         kInnerBorderY - 1,
                                         width_- kInnerBorderX + 1,
                                         height_ - kInnerBorderY + 1);
    DrawText(down_cast<CairoCanvas*>(edit_canvas));
  }

  if (background_)
    background_->Draw(canvas);
  canvas->DrawCanvas(0, 0, edit_canvas);
  DrawCursor(down_cast<CairoCanvas*>(canvas));

  content_modified_ = false;
}


void GtkEditImpl::FocusIn() {
  if (!focused_) {
    focused_ = true;
    if (!readonly_ && im_context_) {
      need_im_reset_ = true;
      gtk_im_context_focus_in(im_context_);
      UpdateIMCursorLocation();
    }
    content_modified_ = true;
    // Don't adjust scroll.
    QueueCursorBlink();
    QueueDraw();
  }
}

void GtkEditImpl::FocusOut() {
  if (focused_) {
    focused_ = false;
    if (!readonly_ && im_context_) {
      need_im_reset_ = true;
      gtk_im_context_focus_out(im_context_);
    }
    content_modified_ = true;
    // Don't adjust scroll.
    QueueCursorBlink();
    QueueDraw();
  }
}

void GtkEditImpl::SetWidth(int width) {
  if (width_ != width) {
    width_ = width;
    if (width_ <= kInnerBorderX * 2)
      width_ = kInnerBorderX * 2 + 1;
    QueueRefresh(true);
  }
}

int GtkEditImpl::GetWidth() {
  return width_;
}

void GtkEditImpl::SetHeight(int height) {
  if (height_ != height) {
    height_ = height;
    if (height_ <= kInnerBorderY * 2)
      height_ = kInnerBorderY * 2 + 1;
    QueueRefresh(true);
  }
}

int GtkEditImpl::GetHeight() {
  return height_;
}

void GtkEditImpl::GetSizeRequest(int *width, int *height) {
  int layout_width, layout_height;
  PangoLayout *layout = EnsureLayout();

  pango_layout_get_pixel_size(layout, &layout_width, &layout_height);

  layout_width += kInnerBorderX * 2;
  layout_height += kInnerBorderY * 2;

  if (wrap_ && layout_width < width_)
    layout_width = width_;

  if (width)
    *width = layout_width;
  if (height)
    *height = layout_height;
}

void GtkEditImpl::SetBold(bool bold) {
  if (bold_ != bold) {
    bold_ = bold;
    QueueRefresh(true);
  }
}

bool GtkEditImpl::IsBold() {
  return bold_;
}

void GtkEditImpl::SetItalic(bool italic) {
  if (italic_ != italic) {
    italic_ = italic;
    QueueRefresh(true);
  }
}

bool GtkEditImpl::IsItalic() {
  return italic_;
}

void GtkEditImpl::SetStrikeout(bool strikeout) {
  if (strikeout_ != strikeout) {
    strikeout_ = strikeout;
    QueueRefresh(true);
  }
}

bool GtkEditImpl::IsStrikeout() {
  return strikeout_;
}

void GtkEditImpl::SetUnderline(bool underline) {
  if (underline_ != underline) {
    underline_ = underline;
    QueueRefresh(true);
  }
}

bool GtkEditImpl::IsUnderline() {
  return underline_;
}

void GtkEditImpl::SetMultiline(bool multiline) {
  if (multiline_ != multiline) {
    multiline_ = multiline;
    QueueRefresh(true);
  }
}

bool GtkEditImpl::IsMultiline() {
  return multiline_;
}

void GtkEditImpl::SetWordWrap(bool wrap) {
  if (wrap_ != wrap) {
    wrap_ = wrap;
    QueueRefresh(true);
  }
}

bool GtkEditImpl::IsWordWrap() {
  return wrap_;
}

void GtkEditImpl::SetReadOnly(bool readonly) {
  if (readonly_ != readonly) {
    readonly_ = readonly;
    if (readonly_) {
      if (im_context_) {
        if (focused_)
          gtk_im_context_focus_out(im_context_);
        g_object_unref(im_context_);
        im_context_ = NULL;
      }
      ResetPreedit();
    } else {
      ResetPreedit();
      InitImContext();
      if (focused_)
        gtk_im_context_focus_in(im_context_);
    }
  }
  QueueRefresh(false);
}

bool GtkEditImpl::IsReadOnly() {
  return readonly_;
}

void GtkEditImpl::SetText(const char* text) {
  const char *end = NULL;
  g_utf8_validate(text, -1, &end);

  if (text && *text && end > text) {
    std::string txt(text, end - text);
    if (txt == text_) {
      return; // prevent some redraws
    }

    text_.assign(txt);
    text_length_ = g_utf8_strlen(text_.c_str(), text_.length());
  } else {
    text_.clear();
    text_length_ = 0;
  }
  cursor_ = 0;
  selection_bound_ = 0;
  need_im_reset_ = true;
  ResetImContext();
  QueueRefresh(true);
  owner_->FireOnChangeEvent();
}

std::string GtkEditImpl::GetText() {
  return text_;
}

void GtkEditImpl::SetBackground(Texture *background) {
  if (background_)
    delete background_;
  background_ = background;
  QueueRefresh(false);
}

const Texture *GtkEditImpl::GetBackground() {
  return background_;
}

void GtkEditImpl::SetTextColor(const Color &color) {
  text_color_ = color;
  content_modified_ = true;
  QueueRefresh(false);
}

Color GtkEditImpl::GetTextColor() {
  return text_color_;
}

void GtkEditImpl::SetFontFamily(const char *font) {
  std::string new_font((font && *font) ? font : kDefaultFontFamily);
  if (font_family_ != new_font) {
    font_family_ = new_font;
    QueueRefresh(true);
  }
}

std::string GtkEditImpl::GetFontFamily() {
  return font_family_;
}

void GtkEditImpl::SetFontSize(int size) {
  if (font_size_ != size) {
    font_size_ = size;
    QueueRefresh(true);
  }
}

int GtkEditImpl::GetFontSize() {
  return font_size_;
}

void GtkEditImpl::SetPasswordChar(const char *c) {
  if (c == NULL || *c == 0 || !IsLegalUTF8Char(c, GetUTF8CharLength(c))) {
    SetVisibility(true);
    password_char_.clear();
  } else {
    SetVisibility(false);
    password_char_.assign(c, GetUTF8CharLength(c));
  }
  QueueRefresh(true);
}

std::string GtkEditImpl::GetPasswordChar() {
  return password_char_;
}

bool GtkEditImpl::IsScrollBarRequired() {
  int request_height;
  GetSizeRequest(NULL, &request_height);
  return height_ >= request_height;
}

void GtkEditImpl::GetScrollBarInfo(int *range, int *line_step,
                               int *page_step, int *cur_pos) {
  PangoLayout *layout = EnsureLayout();
  int nlines = pango_layout_get_line_count(layout);

  // Only enable scrolling when there are more than one lines.
  if (nlines > 1) {
    int request_height;
    int real_height = height_ - kInnerBorderY * 2;
    pango_layout_get_pixel_size(layout, NULL, &request_height);
    if (range)
      *range = (request_height > real_height? (request_height - real_height) : 0);
    if (line_step) {
      *line_step = request_height / nlines;
      if (*line_step == 0) *line_step = 1;
    }
    if (page_step)
      *page_step = real_height;
    if (cur_pos)
      *cur_pos = - scroll_offset_y_;
  } else {
    if (range) *range = 0;
    if (line_step) *line_step = 0;
    if (page_step) *page_step = 0;
    if (cur_pos) *cur_pos = 0;
  }
}

void GtkEditImpl::ScrollTo(int position) {
  int request_height;
  int real_height = height_ - kInnerBorderY * 2;
  PangoLayout *layout = EnsureLayout();
  pango_layout_get_pixel_size(layout, NULL, &request_height);

  if (request_height > real_height) {
    if (position < 0)
      position = 0;
    else if (position >= request_height - real_height)
      position = request_height - real_height - 1;

    scroll_offset_y_ = -position;
    content_modified_ = true;
    QueueDraw();
  }
}

void GtkEditImpl::MarkRedraw() {
  content_modified_ = true;
}

EventResult GtkEditImpl::OnMouseEvent(const MouseEvent &event) {
  // Only handle mouse events with left button down.
  if (event.GetButton() != MouseEvent::BUTTON_LEFT)
    return EVENT_RESULT_UNHANDLED;

  Event::Type type = event.GetType();
  int x = static_cast<int>(round(event.GetX())) -
            kInnerBorderX - scroll_offset_x_;
  int y = static_cast<int>(round(event.GetY())) -
            kInnerBorderY - scroll_offset_y_;
  int offset = XYToOffset(x, y);
  int sel_start, sel_end;
  GetSelectionBounds(&sel_start, &sel_end);

  ResetImContext();
  if (type == Event::EVENT_MOUSE_DOWN) {
    if (event.GetModifier() & Event::MOD_SHIFT) {
      // If current click position is inside the selection range, then just
      // cancel the selection.
      if (offset > sel_start && offset < sel_end)
        SetCursor(offset);
      else if (offset <= sel_start)
        SetSelectionBounds(sel_end, offset);
      else if (offset >= sel_end)
        SetSelectionBounds(sel_start, offset);
    } else {
      SetCursor(offset);
    }
  } else if (type == Event::EVENT_MOUSE_DBLCLICK) {
    if (event.GetModifier() & Event::MOD_SHIFT)
      SelectLine();
    else
      SelectWord();
  } else if (type == Event::EVENT_MOUSE_MOVE) {
    SetSelectionBounds(selection_bound_, offset);
  }
  QueueRefresh(false);
  return EVENT_RESULT_HANDLED;
}

EventResult GtkEditImpl::OnKeyEvent(const KeyboardEvent &event) {
  GdkEventKey *gdk_event = static_cast<GdkEventKey *>(event.GetOriginalEvent());
  ASSERT(gdk_event);

  Event::Type type = event.GetType();
  // Cause the cursor to stop blinking for a while.
  cursor_blink_status_ = 4;

  if (!readonly_ && im_context_ && type != Event::EVENT_KEY_PRESS &&
      gtk_im_context_filter_keypress(im_context_, gdk_event)) {
    need_im_reset_ = true;
    QueueRefresh(false);
    return EVENT_RESULT_HANDLED;
  }

  if (type == Event::EVENT_KEY_UP)
    return EVENT_RESULT_UNHANDLED;

  unsigned int keyval = gdk_event->keyval;
  bool shift = (gdk_event->state & GDK_SHIFT_MASK);
  bool ctrl = (gdk_event->state & GDK_CONTROL_MASK);

  DLOG("GtkEditImpl::OnKeyEvent(%d, shift:%d ctrl:%d)", keyval, shift, ctrl);

  if (type == Event::EVENT_KEY_DOWN) {
    if (keyval == GDK_Left || keyval == GDK_KP_Left) {
      if (!ctrl) MoveCursor(VISUALLY, -1, shift);
      else MoveCursor(WORDS, -1, shift);
    } else if (keyval == GDK_Right || keyval == GDK_KP_Right) {
      if (!ctrl) MoveCursor(VISUALLY, 1, shift);
      else MoveCursor(WORDS, 1, shift);
    } else if (keyval == GDK_Up || keyval == GDK_KP_Up) {
      MoveCursor(DISPLAY_LINES, -1, shift);
    } else if (keyval == GDK_Down || keyval == GDK_KP_Down) {
      MoveCursor(DISPLAY_LINES, 1, shift);
    } else if (keyval == GDK_Home || keyval == GDK_KP_Home) {
      if (!ctrl) MoveCursor(DISPLAY_LINE_ENDS, -1, shift);
      else MoveCursor(BUFFER, -1, shift);
    } else if (keyval == GDK_End || keyval == GDK_KP_End) {
      if (!ctrl) MoveCursor(DISPLAY_LINE_ENDS, 1, shift);
      else MoveCursor(BUFFER, 1, shift);
    } else if (keyval == GDK_Page_Up || keyval == GDK_KP_Page_Up) {
      if (!ctrl) MoveCursor(PAGES, -1, shift);
      else MoveCursor(BUFFER, -1, shift);
    } else if (keyval == GDK_Page_Down || keyval == GDK_KP_Page_Down) {
      if (!ctrl) MoveCursor(PAGES, 1, shift);
      else MoveCursor(BUFFER, 1, shift);
    } else if ((keyval == GDK_x && ctrl && !shift) ||
               (keyval == GDK_Delete && shift && !ctrl)) {
      CutClipboard();
    } else if ((keyval == GDK_c && ctrl && !shift) ||
               (keyval == GDK_Insert && ctrl && !shift)) {
      CopyClipboard();
    } else if ((keyval == GDK_v && ctrl && !shift) ||
               (keyval == GDK_Insert && shift && !ctrl)) {
      PasteClipboard();
    } else if (keyval == GDK_BackSpace) {
      BackSpace();
    } else if (keyval == GDK_Delete && !shift) {
      Delete();
    } else if (keyval == GDK_Insert && !shift && !ctrl) {
      ToggleOverwrite();
    } else {
      return EVENT_RESULT_UNHANDLED;
    }
  } else { // EVENT_KEY_PRESS
    if (keyval == GDK_Return || keyval == GDK_KP_Enter) {
      // If multiline_ is unset, just ignore new_line.
      if (multiline_) EnterText("\n");
    } else if (keyval == GDK_Tab) {
      // The Tab key will likely be consumed by input method.
      EnterText("\t");
    } else {
      return EVENT_RESULT_UNHANDLED;
    }
  }

  QueueRefresh(false);
  return EVENT_RESULT_HANDLED;
}

//private =================================================================

void GtkEditImpl::ResetLayout() {
  if (cached_layout_) {
    g_object_unref(cached_layout_);
    cached_layout_ = NULL;
    content_modified_ = true;
  }
}

PangoLayout* GtkEditImpl::EnsureLayout() {
  if (!cached_layout_) {
    cached_layout_ = CreateLayout();
  }
  return cached_layout_;
}

PangoLayout* GtkEditImpl::CreateLayout() {
  // Creates the pango layout with a temporary canvas that is not zoomed.
  CairoCanvas *canvas = new CairoCanvas(1.0, 1, 1, CAIRO_FORMAT_ARGB32);
  PangoLayout *layout = pango_cairo_create_layout(canvas->GetContext());
  canvas->Destroy();
  PangoAttrList *tmp_attrs = pango_attr_list_new();
  std::string tmp_string;

  /* Set necessary parameters */
  if (wrap_) {
    pango_layout_set_width(layout, (width_ - kInnerBorderX * 2) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
  } else {
    pango_layout_set_width(layout, -1);
  }

  pango_layout_set_single_paragraph_mode(layout, !multiline_);

  if (preedit_.length()) {
    size_t cursor_index =
        g_utf8_offset_to_pointer(text_.c_str(), cursor_) - text_.c_str();
    size_t preedit_length = preedit_.length();
    if (visible_) {
      tmp_string = text_;
      tmp_string.insert(cursor_index, preedit_);
    } else {
      size_t nchars = g_utf8_strlen(text_.c_str(), text_.length());
      size_t preedit_nchars =
          g_utf8_strlen(preedit_.c_str(), preedit_.length());
      nchars += preedit_nchars;
      tmp_string.reserve(password_char_.length() * nchars);
      for (size_t i = 0; i < nchars; ++i)
        tmp_string.append(password_char_);
      /* Fix cursor index and preedit_length */
      cursor_index = g_utf8_offset_to_pointer(tmp_string.c_str(), cursor_)
                      - tmp_string.c_str();
      preedit_length = preedit_nchars * password_char_.length();
    }
    if (preedit_attrs_)
      pango_attr_list_splice(tmp_attrs, preedit_attrs_,
                             cursor_index, preedit_length);
  } else {
    if(visible_) {
      tmp_string = text_;
    } else {
      size_t nchars = g_utf8_strlen(text_.c_str(), text_.length());
      tmp_string.reserve(password_char_.length() * nchars);
      for (size_t i = 0; i < nchars; ++i)
        tmp_string.append(password_char_);
    }
  }

  pango_layout_set_text(layout, tmp_string.c_str(), tmp_string.length());

  /* Set necessary attributes */
  PangoAttribute *attr;
  if (underline_) {
    attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
    attr->start_index = 0;
    attr->end_index = tmp_string.length();
    pango_attr_list_insert(tmp_attrs, attr);
  }
  if (strikeout_) {
    attr = pango_attr_strikethrough_new(TRUE);
    attr->start_index = 0;
    attr->end_index = tmp_string.length();
    pango_attr_list_insert(tmp_attrs, attr);
  }
  /* Set font desc */
  {
    /* safe to down_cast here, because we know the actual implementation. */
    CairoFont *font = down_cast<CairoFont*>(
        graphics_->NewFont(font_family_.c_str(), font_size_,
          italic_ ? FontInterface::STYLE_ITALIC : FontInterface::STYLE_NORMAL,
          bold_ ? FontInterface::WEIGHT_BOLD : FontInterface::WEIGHT_NORMAL));
    ASSERT(font);
    attr = pango_attr_font_desc_new(font->GetFontDescription());
    attr->start_index = 0;
    attr->end_index = tmp_string.length();
    pango_attr_list_insert(tmp_attrs, attr);
    font->Destroy();
  }
  pango_layout_set_attributes(layout, tmp_attrs);
  pango_attr_list_unref(tmp_attrs);

  /* Set alignment according to text direction. Only set layout's alignment
   * when it's not wrapped and in single line mode.
   */
  if (!wrap_ && pango_layout_get_line_count(layout) <= 1) {
    PangoDirection dir;
    if (visible_)
      dir = pango_find_base_dir(tmp_string.c_str(), tmp_string.length());
    else
      dir = PANGO_DIRECTION_NEUTRAL;

    if (dir == PANGO_DIRECTION_NEUTRAL) {
      GtkWidget *widget = GetWidgetAndCursorLocation(NULL);
      if (widget && gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)
        dir = PANGO_DIRECTION_RTL;
      else
        dir = PANGO_DIRECTION_LTR;
    }
    pango_layout_set_alignment(layout,
        dir == PANGO_DIRECTION_RTL ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);
  }

  return layout;
}

CairoCanvas* GtkEditImpl::EnsureCanvas() {
  if (canvas_) {
    if (width_ == static_cast<int>(canvas_->GetWidth()) &&
        height_ == static_cast<int>(canvas_->GetHeight()))
      return canvas_;
    else {
      DLOG("GtkEdit: Recreate canvas");
      canvas_->Destroy();
      canvas_ = NULL;
    }
  }
  canvas_ = down_cast<CairoCanvas*>(graphics_->NewCanvas(width_, height_));
  ASSERT(canvas_);
  return canvas_;
}

void GtkEditImpl::AdjustScroll() {
  int old_offset_x = scroll_offset_x_;
  int old_offset_y = scroll_offset_y_;
  int display_width = width_ - kInnerBorderX * 2;
  int display_height = height_ - kInnerBorderY * 2;

  PangoLayout *layout = EnsureLayout();
  int text_width, text_height;
  pango_layout_get_pixel_size(layout, &text_width, &text_height);

  int strong_x, strong_y, strong_height;
  int weak_x, weak_y, weak_height;
  GetCursorLocationInLayout(&strong_x, &strong_y, &strong_height,
                            &weak_x, &weak_y, &weak_height);

  if (display_width > text_width) {
    PangoAlignment align = pango_layout_get_alignment(layout);
    if (align == PANGO_ALIGN_RIGHT)
      scroll_offset_x_ = display_width - text_width;
    else if (align == PANGO_ALIGN_LEFT)
      scroll_offset_x_ = 0;
    else
      scroll_offset_x_ = (display_width - text_width) / 2;
  } else {
    if (scroll_offset_x_ + strong_x < 0)
      scroll_offset_x_ = -strong_x;
    else if (scroll_offset_x_ + strong_x > display_width)
      scroll_offset_x_ = display_width - strong_x;

    if (std::abs(weak_x - strong_x) < display_width) {
      if (scroll_offset_x_ + weak_x < 0)
        scroll_offset_x_ = - weak_x;
      else if (scroll_offset_x_ + weak_x > display_width)
        scroll_offset_x_ = display_width - weak_x;
    }
  }

  if (display_height > text_height) {
    scroll_offset_y_ = 0;
  } else {
    if (scroll_offset_y_ + strong_y + strong_height > display_height)
      scroll_offset_y_ = display_height - strong_y - strong_height;
    if (scroll_offset_y_ + strong_y < 0)
      scroll_offset_y_ = -strong_y;
  }

  if (old_offset_x != scroll_offset_x_ || old_offset_y != scroll_offset_y_)
    content_modified_ = true;
}

void GtkEditImpl::QueueRefresh(bool relayout) {
  if (relayout) ResetLayout();
  QueueCursorBlink();

  if (!refresh_timer_) {
    refresh_timer_ = main_loop_->AddTimeoutWatch(0,
      new WatchCallbackSlot(NewSlot(this, &GtkEditImpl::RefreshCallback)));
  }
}

bool GtkEditImpl::RefreshCallback(int timer_id) {
  refresh_timer_ = 0;
  AdjustScroll();
  QueueDraw();
  return false;
}

void GtkEditImpl::QueueDraw() {
  owner_->QueueDraw();
}

void GtkEditImpl::ResetImContext() {
  if (need_im_reset_) {
    need_im_reset_ = false;
    if (im_context_)
      gtk_im_context_reset(im_context_);
    ResetPreedit();
  }
}

void GtkEditImpl::ResetPreedit() {
  // Reset layout if there were some content in preedit string
  if (preedit_.length())
    ResetLayout();

  preedit_.clear();
  preedit_cursor_ = 0;
  if (preedit_attrs_) {
    pango_attr_list_unref(preedit_attrs_);
    preedit_attrs_ = NULL;
  }
}

void GtkEditImpl::InitImContext() {
  if (im_context_)
    g_object_unref(im_context_);

  if (visible_)
    im_context_ = gtk_im_multicontext_new();
  else
    im_context_ = gtk_im_context_simple_new();

  gtk_im_context_set_use_preedit(im_context_, TRUE);

  g_signal_connect(im_context_, "commit",
      G_CALLBACK(CommitCallback), this);
  g_signal_connect(im_context_, "retrieve-surrounding",
      G_CALLBACK(RetrieveSurroundingCallback), this);
  g_signal_connect(im_context_, "delete-surrounding",
      G_CALLBACK(DeleteSurroundingCallback), this);
  g_signal_connect(im_context_, "preedit-start",
      G_CALLBACK(PreeditStartCallback), this);
  g_signal_connect(im_context_, "preedit-changed",
      G_CALLBACK(PreeditChangedCallback), this);
  g_signal_connect(im_context_, "preedit-end",
      G_CALLBACK(PreeditEndCallback), this);
}

void GtkEditImpl::SetVisibility(bool visible) {
  if (visible_ != visible) {
    visible_ = visible;

    if (!readonly_) {
      if (focused_)
        gtk_im_context_focus_out(im_context_);

      InitImContext();
      ResetPreedit();

      if (focused_)
        gtk_im_context_focus_in(im_context_);
    }

    ResetLayout();
  }
}

bool GtkEditImpl::IsCursorBlinking() {
  return (focused_ && !readonly_ && selection_bound_ == cursor_);
}

void GtkEditImpl::QueueCursorBlink() {
  if (IsCursorBlinking()) {
    if (!cursor_blink_timer_)
      cursor_blink_timer_ = main_loop_->AddTimeoutWatch(kCursorBlinkTimeout,
          new WatchCallbackSlot(NewSlot(this, &GtkEditImpl::CursorBlinkCallback)));
  } else {
    if (cursor_blink_timer_) {
      main_loop_->RemoveWatch(cursor_blink_timer_);
      cursor_blink_timer_ = 0;
    }

    cursor_visible_ = true;
  }
}

bool GtkEditImpl::CursorBlinkCallback(int timer_id) {
  -- cursor_blink_status_;
  if (cursor_blink_status_ < 0)
    cursor_blink_status_ = 2;

  if (cursor_blink_status_ > 0)
    ShowCursor();
  else
    HideCursor();

  return true;
}

void GtkEditImpl::ShowCursor() {
  if (!cursor_visible_) {
    cursor_visible_ = true;
    if (focused_ && !readonly_)
      QueueDraw();
  }
}

void GtkEditImpl::HideCursor() {
  if (cursor_visible_) {
    cursor_visible_ = false;
    if (focused_ && !readonly_)
      QueueDraw();
  }
}

void GtkEditImpl::DrawCursor(CairoCanvas *canvas) {
  if (!cursor_visible_ || !focused_) return;

  int strong_x, strong_y, strong_height;
  int weak_x, weak_y, weak_height;
  GetCursorLocationInLayout(&strong_x, &strong_y, &strong_height,
                            &weak_x, &weak_y, &weak_height);

  // Draw strong cursor.
  // TODO: Is the color ok?
  canvas->DrawLine(strong_x + kInnerBorderX + scroll_offset_x_,
                   strong_y + kInnerBorderY + scroll_offset_y_,
                   strong_x + kInnerBorderX + scroll_offset_x_,
                   strong_y + strong_height + kInnerBorderY + scroll_offset_y_,
                   kStrongCursorWidth, kStrongCursorColor);
  // Draw a small arror towards weak cursor
  if (strong_x > weak_x) {
    canvas->DrawLine(
        strong_x + kInnerBorderX + scroll_offset_x_ - kStrongCursorWidth * 2.5,
        strong_y + kInnerBorderY + scroll_offset_y_ + kStrongCursorWidth,
        strong_x + kInnerBorderX + scroll_offset_x_,
        strong_y + kInnerBorderY + scroll_offset_y_ + kStrongCursorWidth,
        kStrongCursorWidth, kStrongCursorColor);
  } else if (strong_x < weak_x) {
    canvas->DrawLine(
        strong_x + kInnerBorderX + scroll_offset_x_,
        strong_y + kInnerBorderY + scroll_offset_y_ + kStrongCursorWidth,
        strong_x + kInnerBorderX + scroll_offset_x_ + kStrongCursorWidth * 2.5,
        strong_y + kInnerBorderY + scroll_offset_y_ + kStrongCursorWidth,
        kStrongCursorWidth, kStrongCursorColor);
  }

  if (strong_x != weak_x ) {
    // Draw weak cursor.
    // TODO: Is the color ok?
    canvas->DrawLine(weak_x + kInnerBorderX + scroll_offset_x_,
                     weak_y + kInnerBorderY + scroll_offset_y_,
                     weak_x + kInnerBorderX + scroll_offset_x_,
                     weak_y + weak_height + kInnerBorderY + scroll_offset_y_,
                     kWeakCursorWidth, kWeakCursorColor);
    // Draw a small arror towards strong cursor
    if (weak_x > strong_x) {
      canvas->DrawLine(
          weak_x + kInnerBorderX + scroll_offset_x_ - kWeakCursorWidth * 2.5,
          weak_y + kInnerBorderY + scroll_offset_y_ + kWeakCursorWidth,
          weak_x + kInnerBorderX + scroll_offset_x_,
          weak_y + kInnerBorderY + scroll_offset_y_ + kWeakCursorWidth,
          kWeakCursorWidth, kWeakCursorColor);
    } else {
      canvas->DrawLine(
          weak_x + kInnerBorderX + scroll_offset_x_,
          weak_y + kInnerBorderY + scroll_offset_y_ + kWeakCursorWidth,
          weak_x + kInnerBorderX + scroll_offset_x_ + kWeakCursorWidth * 2.5,
          weak_y + kInnerBorderY + scroll_offset_y_ + kWeakCursorWidth,
          kWeakCursorWidth, kWeakCursorColor);
    }
  }
}

void GtkEditImpl::DrawText(CairoCanvas *canvas) {
  PangoLayout *layout = EnsureLayout();

  cairo_save(canvas->GetContext());
  cairo_set_source_rgb(canvas->GetContext(),
                       text_color_.red,
                       text_color_.green,
                       text_color_.blue);
  cairo_move_to(canvas->GetContext(),
                scroll_offset_x_ + kInnerBorderX,
                scroll_offset_y_ + kInnerBorderY);
  pango_cairo_show_layout(canvas->GetContext(), layout);

  // Draw selection background.
  // Selection in a single line may be not continual, so we use pango to
  // get the x-ranges of each selection range in one line, and draw them
  // separately.
  int start_off, end_off;
  if (GetSelectionBounds(&start_off, &end_off)) {
    const char *text = pango_layout_get_text(layout);
    PangoRectangle line_extents, pos;
    int start_index, end_index;
    int draw_start, draw_end;
    int *ranges;
    int n_ranges;
    int n_lines = pango_layout_get_line_count(layout);

    // If there is preedit string before the cursor, then adjust the start_off
    // and end_off to skip the preedit string.
    if (start_off == cursor_ && preedit_.length()) {
      int len = g_utf8_strlen(preedit_.c_str(), preedit_.length());
      start_off += len;
      end_off += len;
    }

    start_index =
      g_utf8_offset_to_pointer(text, start_off) - text;
    end_index =
      g_utf8_offset_to_pointer(text, end_off) - text;
    for(int line_index = 0; line_index < n_lines; ++line_index) {
#if PANGO_VERSION_CHECK(1,16,0)
      PangoLayoutLine *line = pango_layout_get_line_readonly(layout, line_index);
#else
      PangoLayoutLine *line = pango_layout_get_line(layout, line_index);
#endif
      if (line->start_index + line->length < start_index)
        continue;
      if (end_index < line->start_index)
        break;
      draw_start = std::max(start_index, line->start_index);
      draw_end = std::min(end_index, line->start_index + line->length);
      pango_layout_line_get_x_ranges(line, draw_start, draw_end,
                                     &ranges, &n_ranges);
      pango_layout_line_get_pixel_extents(line, NULL, &line_extents);
      pango_layout_index_to_pos(layout, line->start_index,  &pos);
      for(int i = 0; i < n_ranges; ++i) {
        cairo_rectangle(
            canvas->GetContext(),
            kInnerBorderX + scroll_offset_x_ + PANGO_PIXELS(ranges[i * 2]),
            kInnerBorderY + scroll_offset_y_ + PANGO_PIXELS(pos.y),
            PANGO_PIXELS(ranges[i * 2 + 1] - ranges[i * 2]),
            line_extents.height);
      }
      g_free(ranges);
    }
    cairo_clip(canvas->GetContext());

    Color selection_color = GetSelectionBackgroundColor();
    Color text_color = GetSelectionTextColor();

    cairo_set_source_rgb(canvas->GetContext(),
                         selection_color.red,
                         selection_color.green,
                         selection_color.blue);
    cairo_paint(canvas->GetContext());

    cairo_move_to(canvas->GetContext(),
                  scroll_offset_x_ + kInnerBorderX,
                  scroll_offset_y_ + kInnerBorderY);
    cairo_set_source_rgb(canvas->GetContext(),
                         text_color.red,
                         text_color.green,
                         text_color.blue);
    pango_cairo_show_layout(canvas->GetContext(), layout);
  }
  cairo_restore(canvas->GetContext());
}

void GtkEditImpl::MoveCursor(MovementStep step, int count, bool extend_selection) {
  ResetImContext();
  int new_pos(0);
  // Clear selection first if not extend it.
  if (cursor_ != selection_bound_ && !extend_selection)
    SetCursor(cursor_);

  // Calculate the new offset after motion.
  switch(step) {
    case VISUALLY:
      new_pos = MoveVisually(cursor_, count);
      break;
    case WORDS:
      new_pos = MoveWords(cursor_, count);
      break;
    case DISPLAY_LINES:
      new_pos = MoveDisplayLines(cursor_, count);
      break;
    case DISPLAY_LINE_ENDS:
      new_pos = MoveLineEnds(cursor_, count);
      break;
    case PAGES:
      new_pos = MovePages(cursor_, count);
      break;
    case BUFFER:
      ASSERT(count == -1 || count == 1);
      new_pos = (count == -1 ? 0 : text_length_);
      break;
  }

  if (extend_selection)
    SetSelectionBounds(selection_bound_, new_pos);
  else
    SetCursor(new_pos);

  QueueRefresh(false);
}

int GtkEditImpl::MoveVisually(int current_pos, int count) {
  ASSERT(current_pos >= 0 && current_pos <= text_length_);
  ASSERT(count != 0);
  PangoLayout *layout = EnsureLayout();
  const char *text = pango_layout_get_text(layout);
  int index = g_utf8_offset_to_pointer(text, current_pos) - text;
  int new_index = 0;
  int new_trailing = 0;
  while (count != 0) {
    if (count > 0) {
      --count;
      pango_layout_move_cursor_visually(layout, true, index, 0, 1,
                                        &new_index, &new_trailing);
    } else if (count < 0) {
      ++count;
      pango_layout_move_cursor_visually(layout, true, index, 0, -1,
                                        &new_index, &new_trailing);
    }
    index = new_index;
    if (index < 0 || index == G_MAXINT)
      return current_pos;
    index = g_utf8_offset_to_pointer(text + index, new_trailing) - text;
  }
  current_pos = g_utf8_pointer_to_offset(text, text + index);
  return current_pos;
}

int GtkEditImpl::MoveWords(int current_pos, int count) {
  ASSERT(current_pos >= 0 && current_pos <= text_length_);
  ASSERT(count != 0);

  if (!visible_) {
    current_pos = (count > 0 ? text_length_ : 0);
  } else {
    // The cursor movement direction shall be determined by the direction of
    // current text line.
    PangoLayout *layout = EnsureLayout();
    int n_log_attrs;
    PangoLogAttr *log_attrs;
    pango_layout_get_log_attrs (layout, &log_attrs, &n_log_attrs);
    const char *text = pango_layout_get_text(layout);
    int index = g_utf8_offset_to_pointer(text, current_pos) - text;
    int line_index;
    pango_layout_index_to_line_x(layout, index, FALSE, &line_index, NULL);

    // Weird bug: line_index here may be >= than line count?
    int line_count = pango_layout_get_line_count(layout);
    if (line_index >= line_count) {
      line_index = line_count - 1;
    }

#if PANGO_VERSION_CHECK(1,16,0)
    PangoLayoutLine *line = pango_layout_get_line_readonly(layout, line_index);
#else
    PangoLayoutLine *line = pango_layout_get_line(layout, line_index);
#endif        
    bool rtl = (line->resolved_dir == PANGO_DIRECTION_RTL);
    while (count != 0) {
      if (((rtl && count < 0) || (!rtl && count > 0)) &&
          current_pos < text_length_) {
        while (++current_pos < text_length_ &&
               !log_attrs[current_pos].is_word_start &&
               !log_attrs[current_pos].is_word_end);
      } else if (((rtl && count > 0) || (!rtl && count < 0)) &&
                 current_pos > 0) {
        while (--current_pos > 0 &&
               !log_attrs[current_pos].is_word_start &&
               !log_attrs[current_pos].is_word_end);
      } else {
        break;
      }
      if (count > 0) --count;
      else ++count;
    }
  }
  return current_pos;
}

int GtkEditImpl::MoveDisplayLines(int current_pos, int count) {
  ASSERT(current_pos >= 0 && current_pos <= text_length_);

  PangoLayout *layout = EnsureLayout();
  const char *text = pango_layout_get_text(layout);
  int index = g_utf8_offset_to_pointer(text, current_pos) - text;
  int n_lines = pango_layout_get_line_count(layout);
  int line_index = 0;
  int x_off = 0;
  PangoRectangle rect;

  // Find the current cursor X position in layout
  pango_layout_index_to_line_x(layout, index, FALSE, &line_index, &x_off);

  // Weird bug: line_index here may be >= than line count?
  if (line_index >= n_lines) {
    line_index = n_lines - 1;
  }

  pango_layout_get_cursor_pos(layout, index, &rect, NULL);
  x_off = rect.x;

  line_index += count;

  if (line_index < 0) {
    return 0;
  } else if (line_index >= n_lines) {
    return text_length_;
  } else {
    int trailing;
#if PANGO_VERSION_CHECK(1,16,0)
    PangoLayoutLine *line = pango_layout_get_line_readonly(layout, line_index);
#else
    PangoLayoutLine *line = pango_layout_get_line(layout, line_index);
#endif
    // Find out the cursor x offset related to the new line position.
    if (line->resolved_dir == PANGO_DIRECTION_RTL) {
      pango_layout_get_cursor_pos(layout, line->start_index + line->length,
                                  &rect, NULL);
    } else {
      pango_layout_get_cursor_pos(layout, line->start_index, &rect, NULL);
    }

    // rect.x is the left edge position of the line in the layout
    x_off -= rect.x;
    if (x_off < 0) x_off = 0;
    pango_layout_line_x_to_index(line, x_off, &index, &trailing);
    current_pos = g_utf8_pointer_to_offset(text, text + index);
    return current_pos + trailing;
  }
}

int GtkEditImpl::MovePages(int current_pos, int count) {
  ASSERT(current_pos >= 0 && current_pos <= text_length_);
  // Transfer pages to display lines.
  PangoLayout *layout = EnsureLayout();
  int layout_height;
  pango_layout_get_pixel_size(layout, NULL, &layout_height);
  int n_lines = pango_layout_get_line_count(layout);
  int line_height = layout_height / n_lines;
  int page_lines = (height_ - kInnerBorderY * 2) / line_height;
  return MoveDisplayLines(current_pos, count * page_lines);
}

int GtkEditImpl::MoveLineEnds(int current_pos, int count) {
  ASSERT(current_pos >= 0 && current_pos <= text_length_);
  ASSERT(count < 0 || count > 0);

  PangoLayout *layout = EnsureLayout();
  const char *text = pango_layout_get_text(layout);
  int index = g_utf8_offset_to_pointer(text, current_pos) - text;
  int line_index = 0;

  // Find current line
  pango_layout_index_to_line_x(layout, index, FALSE, &line_index, NULL);

  // Weird bug: line_index here may be >= than line count?
  int line_count = pango_layout_get_line_count(layout);
  if (line_index >= line_count) {
    line_index = line_count - 1;
  }

#if PANGO_VERSION_CHECK(1,16,0)
  PangoLayoutLine *line = pango_layout_get_line_readonly(layout, line_index);
#else
  PangoLayoutLine *line = pango_layout_get_line(layout, line_index);
#endif

  if (line->length == 0)
    return current_pos;

  if ((line->resolved_dir == PANGO_DIRECTION_RTL && count < 0) ||
      (line->resolved_dir != PANGO_DIRECTION_RTL && count > 0)) {
    index = line->start_index + line->length;
  } else {
    index = line->start_index;
  }
  return g_utf8_pointer_to_offset(text, text + index);
}

void GtkEditImpl::SetCursor(int cursor) {
  ResetImContext();
  // If there was a selection range, then the selection range will be cleared.
  // Then content_modified_ shall be set to true to force redrawing the text.
  if (cursor_ != selection_bound_)
    content_modified_ = true;
  cursor_ = cursor;
  selection_bound_ = cursor;
}

int GtkEditImpl::XYToOffset(int x, int y) {
  int width, height;
  PangoLayout *layout = EnsureLayout();
  pango_layout_get_pixel_size(layout, &width, &height);

  if (y < 0) {
    return 0;
  } else if (y >= height) {
    return text_length_;
  } else {
    int trailing;
    int index;
    const char *text = pango_layout_get_text(layout);
    pango_layout_xy_to_index(layout, x * PANGO_SCALE, y * PANGO_SCALE,
                             &index, &trailing);
    int offset = g_utf8_pointer_to_offset(text, text + index) + trailing;
    // Adjust the offset if preedit is not empty and if the offset is after
    // current cursor.
    if (preedit_.length() && offset > cursor_) {
      int preedit_len = g_utf8_strlen(preedit_.c_str(), preedit_.length());
      if (offset >= cursor_ + preedit_len)
        offset -= preedit_len;
      else
        offset = cursor_;
    }
    if (offset > text_length_) offset = text_length_;
    return offset;
  }
}

bool GtkEditImpl::GetSelectionBounds(int *start, int *end) {
  if (start)
    *start = std::min(selection_bound_, cursor_);
  if (end)
    *end = std::max(selection_bound_, cursor_);

  return(selection_bound_ != cursor_);
}

void GtkEditImpl::SetSelectionBounds(int selection_bound, int cursor) {
  ResetImContext();
  if (selection_bound_ != selection_bound || cursor_ != cursor)
    content_modified_ = true;
  selection_bound_ = selection_bound;
  cursor_ = cursor;
}

void GtkEditImpl::EnterText(const char *str) {
  if (readonly_ || !str || !*str) return;

  if (GetSelectionBounds(NULL, NULL)) {
    DeleteSelection();
  } else if (overwrite_ && cursor_ != text_length_) {
    DeleteText(cursor_, cursor_ + 1);
  }

  const char *end = NULL;
  g_utf8_validate(str, -1, &end);
  if (end > str) {
    int n_chars = g_utf8_strlen(str, end - str);
    int index = g_utf8_offset_to_pointer(text_.c_str(), cursor_) - text_.c_str();
    text_.insert(index, str, end - str);
    cursor_ += n_chars;
    selection_bound_ += n_chars;
    text_length_ += n_chars;
  }

  ResetLayout();
  owner_->FireOnChangeEvent();
}

void GtkEditImpl::DeleteText(int start, int end) {
  if (readonly_) return;

  if (start < 0)
    start = 0;
  else if (start > text_length_)
    start = text_length_;

  if (end < 0)
    end = 0;
  else if (end > text_length_)
    end = text_length_;

  if (start > end)
    std::swap(start, end);
  else if (start == end)
    return;

  int start_index =
      g_utf8_offset_to_pointer(text_.c_str(), start) - text_.c_str();
  int end_index =
      g_utf8_offset_to_pointer(text_.c_str(), end) - text_.c_str();

  text_.erase(start_index, end_index - start_index);

  if (cursor_ >= end)
    cursor_ -= (end - start);
  if (selection_bound_ >= end)
    selection_bound_ -= (end - start);

  text_length_ -= (end - start);

  ResetLayout();
  owner_->FireOnChangeEvent();
}

void GtkEditImpl::SelectWord() {
  int selection_bound = MoveWords(cursor_, -1);
  int cursor = MoveWords(selection_bound, 1);
  SetSelectionBounds(selection_bound, cursor);
}

void GtkEditImpl::SelectLine() {
  int selection_bound = MoveLineEnds(cursor_, -1);
  int cursor = MoveLineEnds(selection_bound, 1);
  SetSelectionBounds(selection_bound, cursor);
}

void GtkEditImpl::SelectAll() {
  SetSelectionBounds(0, text_length_);
}

void GtkEditImpl::DeleteSelection() {
  int start, end;
  if (GetSelectionBounds(&start, &end))
    DeleteText(start, end);
}

void GtkEditImpl::CopyClipboard() {
  int start, end;
  if (GetSelectionBounds(&start, &end)) {
    GtkWidget *widget = GetWidgetAndCursorLocation(NULL);
    if (widget) {
      if (visible_) {
        int start_index =
            g_utf8_offset_to_pointer(text_.c_str(), start) - text_.c_str();
        int end_index =
            g_utf8_offset_to_pointer(text_.c_str(), end) - text_.c_str();
        gtk_clipboard_set_text(
            gtk_widget_get_clipboard(widget, GDK_SELECTION_CLIPBOARD),
            text_.c_str() + start_index,
            end_index - start_index);
      } else {
        // Don't copy real content if it's in invisible.
        std::string content;
        for (int i = start; i < end; ++i)
          content.append(password_char_);
        gtk_clipboard_set_text(
            gtk_widget_get_clipboard(widget, GDK_SELECTION_CLIPBOARD),
            content.c_str(), content.length());
      }
    }
  }
}

void GtkEditImpl::CutClipboard() {
  CopyClipboard();
  DeleteSelection();
}

void GtkEditImpl::PasteClipboard() {
  GtkWidget *widget = GetWidgetAndCursorLocation(NULL);
  if (widget) {
    gtk_clipboard_request_text(
        gtk_widget_get_clipboard(widget, GDK_SELECTION_CLIPBOARD),
        PasteCallback, this);
  }
}

void GtkEditImpl::BackSpace() {
  if (GetSelectionBounds(NULL, NULL)) {
    DeleteSelection();
  } else {
    if (cursor_ == 0) return;
    DeleteText(cursor_ - 1, cursor_);
  }
}

void GtkEditImpl::Delete() {
  if (GetSelectionBounds(NULL, NULL)) {
    DeleteSelection();
  } else {
    if (cursor_ == text_length_) return;
    DeleteText(cursor_, cursor_ + 1);
  }
}

void GtkEditImpl::ToggleOverwrite() {
  overwrite_ = !overwrite_;
}

Color GtkEditImpl::GetSelectionBackgroundColor() {
  GtkWidget *widget = GetWidgetAndCursorLocation(NULL);
  if (widget) {
    GtkStyle *style = gtk_widget_get_style(widget);
    if (style) {
      GdkColor *color =
          &style->base[focused_ ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE];
      return Color(static_cast<double>(color->red) / 65535.0,
                   static_cast<double>(color->green) / 65535.0,
                   static_cast<double>(color->blue) / 65535.0);
    }
  }
  return kDefaultSelectionBackgroundColor;
}

Color GtkEditImpl::GetSelectionTextColor() {
  GtkWidget *widget = GetWidgetAndCursorLocation(NULL);
  if (widget) {
    GtkStyle *style = gtk_widget_get_style(widget);
    if (style) {
      GdkColor *color =
          &style->text[focused_ ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE];
      return Color(static_cast<double>(color->red) / 65535.0,
                   static_cast<double>(color->green) / 65535.0,
                   static_cast<double>(color->blue) / 65535.0);
    }
  }
  return kDefaultSelectionTextColor;
}

GtkWidget *GtkEditImpl::GetWidgetAndCursorLocation(GdkRectangle *cur) {
  GtkWidget *widget = GTK_WIDGET(owner_->GetView()->GetNativeWidget());
  if (widget && cur) {
    int cur_x, cur_y, cur_height;
    int display_width = width_ - kInnerBorderX * 2;
    int display_height = height_ - kInnerBorderY * 2;
    GetCursorLocationInLayout(&cur_x, &cur_y, &cur_height, NULL, NULL, NULL);
    cur_x = Clamp(cur_x + scroll_offset_x_, 0, display_width);
    cur_y = Clamp(cur_y + scroll_offset_y_, 0, display_height);
    double x, y, height;
    owner_->GetView()->ViewCoordToNativeWidgetCoord(0, cur_height,
                                                    &x, &height);
    owner_->SelfCoordToViewCoord(cur_x, cur_y, &x, &y);
    owner_->GetView()->ViewCoordToNativeWidgetCoord(x, y, &x, &y);
    cur->x = int(x);
    cur->y = int(y);
    cur->width = 0;
    cur->height = int(ceil(height));
  }
  return widget;
}

void GtkEditImpl::GetCursorLocationInLayout(int *strong_x, int *strong_y,
                                            int *strong_height,
                                            int *weak_x, int *weak_y,
                                            int *weak_height) {
  PangoLayout *layout = EnsureLayout();
  const char *text = pango_layout_get_text(layout);
  size_t cursor_index =
      g_utf8_offset_to_pointer(text, cursor_ + preedit_cursor_) - text;

  PangoRectangle strong, weak;
  pango_layout_get_cursor_pos(layout, cursor_index, &strong, &weak);

  if (strong_x)
    *strong_x = PANGO_PIXELS(strong.x);
  if (strong_y)
    *strong_y = PANGO_PIXELS(strong.y);
  if (strong_height)
    *strong_height = PANGO_PIXELS(strong.height);
  if (weak_x)
    *weak_x = PANGO_PIXELS(weak.x);
  if (weak_y)
    *weak_y = PANGO_PIXELS(weak.y);
  if (weak_height)
    *weak_height = PANGO_PIXELS(weak.height);
}

void GtkEditImpl::UpdateIMCursorLocation() {
  if (im_context_) {
    GdkRectangle cur;
    GtkWidget *widget = GetWidgetAndCursorLocation(&cur);
    if (widget && widget->window) {
      gtk_im_context_set_client_window(im_context_, widget->window);
      gtk_im_context_set_cursor_location(im_context_, &cur);
      DLOG("Update IM cursor location: x=%d, y=%d, h=%d",
           cur.x, cur.y, cur.height);
    }
  }
}

void GtkEditImpl::CommitCallback(GtkIMContext *context, const char *str, void *gg) {
  reinterpret_cast<GtkEditImpl*>(gg)->EnterText(str);
  reinterpret_cast<GtkEditImpl*>(gg)->QueueRefresh(false);
}

gboolean GtkEditImpl::RetrieveSurroundingCallback(GtkIMContext *context, void *gg) {
  GtkEditImpl *edit = reinterpret_cast<GtkEditImpl*>(gg);
  int index =
    g_utf8_offset_to_pointer(edit->text_.c_str(), edit->cursor_) -
    edit->text_.c_str();
  gtk_im_context_set_surrounding(context, edit->text_.c_str(),
                                 edit->text_.length(), index);
  return TRUE;
}

gboolean GtkEditImpl::DeleteSurroundingCallback(GtkIMContext *context, int offset,
                                            int n_chars, void *gg) {
  GtkEditImpl *edit = reinterpret_cast<GtkEditImpl*>(gg);
  int start = edit->cursor_ + offset;
  int end = start + n_chars;
  edit->DeleteText(start, end);
  edit->QueueRefresh(false);
  return TRUE;
}

void GtkEditImpl::PreeditStartCallback(GtkIMContext *context, void *gg) {
  reinterpret_cast<GtkEditImpl*>(gg)->ResetPreedit();
  reinterpret_cast<GtkEditImpl*>(gg)->QueueRefresh(false);
  reinterpret_cast<GtkEditImpl*>(gg)->UpdateIMCursorLocation();
}

void GtkEditImpl::PreeditChangedCallback(GtkIMContext *context, void *gg) {
  GtkEditImpl *edit = reinterpret_cast<GtkEditImpl*>(gg);
  char *str;
  edit->ResetPreedit();
  gtk_im_context_get_preedit_string(context, &str,
                                    &edit->preedit_attrs_,
                                    &edit->preedit_cursor_);
  edit->preedit_.assign(str);
  g_free(str);
  edit->QueueRefresh(false);
  edit->need_im_reset_ = true;
  edit->content_modified_ = true;
}

void GtkEditImpl::PreeditEndCallback(GtkIMContext *context, void *gg) {
  reinterpret_cast<GtkEditImpl*>(gg)->ResetPreedit();
  reinterpret_cast<GtkEditImpl*>(gg)->QueueRefresh(false);
}

void GtkEditImpl::PasteCallback(GtkClipboard *clipboard,
                            const gchar *str, void *gg) {
  reinterpret_cast<GtkEditImpl*>(gg)->EnterText(str);
  reinterpret_cast<GtkEditImpl*>(gg)->QueueRefresh(false);
}

} // namespace gtk
} // namespace ggadget

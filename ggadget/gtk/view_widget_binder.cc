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
#include <cairo.h>

#include <ggadget/common.h>
#include <ggadget/logger.h>
#include <ggadget/event.h>
#include <ggadget/main_loop_interface.h>
#include <ggadget/view_interface.h>
#include <ggadget/view_host_interface.h>
#include "view_widget_binder.h"
#include "cairo_canvas.h"
#include "cairo_graphics.h"
#include "key_convert.h"
#include "utilities.h"

namespace ggadget {
namespace gtk {

static const char *kUriListTarget = "text/uri-list";

// A small motion threshold to prevent a click with tiny mouse move from being
// treated as window move or resize.
static const double kWindowMoveResizeThreshold = 2;

class ViewWidgetBinder::Impl {
 public:
  Impl(CairoGraphics *gfx, ViewInterface *view,
       ViewHostInterface *host, GtkWidget *widget,
       bool no_background)
    : gfx_(gfx),
      view_(view),
      host_(host),
      widget_(widget),
      handlers_(new gulong[kEventHandlersNum]),
      current_drag_event_(NULL),
      on_zoom_connection_(NULL),
      current_widget_width_(0),
      current_widget_height_(0),
      dbl_click_(false),
      composited_(false),
      no_background_(no_background),
      focused_(false),
      zoom_(gfx_->GetZoom()),
      mouse_down_x_(-1),
      mouse_down_y_(-1),
      mouse_down_hittest_(ViewInterface::HT_CLIENT) {
    ASSERT(gfx);
    ASSERT(view);
    ASSERT(host);
    ASSERT(GTK_IS_WIDGET(widget));
    ASSERT(GTK_WIDGET_NO_WINDOW(widget) == FALSE);

    g_object_ref(G_OBJECT(widget_));
    gtk_widget_set_app_paintable(widget_, TRUE);
    gint events = GDK_EXPOSURE_MASK |
                  GDK_FOCUS_CHANGE_MASK |
                  GDK_ENTER_NOTIFY_MASK |
                  GDK_LEAVE_NOTIFY_MASK |
                  GDK_BUTTON_PRESS_MASK |
                  GDK_BUTTON_RELEASE_MASK |
                  GDK_POINTER_MOTION_MASK |
                  GDK_POINTER_MOTION_HINT_MASK |
                  GDK_STRUCTURE_MASK;

    if (GTK_WIDGET_REALIZED(widget_))
      gtk_widget_add_events(widget_, events);
    else
      gtk_widget_set_events(widget_, gtk_widget_get_events(widget_) | events);

    GTK_WIDGET_SET_FLAGS(widget_, GTK_CAN_FOCUS);

    static const GtkTargetEntry kDragTargets[] = {
      { const_cast<char *>(kUriListTarget), 0, 0 },
    };

    gtk_drag_dest_set(widget_, static_cast<GtkDestDefaults>(0),
                      kDragTargets, arraysize(kDragTargets), GDK_ACTION_COPY);

    SetupBackgroundMode();

    for (size_t i = 0; i < kEventHandlersNum; ++i) {
      handlers_[i] = g_signal_connect(G_OBJECT(widget_),
                                      kEventHandlers[i].event,
                                      kEventHandlers[i].handler,
                                      this);
    }

    on_zoom_connection_ = gfx_->ConnectOnZoom(NewSlot(this, &Impl::OnZoom));
  }

  ~Impl() {
    view_ = NULL;

    for (size_t i = 0; i < kEventHandlersNum; ++i) {
      if (handlers_[i] > 0)
        g_signal_handler_disconnect(G_OBJECT(widget_), handlers_[i]);
      else
        DLOG("Handler %s was not connected.", kEventHandlers[i].event);
    }

    delete[] handlers_;
    handlers_ = NULL;

    if (current_drag_event_) {
      delete current_drag_event_;
      current_drag_event_ = NULL;
    }

    if (on_zoom_connection_) {
      on_zoom_connection_->Disconnect();
      on_zoom_connection_ = NULL;
    }

    g_object_unref(G_OBJECT(widget_));
  }

  void OnZoom(double zoom) {
    zoom_ = zoom;
    host_->QueueResize();
  }

  // Disable background if required.
  void SetupBackgroundMode() {
    // Only try to disable background if explicitly required.
    if (no_background_) {
      composited_ = DisableWidgetBackground(widget_);
    }
  }

  static gboolean ButtonPressHandler(GtkWidget *widget, GdkEventButton *event,
                                     gpointer user_data) {
    DLOG("Button press %d", event->button);

    Impl *impl = reinterpret_cast<Impl *>(user_data);
    EventResult result = EVENT_RESULT_UNHANDLED;

    impl->host_->SetTooltip(NULL);

    if (!impl->focused_) {
      impl->focused_ = true;
      SimpleEvent e(Event::EVENT_FOCUS_IN);
      // Ignore the result.
      impl->view_->OnOtherEvent(e);
      if (!gtk_widget_is_focus(widget))
        gtk_widget_grab_focus(widget);
    }

    int mod = ConvertGdkModifierToModifier(event->state);
    int button = event->button == 1 ? MouseEvent::BUTTON_LEFT :
                 event->button == 2 ? MouseEvent::BUTTON_MIDDLE :
                 event->button == 3 ? MouseEvent::BUTTON_RIGHT :
                                      MouseEvent::BUTTON_NONE;

    Event::Type type = Event::EVENT_INVALID;
    if (event->type == GDK_BUTTON_PRESS) {
      type = Event::EVENT_MOUSE_DOWN;
      impl->mouse_down_x_ = event->x;
      impl->mouse_down_y_ = event->y;
    } else if (event->type == GDK_2BUTTON_PRESS) {
      impl->dbl_click_ = true;
      if (button == MouseEvent::BUTTON_LEFT)
        type = Event::EVENT_MOUSE_DBLCLICK;
      else if (button == MouseEvent::BUTTON_RIGHT)
        type = Event::EVENT_MOUSE_RDBLCLICK;
    }

    if (button != MouseEvent::BUTTON_NONE && type != Event::EVENT_INVALID) {
      MouseEvent e(type, event->x / impl->zoom_, event->y / impl->zoom_,
                   0, 0, button, mod);

      result = impl->view_->OnMouseEvent(e);

      impl->mouse_down_hittest_ = impl->view_->GetHitTest();
      // If the View's hittest represents a special button, then handle it
      // here.
      if (result == EVENT_RESULT_UNHANDLED &&
          button == MouseEvent::BUTTON_LEFT &&
          type == Event::EVENT_MOUSE_DOWN) {
        if (impl->mouse_down_hittest_ == ViewInterface::HT_MENU) {
          impl->host_->ShowContextMenu(button);
        } else if (impl->mouse_down_hittest_ == ViewInterface::HT_CLOSE) {
          impl->host_->CloseView();
        }
        result = EVENT_RESULT_HANDLED;
      }
    }

    return result != EVENT_RESULT_UNHANDLED;
  }

  static gboolean ButtonReleaseHandler(GtkWidget *widget, GdkEventButton *event,
                                       gpointer user_data) {
    DLOG("Button release %d", event->button);

    Impl *impl = reinterpret_cast<Impl *>(user_data);
    EventResult result = EVENT_RESULT_UNHANDLED;
    EventResult result2 = EVENT_RESULT_UNHANDLED;

    impl->host_->SetTooltip(NULL);
    gdk_pointer_ungrab(event->time);

    int mod = ConvertGdkModifierToModifier(event->state);
    int button = event->button == 1 ? MouseEvent::BUTTON_LEFT :
                 event->button == 2 ? MouseEvent::BUTTON_MIDDLE :
                 event->button == 3 ? MouseEvent::BUTTON_RIGHT :
                                      MouseEvent::BUTTON_NONE;

    if (button != MouseEvent::BUTTON_NONE) {
      MouseEvent e(Event::EVENT_MOUSE_UP,
                   event->x / impl->zoom_, event->y / impl->zoom_,
                   0, 0, button, mod);
      result = impl->view_->OnMouseEvent(e);

      if (!impl->dbl_click_) {
        MouseEvent e2(button == MouseEvent::BUTTON_LEFT ?
                      Event::EVENT_MOUSE_CLICK : Event::EVENT_MOUSE_RCLICK,
                      event->x / impl->zoom_, event->y / impl->zoom_,
                      0, 0, button, mod);
        result2 = impl->view_->OnMouseEvent(e2);
      } else {
        impl->dbl_click_ = false;
      }
    }

    impl->mouse_down_x_ = -1;
    impl->mouse_down_y_ = -1;
    impl->mouse_down_hittest_ = ViewInterface::HT_CLIENT;

    return result != EVENT_RESULT_UNHANDLED ||
           result2 != EVENT_RESULT_UNHANDLED;
  }

  static gboolean ConfigureHandler(GtkWidget *widget, GdkEventConfigure *event,
                                   gpointer user_data) {
    //For debug purpose.
    //DLOG("Configure : x:%d y:%d - w:%d h:%d",
    //     event->x, event->y, event->width, event->height);
    return FALSE;
  }

  static gboolean KeyPressHandler(GtkWidget *widget, GdkEventKey *event,
                                  gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    EventResult result = EVENT_RESULT_UNHANDLED;
    EventResult result2 = EVENT_RESULT_UNHANDLED;

    impl->host_->SetTooltip(NULL);

    int mod = ConvertGdkModifierToModifier(event->state);
    unsigned int key_code = ConvertGdkKeyvalToKeyCode(event->keyval);
    if (key_code) {
      KeyboardEvent e(Event::EVENT_KEY_DOWN, key_code, mod, event);
      result = impl->view_->OnKeyEvent(e);
    } else {
      LOG("Unknown key: 0x%x", event->keyval);
    }

    guint32 key_char = 0;
    if ((event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0) {
      if (key_code == KeyboardEvent::KEY_ESCAPE ||
          key_code == KeyboardEvent::KEY_RETURN ||
          key_code == KeyboardEvent::KEY_BACK ||
          key_code == KeyboardEvent::KEY_TAB) {
        // gdk_keyval_to_unicode doesn't support the above keys.
        key_char = key_code;
      } else {
        key_char = gdk_keyval_to_unicode(event->keyval);
      }
    } else if ((event->state & GDK_CONTROL_MASK) &&
               key_code >= 'A' && key_code <= 'Z') {
      // Convert CTRL+(A to Z) to key press code for compatibility.
      key_char = key_code - 'A' + 1;
    }

    if (key_char) {
      // Send the char code in KEY_PRESS event.
      KeyboardEvent e2(Event::EVENT_KEY_PRESS, key_char, mod, event);
      result2 = impl->view_->OnKeyEvent(e2);
    }

    return result != EVENT_RESULT_UNHANDLED ||
           result2 != EVENT_RESULT_UNHANDLED;
  }

  static gboolean KeyReleaseHandler(GtkWidget *widget, GdkEventKey *event,
                                    gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    EventResult result = EVENT_RESULT_UNHANDLED;

    int mod = ConvertGdkModifierToModifier(event->state);
    unsigned int key_code = ConvertGdkKeyvalToKeyCode(event->keyval);
    if (key_code) {
      KeyboardEvent e(Event::EVENT_KEY_UP, key_code, mod, event);
      result = impl->view_->OnKeyEvent(e);
    } else {
      LOG("Unknown key: 0x%x", event->keyval);
    }

    return result != EVENT_RESULT_UNHANDLED;
  }

  static gboolean ExposeHandler(GtkWidget *widget, GdkEventExpose *event,
                                gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    gint width, height;
    gdk_drawable_get_size(widget->window, &width, &height);

    cairo_t *cr = gdk_cairo_create(widget->window);
    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);

    // If background is disabled, and if composited is enabled,  the window
    // needs clearing every times.
    if (impl->no_background_ && impl->composited_) {
      cairo_operator_t op = cairo_get_operator(cr);
      cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint(cr);
      cairo_set_operator(cr, op);
    }

    // OK to downcast here since the canvas is created using CairoGraphics.
    CairoCanvas *canvas = ggadget::down_cast<CairoCanvas*>(
      impl->gfx_->NewCanvas(impl->view_->GetWidth(), impl->view_->GetHeight()));

    ASSERT(canvas);

    impl->view_->Draw(canvas);

    cairo_surface_t *surface = canvas->GetSurface();
    cairo_set_source_surface(cr, surface, 0., 0.);
    cairo_paint(cr);

#if GTK_CHECK_VERSION(2,10,0)
    // We need set input shape mask if there is no background.
    if (impl->no_background_ && impl->composited_) {
      int canvasw = cairo_image_surface_get_width(surface);
      int canvash = cairo_image_surface_get_height(surface);

      // create an identical bitmap to use as shape mask
      GdkBitmap *bitmap =
        static_cast<GdkBitmap *>(gdk_pixmap_new(NULL, std::min(canvasw, width),
                                                std::min(canvash, height), 1));
      cairo_t *mask = gdk_cairo_create(bitmap);
      // Note: Don't set clipping here, since we're resetting shape mask
      // for the entire widget, including areas outside the exposed region.
      cairo_set_operator(mask, CAIRO_OPERATOR_CLEAR);
      cairo_paint(mask);
      cairo_set_operator(mask, CAIRO_OPERATOR_OVER);
      cairo_set_source_surface(mask, surface, 0., 0.);
      cairo_paint(mask);
      cairo_destroy(mask);
      mask = NULL;

      gtk_widget_input_shape_combine_mask(widget, bitmap, 0, 0);
      gdk_bitmap_unref(bitmap);
    }
#endif

    canvas->Destroy();
    cairo_destroy(cr);

    return TRUE;
  }

  static gboolean MotionNotifyHandler(GtkWidget *widget, GdkEventMotion *event,
                                      gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    int button = ConvertGdkModifierToButton(event->state);
    int mod = ConvertGdkModifierToModifier(event->state);
    MouseEvent e(Event::EVENT_MOUSE_MOVE,
                 event->x / impl->zoom_, event->y / impl->zoom_,
                 0, 0, button, mod);

    if (button != MouseEvent::BUTTON_NONE) {
      // Grab the cursor to prevent losing events.
      gdk_pointer_grab(widget->window, FALSE,
                       (GdkEventMask)(GDK_BUTTON_RELEASE_MASK |
                                      GDK_POINTER_MOTION_MASK |
                                      GDK_POINTER_MOTION_HINT_MASK),
                       NULL, NULL, event->time);
    }

    EventResult result = impl->view_->OnMouseEvent(e);

    if (result == EVENT_RESULT_UNHANDLED && button != MouseEvent::BUTTON_NONE &&
        impl->mouse_down_x_ >= 0 && impl->mouse_down_y_ >= 0 &&
        (abs(int(event->x - impl->mouse_down_x_)) >= kWindowMoveResizeThreshold ||
         abs(int(event->y - impl->mouse_down_y_)) >= kWindowMoveResizeThreshold)) {
      // Send fake mouse up event to the view so that we can start to drag
      // the window. Note: no mouse click event is sent in this case, to prevent
      // unwanted action after window move.
      MouseEvent e(Event::EVENT_MOUSE_UP,
                   event->x / impl->zoom_, event->y / impl->zoom_,
                   0, 0, button, mod);

      // Ignore the result of this fake event.
      impl->view_->OnMouseEvent(e);

      ViewInterface::HitTest hittest = impl->mouse_down_hittest_;
      bool resize_drag = false;
      // Determine the resize drag edge.
      if (hittest == ViewInterface::HT_LEFT ||
          hittest == ViewInterface::HT_RIGHT ||
          hittest == ViewInterface::HT_TOP ||
          hittest == ViewInterface::HT_BOTTOM ||
          hittest == ViewInterface::HT_TOPLEFT ||
          hittest == ViewInterface::HT_TOPRIGHT ||
          hittest == ViewInterface::HT_BOTTOMLEFT ||
          hittest == ViewInterface::HT_BOTTOMRIGHT) {
        resize_drag = true;
      } else if (mod & Event::MOD_CONTROL) {
        // If ctrl is holding, then emulate resize draging.
        // Only for testing purpose. Shall be removed later.
        resize_drag = true;
        gint x = static_cast<int>(event->x);
        gint y = static_cast<int>(event->y);
        gint mid_x = impl->current_widget_width_ / 2;
        gint mid_y = impl->current_widget_height_ / 2;
        if (x < mid_x && y < mid_y)
          hittest = ViewInterface::HT_TOPLEFT;
        else if (x > mid_x && y < mid_y)
          hittest = ViewInterface::HT_TOPRIGHT;
        else if (x < mid_x && y > mid_y)
          hittest = ViewInterface::HT_BOTTOMLEFT;
        else
          hittest = ViewInterface::HT_BOTTOMRIGHT;
      }

      if (resize_drag) {
        impl->host_->BeginResizeDrag(button, hittest);
      } else {
        impl->host_->BeginMoveDrag(button);
      }

      impl->mouse_down_x_ = -1;
      impl->mouse_down_y_ = -1;
      impl->mouse_down_hittest_ = ViewInterface::HT_CLIENT;
    }

    // Since motion hint is enabled, we must notify GTK that we're ready to
    // receive the next motion event.
    // FIXME: Seems that this code has no effect.
#if GTK_CHECK_VERSION(2,12,0)
    gdk_event_request_motions(event); // requires version 2.12
#else
    int x, y;
    gdk_window_get_pointer(widget->window, &x, &y, NULL);
#endif

    return result != EVENT_RESULT_UNHANDLED;
  }

  static gboolean ScrollHandler(GtkWidget *widget, GdkEventScroll *event,
                                gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    int delta_x = 0, delta_y = 0;
    if (event->direction == GDK_SCROLL_UP) {
      delta_y = MouseEvent::kWheelDelta;
    } else if (event->direction == GDK_SCROLL_DOWN) {
      delta_y = -MouseEvent::kWheelDelta;
    } else if (event->direction == GDK_SCROLL_RIGHT) {
      delta_x = MouseEvent::kWheelDelta;
    } else if (event->direction == GDK_SCROLL_LEFT) {
      delta_x = -MouseEvent::kWheelDelta;
    }

    MouseEvent e(Event::EVENT_MOUSE_WHEEL,
                 event->x / impl->zoom_, event->y / impl->zoom_,
                 delta_x, delta_y,
                 ConvertGdkModifierToButton(event->state),
                 ConvertGdkModifierToModifier(event->state));
    return impl->view_->OnMouseEvent(e) != EVENT_RESULT_UNHANDLED;
  }

  static gboolean LeaveNotifyHandler(GtkWidget *widget, GdkEventCrossing *event,
                                     gpointer user_data) {
    if (event->mode != GDK_CROSSING_NORMAL ||
        event->detail == GDK_NOTIFY_INFERIOR) {
      DLOG("Ignores the leave notify: %d %d", event->mode, event->detail);
      return FALSE;
    }

    Impl *impl = reinterpret_cast<Impl *>(user_data);
    impl->host_->SetTooltip(NULL);

    MouseEvent e(Event::EVENT_MOUSE_OUT,
                 event->x / impl->zoom_, event->y / impl->zoom_, 0, 0,
                 MouseEvent::BUTTON_NONE,
                 ConvertGdkModifierToModifier(event->state));
    return impl->view_->OnMouseEvent(e) != EVENT_RESULT_UNHANDLED;
  }

  static gboolean EnterNotifyHandler(GtkWidget *widget, GdkEventCrossing *event,
                                     gpointer user_data) {
    if (event->mode != GDK_CROSSING_NORMAL ||
        event->detail == GDK_NOTIFY_INFERIOR) {
      DLOG("Ignores the enter notify: %d %d", event->mode, event->detail);
      return FALSE;
    }

    Impl *impl = reinterpret_cast<Impl *>(user_data);
    impl->host_->SetTooltip(NULL);

    MouseEvent e(Event::EVENT_MOUSE_OVER,
                 event->x / impl->zoom_, event->y / impl->zoom_, 0, 0,
                 MouseEvent::BUTTON_NONE,
                 ConvertGdkModifierToModifier(event->state));
    return impl->view_->OnMouseEvent(e) != EVENT_RESULT_UNHANDLED;
  }

  static gboolean FocusInHandler(GtkWidget *widget, GdkEventFocus *event,
                                 gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    if (!impl->focused_) {
      impl->focused_ = true;
      SimpleEvent e(Event::EVENT_FOCUS_IN);
      return impl->view_->OnOtherEvent(e) != EVENT_RESULT_UNHANDLED;
    }
    return FALSE;
  }

  static gboolean FocusOutHandler(GtkWidget *widget, GdkEventFocus *event,
                                  gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    if (impl->focused_) {
      impl->focused_ = false;
      SimpleEvent e(Event::EVENT_FOCUS_OUT);
      // Ungrab the pointer if the focus is lost.
      gdk_pointer_ungrab(gtk_get_current_event_time());
      return impl->view_->OnOtherEvent(e) != EVENT_RESULT_UNHANDLED;
    }
    return FALSE;
  }

  static gboolean DragMotionHandler(GtkWidget *widget, GdkDragContext *context,
                                    gint x, gint y, guint time,
                                    gpointer user_data) {
    return OnDragEvent(widget, context, x, y, time,
                       Event::EVENT_DRAG_MOTION, user_data);
  }

  static void DragLeaveHandler(GtkWidget *widget, GdkDragContext *context,
                               guint time, gpointer user_data) {
    OnDragEvent(widget, context, 0, 0, time, Event::EVENT_DRAG_OUT, user_data);
  }

  static gboolean DragDropHandler(GtkWidget *widget, GdkDragContext *context,
                                  gint x, gint y, guint time,
                                  gpointer user_data) {
    gboolean result = OnDragEvent(widget, context, x, y, time,
                                  Event::EVENT_DRAG_DROP, user_data);
    gtk_drag_finish(context, result, FALSE, time);
    return result;
  }

  static void DragDataReceivedHandler(GtkWidget *widget,
                                      GdkDragContext *context,
                                      gint x, gint y,
                                      GtkSelectionData *data,
                                      guint info, guint time,
                                      gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    if (!impl->current_drag_event_) {
      // There are some cases that multiple drag events are fired in one event
      // loop. For example, drag_leave and drag_drop. Although current_drag_event
      // only contains the latter one, there are still multiple
      // drag_data_received events fired.
      return;
    }

    gchar **uris = gtk_selection_data_get_uris(data);
    if (!uris) {
      DLOG("No URI in drag data");
      DisableDrag(widget, context, time);
      return;
    }

    guint count = g_strv_length(uris);
    const char **drag_files = new const char *[count + 1];

    guint accepted_count = 0;
    for (guint i = 0; i < count; i++) {
      gchar *hostname;
      gchar *filename = g_filename_from_uri(uris[i], &hostname, NULL);
      if (filename) {
        if (!hostname)
          drag_files[accepted_count++] = filename;
        else
          g_free(filename);
      }
      g_free(hostname);
    }

    if (accepted_count == 0) {
      DLOG("No acceptable URI in drag data");
      DisableDrag(widget, context, time);
      return;
    }

    drag_files[accepted_count] = NULL;

    impl->current_drag_event_->SetDragFiles(drag_files);
    EventResult result = impl->view_->OnDragEvent(*impl->current_drag_event_);
    if (result == ggadget::EVENT_RESULT_HANDLED) {
      Event::Type type = impl->current_drag_event_->GetType();
      if (type == Event::EVENT_DRAG_DROP || type == Event::EVENT_DRAG_OUT) {
        gtk_drag_unhighlight(widget);
      } else {
        gdk_drag_status(context, GDK_ACTION_COPY, time);
        gtk_drag_highlight(widget);
      }
    } else {
      // Drag event is not accepted by the gadget.
      DisableDrag(widget, context, time);
    }

    delete impl->current_drag_event_;
    impl->current_drag_event_ = NULL;
    for (guint i = 0; i < count; i++) {
      g_free(const_cast<gchar *>(drag_files[i]));
    }
    delete[] drag_files;
    g_strfreev(uris);
  }

  static void SizeAllocateHandler(GtkWidget *widget, GtkAllocation *allocation,
                                  gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    int widget_width = allocation->width;
    int widget_height = allocation->height;

    DLOG("SizeAllocate: %d %d", widget_width, widget_height);

    if (widget_width == impl->current_widget_width_ &&
        widget_height == impl->current_widget_height_)
      return;

    impl->current_widget_width_ = widget_width;
    impl->current_widget_height_ = widget_height;

    if (!GTK_WIDGET_MAPPED(widget)) {
      DLOG("The widget is not mapped yet, don't adjust view size.");
      return;
    }

    ViewInterface::ResizableMode mode = impl->view_->GetResizable();
    if (mode == ViewInterface::RESIZABLE_TRUE) {
      double width = ceil(widget_width / impl->zoom_);
      double height = ceil(widget_height / impl->zoom_);
      if (width != impl->view_->GetWidth() ||
          height != impl->view_->GetHeight()) {
        if (impl->view_->OnSizing(&width, &height)) {
          DLOG("Resize View to: %lf %lf", width, height);
          impl->view_->SetSize(width, height);
        } else {
          // If failed to resize the view, then send a window resize request to
          // host, to change the window size back to the original size.
          impl->host_->QueueResize();
        }
      }
    } else if (mode == ViewInterface::RESIZABLE_ZOOM) {
      double width = impl->view_->GetWidth();
      double height = impl->view_->GetHeight();
      if (width && height) {
        double xzoom = widget_width / width;
        double yzoom = widget_height / height;
        double zoom = std::min(xzoom, yzoom);
        if (zoom != impl->gfx_->GetZoom()) {
          DLOG("Zoom View to: %lf", zoom);
          impl->gfx_->SetZoom(zoom);
          impl->view_->MarkRedraw();
        }
        impl->host_->QueueResize();
      }
    } else {
      DLOG("The size of view widget was changed, "
           "but the view is not resizable.");
      impl->host_->QueueResize();
    }
  }

  static void ScreenChangedHandler(GtkWidget *widget, GdkScreen *last_screen,
                                   gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    impl->SetupBackgroundMode();
  }

  static void CompositedChangedHandler(GtkWidget *widget, gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    impl->SetupBackgroundMode();
  }

  static void DisableDrag(GtkWidget *widget, GdkDragContext *context,
                          guint time) {
    gdk_drag_status(context, static_cast<GdkDragAction>(0), time);
    gtk_drag_unhighlight(widget);
  }

  static gboolean OnDragEvent(GtkWidget *widget, GdkDragContext *context,
                              gint x, gint y, guint time,
                              Event::Type event_type, gpointer user_data) {
    Impl *impl = reinterpret_cast<Impl *>(user_data);
    if (impl->current_drag_event_) {
      // There are some cases that multiple drag events are fired in one event
      // loop. For example, drag_leave and drag_drop, we use the latter one.
      delete impl->current_drag_event_;
      impl->current_drag_event_ = NULL;
    }

    impl->current_drag_event_ = new DragEvent(event_type, x, y, NULL);
    GdkAtom target = gtk_drag_dest_find_target(
        widget, context, gtk_drag_dest_get_target_list(widget));
    if (target != GDK_NONE) {
      gtk_drag_get_data(widget, context, target, time);
      return TRUE;
    } else {
      DLOG("Drag target or action not acceptable");
      DisableDrag(widget, context, time);
      return FALSE;
    }
  }

  CairoGraphics *gfx_;
  ViewInterface *view_;
  ViewHostInterface *host_;
  GtkWidget *widget_;
  gulong *handlers_;
  DragEvent *current_drag_event_;
  Connection *on_zoom_connection_;
  int current_widget_width_;
  int current_widget_height_;
  bool dbl_click_;
  bool composited_;
  bool no_background_;
  bool focused_;
  double zoom_;
  double mouse_down_x_;
  double mouse_down_y_;
  ViewInterface::HitTest mouse_down_hittest_;

  struct EventHandlerInfo {
    const char *event;
    void (*handler)(void);
  };

  static EventHandlerInfo kEventHandlers[];
  static const size_t kEventHandlersNum;
};

ViewWidgetBinder::Impl::EventHandlerInfo
ViewWidgetBinder::Impl::kEventHandlers[] = {
  { "button-press-event",
    G_CALLBACK(ViewWidgetBinder::Impl::ButtonPressHandler) },
  { "button-release-event",
    G_CALLBACK(ViewWidgetBinder::Impl::ButtonReleaseHandler) },
  { "composited-changed",
    G_CALLBACK(ViewWidgetBinder::Impl::CompositedChangedHandler) },
  //{ "configure-event",
  //  G_CALLBACK(ViewWidgetBinder::Impl::ConfigureHandler) },
  { "drag-data-received",
    G_CALLBACK(ViewWidgetBinder::Impl::DragDataReceivedHandler) },
  { "drag-drop",
    G_CALLBACK(ViewWidgetBinder::Impl::DragDropHandler) },
  { "drag-leave",
    G_CALLBACK(ViewWidgetBinder::Impl::DragLeaveHandler) },
  { "drag-motion",
    G_CALLBACK(ViewWidgetBinder::Impl::DragMotionHandler) },
  { "enter-notify-event",
    G_CALLBACK(ViewWidgetBinder::Impl::EnterNotifyHandler) },
  { "expose-event",
    G_CALLBACK(ViewWidgetBinder::Impl::ExposeHandler) },
  { "focus-in-event",
    G_CALLBACK(ViewWidgetBinder::Impl::FocusInHandler) },
  { "focus-out-event",
    G_CALLBACK(ViewWidgetBinder::Impl::FocusOutHandler) },
  { "key-press-event",
    G_CALLBACK(ViewWidgetBinder::Impl::KeyPressHandler) },
  { "key-release-event",
    G_CALLBACK(ViewWidgetBinder::Impl::KeyReleaseHandler) },
  { "leave-notify-event",
    G_CALLBACK(ViewWidgetBinder::Impl::LeaveNotifyHandler) },
  { "motion-notify-event",
    G_CALLBACK(ViewWidgetBinder::Impl::MotionNotifyHandler) },
  { "screen-changed",
    G_CALLBACK(ViewWidgetBinder::Impl::ScreenChangedHandler) },
  { "scroll-event",
    G_CALLBACK(ViewWidgetBinder::Impl::ScrollHandler) },
  { "size-allocate",
    G_CALLBACK(ViewWidgetBinder::Impl::SizeAllocateHandler) },
};

const size_t ViewWidgetBinder::Impl::kEventHandlersNum =
  arraysize(ViewWidgetBinder::Impl::kEventHandlers);

ViewWidgetBinder::ViewWidgetBinder(CairoGraphics *gfx, ViewInterface *view,
                                   ViewHostInterface *host, GtkWidget *widget,
                                   bool no_background)
  : impl_(new Impl(gfx, view, host, widget, no_background)) {
}

ViewWidgetBinder::~ViewWidgetBinder() {
  delete impl_;
  impl_ = NULL;
}

} // namespace gtk
} // namespace ggadget

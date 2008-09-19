/*
  Copyright 2008 Google Inc.

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

#include <vector>
#include <list>

#include "sidebar.h"

#include "button_element.h"
#include "canvas_interface.h"
#include "div_element.h"
#include "element_factory.h"
#include "elements.h"
#include "gadget_consts.h"
#include "gadget.h"
#include "file_manager_factory.h"
#include "host_interface.h"
#include "img_element.h"
#include "math_utils.h"
#include "scriptable_binary_data.h"
#include "view_element.h"
#include "view.h"
#include "messages.h"

namespace ggadget {

static const double kGadgetSpacing = 1;
static const double kUndockDragThreshold = 2;
static const double kBackgroundOpacity = 0.618;
static const double kSideBarMinWidth = 50;
static const double kSideBarMaxWidth = 999;
static const double kBorderWidth = 3;

class SideBar::Impl : public View {
 public:
  class SideBarViewHost : public ViewHostInterface {
   public:
    SideBarViewHost(SideBar::Impl *owner, int index)
      : owner_(owner),
        view_element_(new ViewElement(owner->main_div_, owner, NULL, true)),
        initial_index_(index) {
      view_element_->SetVisible(false);
      owner_->InsertViewElement(index, view_element_);
    }
    virtual ~SideBarViewHost() {
      owner_->RemoveViewElement(view_element_);
      owner_->Layout();
      view_element_ = NULL;
      DLOG("SideBarViewHost Dtor: %p", this);
    }
    virtual ViewHostInterface::Type GetType() const {
      return ViewHostInterface::VIEW_HOST_MAIN;
    }
    virtual void Destroy() { delete this; }
    virtual void SetView(ViewInterface *view) {
      // set invisible before ShowView is called
      view_element_->SetVisible(false);
      view_element_->SetChildView(down_cast<View*>(view));
      QueueDraw();
    }
    virtual ViewInterface *GetView() const {
      return view_element_->GetChildView();
    }
    virtual GraphicsInterface *NewGraphics() const {
      return owner_->view_host_->NewGraphics();
    }
    virtual void *GetNativeWidget() const {
      return owner_->GetNativeWidget();
    }
    virtual void ViewCoordToNativeWidgetCoord(double x, double y,
                                              double *wx, double *wy) const {
      view_element_->ChildViewCoordToViewCoord(x, y, &x, &y);
      owner_->view_host_->ViewCoordToNativeWidgetCoord(x, y, wx, wy);
    }
    virtual void NativeWidgetCoordToViewCoord(double x, double y,
                                              double *vx, double *vy) const {
      owner_->view_host_->NativeWidgetCoordToViewCoord(x, y, &x, &y);
      view_element_->ViewCoordToChildViewCoord(x, y, vx, vy);
    }
    virtual void QueueDraw() {
      if (view_element_)
        view_element_->QueueDraw();
    }
    virtual void QueueResize() {
      owner_->Layout();
    }
    virtual void EnableInputShapeMask(bool /* enable */) {
      // Do nothing.
    }
    virtual void SetResizable(ViewInterface::ResizableMode mode) {}
    virtual void SetCaption(const char *caption) {}
    virtual void SetShowCaptionAlways(bool always) {}
    virtual void SetCursor(int type) {
      view_element_->SetCursor(
          static_cast<ViewInterface::CursorType>(type));
      owner_->view_host_->SetCursor(type);
    }
    virtual void SetTooltip(const char *tooltip) {
      owner_->view_host_->SetTooltip(tooltip);
    }
    virtual bool ShowView(bool modal, int flags,
                          Slot1<void, int> *feedback_handler) {
      delete feedback_handler;
      if (view_element_->GetChildView()) {
        view_element_->SetVisible(true);
        owner_->Layout();
        return true;
      }
      return false;
    }
    virtual void CloseView() {
      view_element_->SetVisible(false);
      owner_->Layout();
    }
    virtual bool ShowContextMenu(int button) {
      return owner_->view_host_->ShowContextMenu(button);
    }
    virtual void BeginResizeDrag(int button, ViewInterface::HitTest hittest) {}
    virtual void BeginMoveDrag(int button) {}
    virtual void Alert(const ViewInterface *view, const char *message) {
      owner_->view_host_->Alert(view, message);
    }
    virtual bool Confirm(const ViewInterface *view, const char *message) {
      return owner_->view_host_->Confirm(view, message);
    }
    virtual std::string Prompt(const ViewInterface *view,
                               const char *message,
                               const char *default_value) {
      return owner_->view_host_->Prompt(view, message, default_value);
    }
    virtual int GetDebugMode() const {
      return owner_->view_host_->GetDebugMode();
    }
    int GetInitialIndex() const { return initial_index_; }
   private:
    SideBar::Impl *owner_;
    ViewElement *view_element_;
    int initial_index_;
  };

  Impl(SideBar *owner, ViewHostInterface *view_host)
    : View(view_host, NULL, NULL, NULL),
      owner_(owner),
      view_host_(view_host),
      null_element_(NULL),
      blank_height_(0),
      mouse_move_event_x_(-1),
      mouse_move_event_y_(-1),
      hit_element_bottom_(false),
      hit_element_normal_part_(false),
      hit_sidebar_border_(false),
      hittest_(HT_CLIENT),
      original_width_(0),
      original_height_(0),
      top_div_(NULL),
      main_div_(NULL),
      add_gadget_button_(NULL),
      menu_button_(NULL),
      close_button_(NULL),
      children_(NULL),
      initializing_(false) {
    SetResizable(ViewInterface::RESIZABLE_TRUE);
    // EnableCanvasCache(false);
    SetupUI();
  }
  ~Impl() {
  }

 public:
  virtual EventResult OnMouseEvent(const MouseEvent &event) {
    hittest_ = HT_CLIENT;

    EventResult result = EVENT_RESULT_UNHANDLED;
    // don't sent mouse event to view elements when layouting or resizing
    if (!hit_element_bottom_)
      result = View::OnMouseEvent(event);

    if (event.GetType() == Event::EVENT_MOUSE_DOWN) {
      ViewElement *e = GetMouseOverViewElement();
      onclick_signal_(e ? e->GetChildView() : NULL);
    }

    if (result == EVENT_RESULT_UNHANDLED && !IsMinimized()) {
      if (event.GetX() >= 0 && event.GetX() < kBorderWidth) {
        hittest_ = HT_LEFT;
        SetCursor(CURSOR_SIZEWE);
      } else if (event.GetX() < GetWidth() &&
               event.GetX() >= GetWidth() - kBorderWidth) {
        hittest_ = HT_RIGHT;
        SetCursor(CURSOR_SIZEWE);
      }
    }

    if (result != EVENT_RESULT_UNHANDLED ||
        event.GetButton() != MouseEvent::BUTTON_LEFT) {
      return result;
    }

    int index = 0;
    double x, y;
    BasicElement *element = NULL;
    ViewElement *focused = GetMouseOverViewElement();
    double offset = mouse_move_event_y_ - event.GetY();
    switch (event.GetType()) {
      case Event::EVENT_MOUSE_DOWN:
        DLOG("Mouse down at (%f,%f)", event.GetX(), event.GetY());
        mouse_move_event_x_ = event.GetX();
        mouse_move_event_y_ = event.GetY();

        if (hittest_ != HT_CLIENT) {
          hit_sidebar_border_ = true;
          return result;
        }

        if (!focused) return result;
        focused->ViewCoordToSelfCoord(event.GetX(), event.GetY(), &x, &y);
        switch (focused->GetHitTest(x, y)) {
          case HT_BOTTOM:
            hit_element_bottom_ = true;
            // record the original height of each view elements
            for (; index < children_->GetCount(); ++index) {
              element = children_->GetItemByIndex(index);
              elements_height_.push_back(element->GetPixelHeight());
            }
            if (element) {
              blank_height_ = main_div_->GetPixelHeight() -
                  element->GetPixelY() - element->GetPixelHeight();
            }
            break;
          case HT_CLIENT:
            hit_element_normal_part_ = true;
            break;
          default:
            break;
        }
        return result;
        break;
      case Event::EVENT_MOUSE_UP:
        ResetState();
        return result;
        break;
      case Event::EVENT_MOUSE_MOVE:  // handle it soon
        if (mouse_move_event_x_ < 0 && mouse_move_event_y_ < 0) {
          return EVENT_RESULT_HANDLED;
        }
        if (!focused) {
          // if mouse over null_element_, BasicElement::GetMouseOverElement()
          // will not return it. Check it specially
          if (null_element_) {
            double x, y;
            null_element_->ViewCoordToSelfCoord(event.GetX(), event.GetY(),
                                                &x, &y);
            if (y >= 0 && y <= null_element_->GetPixelHeight()) {
              return EVENT_RESULT_HANDLED;
            }
          }
          return result;
        }
        if (hit_element_bottom_) {
          // set cursor so that user understand that it's still in layout process
          SetCursor(CURSOR_SIZENS);
          int index = GetIndex(focused);
          if (offset < 0) {
            if (DownResize(false, index + 1, &offset) &&
                UpResize(true, index, &offset)) {
              DownResize(true, index + 1, &offset);
              QueueDraw();
            }
          } else {
            UpResize(true, index, &offset);
            Layout();
          }
        } else if (hit_element_normal_part_ && focused->GetChildView() &&
                   (std::abs(offset) > kUndockDragThreshold ||
                    std::abs(event.GetX() - mouse_move_event_x_) >
                    kUndockDragThreshold)) {
          focused->ViewCoordToChildViewCoord(mouse_move_event_x_,
                                             mouse_move_event_y_, &x, &y);
          onundock_signal_(focused->GetChildView(), GetIndex(focused), x, y);
          ResetState();
        } else if (hit_sidebar_border_) {
          return EVENT_RESULT_UNHANDLED;
        }
        break;
      default:
        return result;
    }

    return EVENT_RESULT_HANDLED;
  }

  virtual HitTest GetHitTest() const {
    // Always return HT_CLIENT except when mouse cursor is on left or right
    // border.
    return hittest_;
  }

  virtual bool OnAddContextMenuItems(MenuInterface *menu) {
    BasicElement *e = GetMouseOverElement();
    if (e && e->IsInstanceOf(ViewElement::CLASS_ID)) {
      e->OnAddContextMenuItems(menu);
    } else {
      onmenu_signal_(menu);
    }
    // In sidebar mode, view host shouldn't add any host level menu items.
    return false;
  }

  virtual bool OnSizing(double *width, double *height) {
    return kSideBarMinWidth < *width && *width < kSideBarMaxWidth &&
           *height >= main_div_->GetPixelY();
  }

  virtual void SetSize(double width, double height) {
    if (top_div_->IsVisible() && main_div_->IsVisible()) {
      // Not minimized.
      View::SetSize(width, height);
      original_width_ = width;
      original_height_ = height;
    } else if (top_div_->IsVisible()) {
      // horizontal minimized.
      View::SetSize(width, main_div_->GetPixelY());
      original_width_ = width;
    } else {
      // vertical minimized.
      View::SetSize(kBorderWidth, height);
      original_height_ = height;
    }

    if (main_div_->IsVisible()) {
      main_div_->SetPixelWidth(width - kBorderWidth * 2);
      main_div_->SetPixelHeight(height - kBorderWidth - main_div_->GetPixelY());
    }
    if (top_div_->IsVisible()) {
      top_div_->SetPixelWidth(width - kBorderWidth * 2);
    }
    Layout();
  }

 public:
  ViewElement *GetMouseOverViewElement() {
    BasicElement *e = GetMouseOverElement();
    return (e && e->IsInstanceOf(ViewElement::CLASS_ID)) ?
           down_cast<ViewElement*>(e) : NULL;
  }

  ViewHostInterface *NewViewHost(int index) {
    DLOG("sidebar: NewViewHost with index: %d", index);
    SideBarViewHost *vh = new SideBarViewHost(this, index);
    return vh;
  }

  void Minimize(bool vertical) {
    // Don't minimize again if it's already minimized.
    if (!IsMinimized()) {
      if (vertical) {
        top_div_->SetVisible(false);
        main_div_->SetVisible(false);
      } else {
        main_div_->SetVisible(false);
      }
      SetSize(original_width_, original_height_);
    }
  }

  bool IsMinimized() const {
    return !(top_div_->IsVisible() && main_div_->IsVisible());
  }

  void Restore() {
    if (IsMinimized()) {
      top_div_->SetVisible(true);
      main_div_->SetVisible(true);
      SetSize(original_width_, original_height_);
    }
  }

  int GetIndexOfPosition(double y) const {
    int count = children_->GetCount();
    for (int i = 0; i < count; ++i) {
      ViewElement *e = down_cast<ViewElement*>(children_->GetItemByIndex(i));
      double x, middle;
      e->SelfCoordToViewCoord(0, e->GetPixelHeight()/2, &x, &middle);
      if (y < middle)
        return i;
    }
    return count;
  }

  int GetIndexOfView(const ViewInterface *view) const {
    int count = children_->GetCount();
    for (int i = 0; i < count; ++i) {
      ViewElement *e = down_cast<ViewElement*>(children_->GetItemByIndex(i));
      View *v = e->GetChildView();
      if (v == view)
        return i;
    }
    return -1;
  }

  void InsertPlaceholder(int index, double height) {
    // only one null element is allowed
    if (!null_element_) {
      null_element_ = new ViewElement(main_div_, this, NULL, true);
    }
    null_element_->SetPixelHeight(height);
    InsertViewElement(index, null_element_);
  }

  void ClearPlaceholder() {
    if (null_element_) {
      RemoveViewElement(null_element_);
      null_element_ = NULL;
      Layout();
    }
  }

  void EnumerateViews(Slot2<bool, int, View*> *slot) {
    ASSERT(slot);
    int count = children_->GetCount();
    for (int i = 0; i < count; ++i) {
      ViewElement *e = down_cast<ViewElement*>(children_->GetItemByIndex(i));
      View *v = e->GetChildView();
      if (v && !(*slot)(i, v))
        break;
    }
    delete slot;
  }

  void ResetState() {
    mouse_move_event_x_ = -1;
    mouse_move_event_y_ = -1;
    hit_element_bottom_ = false;
    hit_element_normal_part_ = false;
    hit_sidebar_border_ = false;
    blank_height_ = 0;
    elements_height_.clear();
  }

  void SetupUI() {
    ImgElement *background = new ImgElement(NULL, this, NULL);
    GetChildren()->InsertElement(background, NULL);
    background->SetSrc(Variant(kVDMainBackground));
    background->SetStretchMiddle(true);
    background->SetOpacity(kBackgroundOpacity);
    background->SetPixelX(0);
    background->SetPixelY(0);
    background->SetRelativeWidth(1);
    background->SetRelativeHeight(1);
    background->EnableCanvasCache(true);

    top_div_ = new DivElement(NULL, this, NULL);
    GetChildren()->InsertElement(top_div_, NULL);
    top_div_->SetPixelX(kBorderWidth);
    top_div_->SetPixelY(kBorderWidth);

    ImgElement *icon = new ImgElement(top_div_, this, NULL);
    top_div_->GetChildren()->InsertElement(icon, NULL);
    icon->SetSrc(Variant(kSideBarGoogleIcon));
    icon->SetPixelX(0);
    icon->SetPixelY(0);

    DivElement *button_div = new DivElement(top_div_, this, NULL);
    top_div_->GetChildren()->InsertElement(button_div, NULL);
    button_div->SetRelativePinX(1);
    button_div->SetRelativeX(1);
    button_div->SetPixelY(0);
    button_div->SetRelativeHeight(1);

    add_gadget_button_ = new ButtonElement(button_div, this, NULL);
    button_div->GetChildren()->InsertElement(add_gadget_button_, NULL);
    add_gadget_button_->SetImage(Variant(kSBButtonAddUp));
    add_gadget_button_->SetDownImage(Variant(kSBButtonAddDown));
    add_gadget_button_->SetOverImage(Variant(kSBButtonAddOver));
    add_gadget_button_->SetTooltip(GM_("SIDEBAR_ADD_GADGETS_TOOLTIP"));

    menu_button_ = new ButtonElement(button_div, this, NULL);
    button_div->GetChildren()->InsertElement(menu_button_, NULL);
    menu_button_->SetImage(Variant(kSBButtonMenuUp));
    menu_button_->SetDownImage(Variant(kSBButtonMenuDown));
    menu_button_->SetOverImage(Variant(kSBButtonMenuOver));
    menu_button_->SetTooltip(GM_("SIDEBAR_MENU_BUTTON_TOOLTIP"));
    menu_button_->ConnectOnClickEvent(NewSlot(this, &Impl::OnMenuButtonClick));

    close_button_ = new ButtonElement(button_div, this, NULL);
    button_div->GetChildren()->InsertElement(close_button_, NULL);
    close_button_->SetImage(Variant(kSBButtonMinimizeUp));
    close_button_->SetDownImage(Variant(kSBButtonMinimizeDown));
    close_button_->SetOverImage(Variant(kSBButtonMinimizeOver));
    close_button_->SetTooltip(GM_("SIDEBAR_MINIMIZE_BUTTON_TOOLTIP"));

    Elements *buttons = button_div->GetChildren();
    double max_button_height = 0;
    double buttons_width = 0;
    for (int i = 0; i < 3; ++i) {
      BasicElement *button = buttons->GetItemByIndex(i);
      button->Layout();
      button->SetRelativePinY(0.5);
      button->SetRelativeY(0.5);
      button->SetPixelX(buttons_width);
      max_button_height = std::max(button->GetPixelHeight(), max_button_height);
      buttons_width += button->GetPixelWidth();
    }
    button_div->SetPixelWidth(buttons_width);
    top_div_->SetPixelHeight(std::max(icon->GetSrcHeight(), max_button_height));

    main_div_ = new DivElement(NULL, this, NULL);
    GetChildren()->InsertElement(main_div_, NULL);
    main_div_->SetPixelX(kBorderWidth);
    main_div_->SetPixelY(top_div_->GetPixelY() + top_div_->GetPixelHeight());
    children_ = main_div_->GetChildren();
  }

  void OnMenuButtonClick() {
    view_host_->ShowContextMenu(MouseEvent::BUTTON_LEFT);
  }

  int GetIndex(const BasicElement *element) const {
    ASSERT(element->IsInstanceOf(ViewElement::CLASS_ID));
    for (int i = 0; i < children_->GetCount(); ++i)
      if (element == children_->GetItemByIndex(i)) return i;
    return -1;
  }

  void InsertViewElement(int index, ViewElement *element) {
    ASSERT(index >= 0);
    ASSERT(element);
    int count = children_->GetCount();
    if (initializing_) {
      for (int i = 0; i < count; ++i) {
        ViewElement *e = down_cast<ViewElement*>(children_->GetItemByIndex(i));
        View *v = e->GetChildView();
        if (v) {
          SideBarViewHost *vh = down_cast<SideBarViewHost*>(v->GetViewHost());
          if (index <= vh->GetInitialIndex()) {
            children_->InsertElement(element, e);
            element = NULL;
            break;
          }
        }
      }
      if (element)
        children_->InsertElement(element, NULL);
    } else {
      if (index >= count) {
        children_->InsertElement(element, NULL);
      } else {
        BasicElement *e = children_->GetItemByIndex(index);
        if (e != element)
          children_->InsertElement(element, e);
      }
    }
    Layout();
  }

  void RemoveViewElement(ViewElement *element) {
    children_->RemoveElement(element);
  }

  void Layout() {
    double y = 0;
    int count = children_->GetCount();
    for (int i = 0; i < count; ++i) {
      ViewElement *e = down_cast<ViewElement *>(children_->GetItemByIndex(i));
      double width = main_div_->GetPixelWidth();
      double height = ceil(e->GetPixelHeight());
      // Just ignore the OnSizing result, to set child view's width forcely.
      e->OnSizing(&width, &height);
      e->SetSize(width, ceil(height));
      e->SetPixelX(0);
      e->SetPixelY(ceil(y));
      if (e->IsVisible())
        y += e->GetPixelHeight();
      y += kGadgetSpacing;
    }
    QueueDraw();
  }

  // *offset could be any value
  bool UpResize(bool do_resize, int index, double *offset) {
    double sign = *offset > 0 ? 1 : -1;
    double count = 0;
    while (*offset * sign > count * sign && index >= 0) {
      ViewElement *element =
          down_cast<ViewElement *>(children_->GetItemByIndex(index));
      double w = element->GetPixelWidth();
      double h = elements_height_[index] + count - *offset;
      // don't send non-positive resize request
      if (h <= .0) h = 1;
      if (element->OnSizing(&w, &h)) {
        double diff = std::min(sign * (elements_height_[index] - h),
                               sign * (*offset - count)) * sign;
        if (do_resize)
          element->SetSize(w, ceil(elements_height_[index] - diff));
        count += diff;
      } else {
        double oh = element->GetPixelHeight();
        double diff = std::min(sign * (elements_height_[index] - oh),
                               sign * (*offset - count)) * sign;
        if (diff > 0) count += diff;
      }
      index--;
    }
    if (do_resize)
      // recover upmost elements' size
      while (index >= 0) {
        ViewElement *element =
            down_cast<ViewElement *>(children_->GetItemByIndex(index));
        element->SetSize(main_div_->GetPixelWidth(), elements_height_[index]);
        index--;
      }
    DLOG("original: at last off: %.1lf, count: %.1lf", *offset, count);
    if (count == 0) return false;
    *offset = count;
    return true;
  }

  bool DownResize(bool do_resize, int index, double *offset) {
    double count = 0;
    if (blank_height_ > 0) count = std::max(-blank_height_, *offset);
    while (*offset < count && index < children_->GetCount()) {
      ViewElement *element =
          down_cast<ViewElement *>(children_->GetItemByIndex(index));
      double w = element->GetPixelWidth();
      double h = elements_height_[index] + *offset - count;
      // don't send non-positive resize request
      if (h <= .0) h = 1;
      if (element->OnSizing(&w, &h) && h < elements_height_[index]) {
        double diff = std::min(elements_height_[index] - h, count - *offset);
        if (do_resize) {
          element->SetSize(w, ceil(elements_height_[index] - diff));
        }
        count -= diff;
      } else {
        double oh = element->GetPixelHeight();
        double diff = std::min(elements_height_[index] - oh, count - *offset);
        if (diff > 0) count -= diff;
      }
      index++;
    }
    if (do_resize) {
      // recover upmost elemnts' size
      while (index < children_->GetCount()) {
        ViewElement *element =
            down_cast<ViewElement *>(children_->GetItemByIndex(index));
        element->SetSize(main_div_->GetPixelWidth(), elements_height_[index]);
        index++;
      }
      Layout();
    }
    if (count == 0) return false;
    *offset = count;
    return true;
  }

  double GetBlankHeight() const {
    int index = children_->GetCount();
    if (!index) return GetHeight();
    BasicElement *element = children_->GetItemByIndex(index - 1);
    return GetHeight() - element->GetPixelY() - element->GetPixelHeight();
  }

 public:
  SideBar *owner_;
  ViewHostInterface *view_host_;
  ViewElement *null_element_;

  std::vector<double> elements_height_;
  double blank_height_;
  double mouse_move_event_x_;
  double mouse_move_event_y_;
  bool hit_element_bottom_;
  bool hit_element_normal_part_;
  bool hit_sidebar_border_;
  HitTest hittest_;

  double original_width_;
  double original_height_;

  DivElement *top_div_;
  DivElement *main_div_;
  ButtonElement *add_gadget_button_;
  ButtonElement *menu_button_;
  ButtonElement *close_button_;

  Elements   *children_;

  bool initializing_;

  Signal4<void, View*, int, double, double> onundock_signal_;
  Signal1<void, View*> onclick_signal_;
  Signal1<void, MenuInterface *> onmenu_signal_;
};

SideBar::SideBar(ViewHostInterface *view_host)
  : impl_(new Impl(this, view_host)) {
}

SideBar::~SideBar() {
  delete impl_;
  impl_ = NULL;
}

void SideBar::SetInitializing(bool initializing) {
  impl_->initializing_ = initializing;
}

ViewHostInterface *SideBar::NewViewHost(int index) {
  return impl_->NewViewHost(index);
}

ViewHostInterface *SideBar::GetSideBarViewHost() const {
  return impl_->GetViewHost();
}

void SideBar::SetSize(double width, double height) {
  impl_->SetSize(width, height);
}

double SideBar::GetWidth() const {
  return impl_->GetWidth();
}

double SideBar::GetHeight() const {
  return impl_->GetHeight();
}

void SideBar::Show() {
  impl_->ShowView(false, 0, NULL);
}

void SideBar::Hide() {
  impl_->CloseView();
}

void SideBar::Minimize(bool vertical) {
  impl_->Minimize(vertical);
}

void SideBar::Restore() {
  impl_->Restore();
}

bool SideBar::IsMinimized() const {
  return impl_->IsMinimized();
}

int SideBar::GetIndexOfPosition(double y) const {
  return impl_->GetIndexOfPosition(y);
}

int SideBar::GetIndexOfView(const ViewInterface *view) const {
  return impl_->GetIndexOfView(view);
}

void SideBar::InsertPlaceholder(int index, double height) {
  return impl_->InsertPlaceholder(index, height);
}

void SideBar::ClearPlaceholder() {
  impl_->ClearPlaceholder();
}

void SideBar::EnumerateViews(Slot2<bool, int, View*> *slot) {
  impl_->EnumerateViews(slot);
}

Connection *SideBar::ConnectOnUndock(
    Slot4<void, View*, int, double, double> *slot) {
  return impl_->onundock_signal_.Connect(slot);
}

Connection *SideBar::ConnectOnClick(Slot1<void, View*> *slot) {
  return impl_->onclick_signal_.Connect(slot);
}

Connection *SideBar::ConnectOnAddGadget(Slot0<void> *slot) {
  return impl_->add_gadget_button_->ConnectOnClickEvent(slot);
}

Connection *SideBar::ConnectOnMenu(Slot1<void, MenuInterface *> *slot) {
  return impl_->onmenu_signal_.Connect(slot);
}

Connection *SideBar::ConnectOnClose(Slot0<void> *slot) {
  return impl_->close_button_->ConnectOnClickEvent(slot);
}

Connection *SideBar::ConnectOnSizeEvent(Slot0<void> *slot) {
  return impl_->ConnectOnSizeEvent(slot);
}

}  // namespace ggadget
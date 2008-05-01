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

#include "sidebar_gtk_host.h"

#include <gtk/gtk.h>
#include <string>
#include <map>
#include <ggadget/common.h>
#include <ggadget/decorated_view_host.h>
#include <ggadget/gadget.h>
#include <ggadget/gadget_consts.h>
#include <ggadget/gadget_manager_interface.h>
#include <ggadget/gtk/single_view_host.h>
#include <ggadget/gtk/utilities.h>
#include <ggadget/main_loop_interface.h>
#include <ggadget/messages.h>
#include <ggadget/menu_interface.h>
#include <ggadget/logger.h>
#include <ggadget/script_runtime_manager.h>
#include <ggadget/sidebar.h>
#include <ggadget/view.h>
#include <ggadget/view_element.h>

using namespace ggadget;
using namespace ggadget::gtk;

namespace hosts {
namespace gtk {

class SidebarGtkHost::Impl {
 public:
  // A special Host for Gadget browser to show browser in a decorated window.
  class GadgetBrowserHost : public HostInterface {
   public:
    GadgetBrowserHost(HostInterface *owner) : owner_(owner) { }
    virtual ViewHostInterface *NewViewHost(ViewHostInterface::Type type) {
      return new SingleViewHost(type, 1.0, true, true, true,
                                ViewInterface::DEBUG_DISABLED);
    }
    virtual void RemoveGadget(Gadget *gadget, bool save_data) {
      GetGadgetManager()->RemoveGadgetInstance(gadget->GetInstanceID());
    }
    virtual void DebugOutput(DebugLevel level, const char *message) const {
      owner_->DebugOutput(level, message);
    }
    virtual bool OpenURL(const char *url) const {
      return owner_->OpenURL(url);
    }
    virtual bool LoadFont(const char *filename) {
      return owner_->LoadFont(filename);
    }
    virtual void ShowGadgetAboutDialog(Gadget *gadget) {
      owner_->ShowGadgetAboutDialog(gadget);
    }
    virtual void Run() {}
   private:
    HostInterface *owner_;
  };

  class GadgetMoveClosure {
   public:
    GadgetMoveClosure(SidebarGtkHost::Impl *owner,
                      SingleViewHost *outer_view_host,
                      DecoratedViewHost *decorator_view_host,
                      View *view,
                      int height)
        : owner_(owner),
          outer_view_host_(outer_view_host),
          decorator_view_host_(decorator_view_host),
          view_(view),
          sidebar_(NULL),
          height_(height) {
      sidebar_ = gtk_widget_get_toplevel(GTK_WIDGET(down_cast<SingleViewHost *>(
            owner->side_bar_->GetViewHost())->GetNativeWidget()));
      AddConnection(outer_view_host_->ConnectOnMoveDrag(
            NewSlot(this, &GadgetMoveClosure::HandleMove)));
      AddConnection(outer_view_host_->ConnectOnEndMoveDrag(
            NewSlot(this, &GadgetMoveClosure::HandleMoveEnd)));
      AddConnection(decorator_view_host_->ConnectOnDock(
            NewSlot(this, &GadgetMoveClosure::HandleDock)));
    }
    ~GadgetMoveClosure() {
      for (size_t i = 0; i < connections_.size(); ++i)
        connections_[i]->Disconnect();
    }
    void AddConnection(Connection *connection) {
      connections_.push_back(connection);
    }
    void HandleMove(int button) {
      int h;
      if (IsOverlapWithSideBar(&h)) {
        owner_->side_bar_->InsertNullElement(h, view_);
        height_ = h;
      } else {
        owner_->side_bar_->ClearNullElement();
      }
    }
    void HandleMoveEnd(int button) {
      int h;
      owner_->side_bar_->ClearNullElement();
      if (IsOverlapWithSideBar(&h)) {
        view_->GetGadget()->SetDisplayTarget(Gadget::TARGET_SIDEBAR);
        height_ = h;
        HandleDock();
      }
    }
    void HandleDock() {
      owner_->Dock(view_, height_, true);
    }
   private:
    bool IsOverlapWithSideBar(int *height) {
      int x, y, w, h;
      GtkWidget *floating = gtk_widget_get_toplevel(GTK_WIDGET(
          outer_view_host_->GetNativeWidget()));
      gtk_window_get_position(GTK_WINDOW(floating), &x, &y);
      gtk_window_get_size(GTK_WINDOW(floating), &w, &h);
      int sx, sy, sw, sh;
      gtk_window_get_position(GTK_WINDOW(sidebar_), &sx, &sy);
      gtk_window_get_size(GTK_WINDOW(sidebar_), &sw, &sh);
      if ((x + w >= sx) && (sx + sw >= x)) {
        if (height) {
          int dummy;
          gtk_widget_get_pointer(sidebar_, &dummy, height);
        }
        return true;
      }
      return false;
    }
    SidebarGtkHost::Impl *owner_;
    SingleViewHost *outer_view_host_;
    DecoratedViewHost *decorator_view_host_;
    View *view_;
    GtkWidget *sidebar_;
    int height_;
    std::vector<Connection *> connections_;
  };

  Impl(SidebarGtkHost *owner, bool decorated, int view_debug_mode)
    : gadget_browser_host_(owner),
      owner_(owner),
      decorated_(decorated),
      view_debug_mode_(static_cast<ViewInterface::DebugMode>(view_debug_mode)),
      view_host_(NULL),
      expanded_original_(NULL),
      expanded_popout_(NULL),
      side_bar_(NULL),
      gadget_manager_(GetGadgetManager()) {
    ASSERT(gadget_manager_);
    ScriptRuntimeManager::get()->ConnectErrorReporter(
        NewSlot(this, &Impl::ReportScriptError));
    view_host_ = new SingleViewHost(ViewHostInterface::VIEW_HOST_MAIN, 1.0,
                                    decorated, false, true, view_debug_mode_);
    side_bar_ = new SideBar(owner_, view_host_);
    side_bar_->SetAddGadgetSlot(NewSlot(this, &Impl::AddGadgetHandler));
    side_bar_->SetMenuSlot(NewSlot(this, &Impl::MenuGenerator));
    side_bar_->SetCloseSlot(NewSlot(this, &Impl::ExitHandler));

    side_bar_->ConnectOnUndock(NewSlot(this, &Impl::HandleUndock));
    side_bar_->ConnectOnPopIn(NewSlot(this, &Impl::HandleGeneralPopIn));
  }

  ~Impl() {
    for (GadgetsMap::iterator it = gadgets_.begin();
         it != gadgets_.end(); ++it)
      delete it->second;

    delete side_bar_;
  }

  void SetupUI() {
    // add the menu into the main widget
    main_widget_ = gtk_widget_get_toplevel(GTK_WIDGET(
        view_host_->GetNativeWidget()));
#ifdef _DEBUG
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(main_widget_), FALSE);
#endif
    gtk_window_set_title(GTK_WINDOW(main_widget_), "Google Gadgets");
    gtk_widget_show(main_widget_);
    g_assert(GTK_WIDGET_REALIZED(main_widget_));
  }

  bool ConfirmGadget(int id) {
    std::string path = gadget_manager_->GetGadgetInstancePath(id);
    StringMap data;
    if (!Gadget::GetGadgetManifest(path.c_str(), &data))
      return false;

    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "%s\n\n%s\n%s\n\n%s%s",
        GM_("GADGET_CONFIRM_MESSAGE"), data[kManifestName].c_str(),
        gadget_manager_->GetGadgetInstanceDownloadURL(id).c_str(),
        GM_("GADGET_DESCRIPTION"), data[kManifestDescription].c_str());

    GdkScreen *screen;
    gdk_display_get_pointer(gdk_display_get_default(), &screen,
                            NULL, NULL, NULL);
    gtk_window_set_screen(GTK_WINDOW(dialog), screen);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(dialog), GM_("GADGET_CONFIRM_TITLE"));
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return result == GTK_RESPONSE_YES;
  }

  bool NewGadgetInstanceCallback(int id) {
    if (gadget_manager_->IsGadgetInstanceTrusted(id) ||
        ConfirmGadget(id)) {
      return AddGadgetInstanceCallback(id);
    }
    return false;
  }

  bool AddGadgetInstanceCallback(int id) {
    std::string options = gadget_manager_->GetGadgetInstanceOptionsName(id);
    std::string path = gadget_manager_->GetGadgetInstancePath(id);
    if (options.length() && path.length()) {
      bool result = LoadGadget(path.c_str(), options.c_str(), id);
      LOG("SidebarGtkHost: Load gadget %s, with option %s, %s",
          path.c_str(), options.c_str(), result ? "succeeded" : "failed");
    }
    return true;
  }

  bool Dock(View *view, int height, bool force_insert) {
    view->GetGadget()->SetDisplayTarget(Gadget::TARGET_SIDEBAR);
    DLOG("Dock in SidebarGtkHost, view: %p", view);
    ViewHostInterface *view_host =
        side_bar_->NewViewHost(ViewHostInterface::VIEW_HOST_MAIN, height);
    DecoratedViewHost *decorator =
        new DecoratedViewHost(view_host, DecoratedViewHost::MAIN_DOCKED, false);
    decorator->ConnectOnUndock(NewSlot(this, &Impl::HandleFloatingUndock));
    decorator->ConnectOnClose(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnCloseHandler)));
    decorator->ConnectOnPopOut(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnPopOutHandler)));
    decorator->ConnectOnPopIn(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnPopInHandler)));
    ViewHostInterface *old = view->SwitchViewHost(decorator);
    if (old) old->Destroy();
    side_bar_->Layout();
    return true;
  }

  class SlotPostCallback : public WatchCallbackInterface {
   public:
    SlotPostCallback(Slot1<void, int> *slot) : slot_(slot) {}
    virtual bool Call(MainLoopInterface *main_loop, int watch_id) {
      DLOG("call the slot now");
      (*slot_)(MouseEvent::BUTTON_LEFT);
      return false;
    }
    virtual void OnRemove(MainLoopInterface *main_loop, int watch_id) {
      delete slot_;
      delete this;
    }
    Slot1<void, int> *slot_;
  };

  bool Undock(View *view, bool move_to_cursor) {
    view->GetGadget()->SetDisplayTarget(Gadget::TARGET_FLOATING_VIEW);
    ViewElement *ele = side_bar_->FindViewElementByView(view);
    int wx = ele ? static_cast<int>(ele->GetPixelX()) : 0;
    int wy = ele ? static_cast<int>(ele->GetPixelY()) : 0;
    ViewHostInterface *new_host = NewSingleViewHost(view, true, wy);
    ViewHostInterface *old = view->SwitchViewHost(new_host);
    if (old) old->Destroy();
    bool r = view->ShowView(false, 0, NULL);
    if (move_to_cursor) {
      side_bar_->InsertNullElement(wy, view);
      // move new gadget to a proper place
      double x, y;
      side_bar_->GetPointerPosition(&x, &y);
      gdk_pointer_ungrab(gtk_get_current_event_time());
      int px, py;
      gdk_display_get_pointer(gdk_display_get_default(), NULL, &px, &py, NULL);
      GtkWidget *window =
          gtk_widget_get_toplevel(GTK_WIDGET(new_host->GetNativeWidget()));
      gtk_window_move(GTK_WINDOW(window),
                      px - static_cast<int>(x) + wx,
                      py - static_cast<int>(y) + wy);
      DLOG("move window, x: %f y: %f px: %d py: %d, wx: %d, wy: %d",
           x, y, px, py, wx, wy);
      gdk_pointer_grab(window->window, FALSE,
                       (GdkEventMask)(GDK_BUTTON_RELEASE_MASK |
                                      GDK_POINTER_MOTION_MASK |
                                      GDK_POINTER_MOTION_HINT_MASK),
                       NULL, NULL, gtk_get_current_event_time());
      gtk_window_deiconify(GTK_WINDOW(window));
      gdk_window_focus(window->window, gtk_get_current_event_time());
      gtk_window_set_transient_for(GTK_WINDOW(window),
                                   GTK_WINDOW(main_widget_));
      // posted the slot in to main loop, to avoid it is run before the window is
      // moved
      GetGlobalMainLoop()->AddTimeoutWatch(0, new SlotPostCallback(NewSlot(
          new_host, &ViewHostInterface::BeginMoveDrag)));
    }
    return r;
  }

  void HandleUndock() {
    ViewElement *element = side_bar_->GetMouseOverElement();
    if (element) {
      Gadget *gadget = element->GetChildView()->GetGadget();
      Undock(gadget->GetMainView(), true);
    }
  }

  // handle undock event triggered by click menu, the undocked gadget should
  // not move with cursor
  void HandleFloatingUndock() {
    ViewElement *element = side_bar_->GetMouseOverElement();
    if (element) {
      Gadget *gadget = element->GetChildView()->GetGadget();
      Undock(gadget->GetMainView(), false);
    }
  }

  void HandleGeneralPopIn() {
    OnPopInHandler(expanded_original_);
  }

  void InitGadgets() {
    gadget_manager_->ConnectOnNewGadgetInstance(
        NewSlot(this, &Impl::NewGadgetInstanceCallback));
  }

  bool LoadGadget(const char *path, const char *options_name, int instance_id) {
    if (gadgets_.find(instance_id) != gadgets_.end()) {
      // Gadget is already loaded.
      return true;
    }

    Gadget *gadget = new Gadget(
        owner_, path, options_name, instance_id,
        gadget_manager_->IsGadgetInstanceTrusted(instance_id));

    DLOG("Gadget %p with view %p", gadget, gadget->GetMainView());

    if (!gadget->IsValid()) {
      LOG("Failed to load gadget %s", path);
      delete gadget;
      return false;
    }

    if (!Dock(gadget->GetMainView(), 0, false)) {
      DLOG("Dock view(%p) failed.", gadget->GetMainView());
      Undock(gadget->GetMainView(), false);
    }

    if (!gadget->ShowMainView()) {
      LOG("Failed to show main view of gadget %s", path);
      delete gadget;
      return false;
    }

    gadgets_[instance_id] = gadget;
    return true;
  }

  ViewHostInterface *NewSingleViewHost(View *view,
                                       bool remove_on_close, int height) {
    SingleViewHost *view_host =
      new SingleViewHost(ViewHostInterface::VIEW_HOST_MAIN, 1.0,
          decorated_, remove_on_close, false, view_debug_mode_);
    DLOG("New decorator for vh %p", view_host);
    DecoratedViewHost *decorator =
        new DecoratedViewHost(view_host, DecoratedViewHost::MAIN_STANDALONE,
                              true);
    decorator->ConnectOnClose(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnCloseHandler)));
    decorator->ConnectOnPopIn(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnPopInHandler)));
    GadgetMoveClosure *closure =
        new GadgetMoveClosure(this, view_host, decorator, view, height);
    move_slots_[view->GetGadget()] = closure;
    DLOG("New decorator %p with vh %p", decorator, view_host);
    return decorator;
  }

  ViewHostInterface *NewViewHost(ViewHostInterface::Type type) {
    ViewHostInterface *view_host;
    DecoratedViewHost *decorator;
    switch (type) {
      case ViewHostInterface::VIEW_HOST_MAIN:
        view_host = side_bar_->NewViewHost(type, 0);
        decorator = new DecoratedViewHost(view_host,
                                          DecoratedViewHost::MAIN_DOCKED,
                                          false);
        decorator->ConnectOnUndock(NewSlot(this, &Impl::HandleFloatingUndock));
        break;
      case ViewHostInterface::VIEW_HOST_OPTIONS:
        // No decorator for options view.
        view_host = new SingleViewHost(type, 1.0, true, true, true,
                                       view_debug_mode_);
        return view_host;
      default:
        view_host = new SingleViewHost(type, 1.0, decorated_, true, true,
                                       view_debug_mode_);
        decorator = new DecoratedViewHost(view_host, DecoratedViewHost::DETAILS,
                                          true);
        break;
    }
    decorator->ConnectOnClose(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnCloseHandler)));
    decorator->ConnectOnPopIn(NewFunctorSlot<void>(
        DecoratorSignalHandler(decorator, this, &Impl::OnPopInHandler)));
    return view_host;
  }

  //TODO: refactor these methods/subclass which are the same w/ simple_gtk_host
  class DecoratorSignalHandler {
   public:
    DecoratorSignalHandler(DecoratedViewHost *decorator, Impl *impl,
                           void (Impl::*handler)(DecoratedViewHost *))
      : decorator_(decorator), impl_(impl), handler_(handler) {
    }
    void operator()() const {
      (impl_->*handler_)(decorator_);
    }
    // No use.
    bool operator==(const DecoratorSignalHandler &) const { return false; }

   private:
    DecoratedViewHost *decorator_;
    Impl *impl_;
    void (Impl::*handler_)(DecoratedViewHost *);
  };

  void RemoveGadget(Gadget *gadget, bool save_data) {
    ASSERT(gadget);
    ViewInterface *main_view = gadget->GetMainView();

    // If this gadget is popped out, popin it first.
    if (main_view->GetViewHost() == expanded_popout_) {
      OnPopInHandler(expanded_original_);
    }

    gadget_manager_->RemoveGadgetInstance(gadget->GetInstanceID());
  }
  void OnCloseHandler(DecoratedViewHost *decorated) {
    ViewInterface *child = decorated->GetView();
    Gadget *gadget = child ? child->GetGadget() : NULL;

    ASSERT(gadget);
    if (!gadget) return;

    switch (decorated->GetDecoratorType()) {
      case DecoratedViewHost::MAIN_STANDALONE:
      case DecoratedViewHost::MAIN_DOCKED:
        gadget->RemoveMe(true);
        break;
      case DecoratedViewHost::MAIN_EXPANDED:
        if (expanded_original_ &&
            expanded_popout_ == decorated)
          OnPopInHandler(expanded_original_);
        break;
      case DecoratedViewHost::DETAILS:
        gadget->CloseDetailsView();
        break;
      default:
        ASSERT("Invalid decorator type.");
    }
  }

  void OnPopOutHandler(DecoratedViewHost *decorated) {
    if (expanded_original_) {
      OnPopInHandler(expanded_original_);
    }

    ViewInterface *child = decorated->GetView();
    ASSERT(child);
    if (child) {
      ViewElement *ele = side_bar_->SetPopoutedView(child);
      expanded_original_ = decorated;
      SingleViewHost *svh = new SingleViewHost(
          ViewHostInterface::VIEW_HOST_MAIN, 1.0, false, false, false,
          static_cast<ViewInterface::DebugMode>(view_debug_mode_));
      svh->ConnectOnBeginMoveDrag(NewSlot(this, &Impl::HandlePopoutViewMove));
      expanded_popout_ =
          new DecoratedViewHost(svh, DecoratedViewHost::MAIN_EXPANDED, true);
      expanded_popout_->ConnectOnClose(NewFunctorSlot<void>(
        DecoratorSignalHandler(expanded_popout_, this, &Impl::OnCloseHandler)));

      // Send popout event to decorator first.
      SimpleEvent event(Event::EVENT_POPOUT);
      expanded_original_->GetDecoratedView()->OnOtherEvent(event);

      child->SwitchViewHost(expanded_popout_);
      expanded_popout_->ShowView(false, 0, NULL);
      SetProperPopoutPosition(ele, expanded_popout_);
    }
  }

  void OnPopInHandler(DecoratedViewHost *decorated) {
    if (expanded_original_ == decorated && expanded_popout_) {
      ViewInterface *child = expanded_popout_->GetView();
      ASSERT(child);
      if (child) {
        ViewHostInterface *old_host = child->SwitchViewHost(expanded_original_);
        SimpleEvent event(Event::EVENT_POPIN);
        expanded_original_->GetDecoratedView()->OnOtherEvent(event);
        // The old host must be destroyed after sending onpopin event.
        old_host->Destroy();
        expanded_original_ = NULL;
        expanded_popout_ = NULL;
        side_bar_->SetPopoutedView(NULL);
      }
    }
  }
  //Refactor To here

  void SetProperPopoutPosition(const BasicElement *element_in_sidebar,
                               const DecoratedViewHost *popout_view_host) {
    double ex, ey;
    element_in_sidebar->SelfCoordToViewCoord(0, 0, &ex, &ey);
    int sx, sy;
    //FIXME: should use Decorator View's width, but now it is 0...
    int pw = static_cast<int>(popout_view_host->GetView()->GetWidth());
    gtk_window_get_position(GTK_WINDOW(main_widget_), &sx, &sy);
    GtkWidget *win = gtk_widget_get_toplevel(GTK_WIDGET(
        popout_view_host->GetNativeWidget()));
    DLOG("Popout, sx: %d, sy: %d, pw: %d, ey: %f", sx, sy, pw, ey);
    if (sx > pw) {
      gtk_window_move(GTK_WINDOW(win), sx - pw, sy + static_cast<int>(ey));
    } else {
      int sw, sh;
      gtk_window_get_size(GTK_WINDOW(main_widget_), &sw, &sh);
      gtk_window_move(GTK_WINDOW(win), sx + pw, sy + static_cast<int>(ey));
    }
  }

  void RemoveGadgetInstanceCallback(int instance_id) {
    GadgetsMap::iterator it = gadgets_.find(instance_id);

    if (it != gadgets_.end()) {
      delete it->second;
      gadgets_.erase(it);
    } else {
      LOG("Can't find gadget instance %d", instance_id);
    }
  }

  void AddGadgetHandler() {
    DLOG("Add Gadget now");
    gadget_manager_->ShowGadgetBrowserDialog(&gadget_browser_host_);
  }

  bool HandlePopoutViewMove(int button) {
    // popout view is not allowed to move, just return true
    return true;
  }

  void AddGadgetHandlerWithOneArg(const char *str) {
    DLOG("Add Gadget now, str: %s", str);
    gadget_manager_->ShowGadgetBrowserDialog(&gadget_browser_host_);
  }

  void HandleMenuAutoHide(const char *str) {
    //TODO;
  }

  void HandleMenuAlwaysOnTop(const char *str) {
    //TODO;
  }

  void HandleMenuReplaceSidebar(const char *str) {
  }

  void HandleMenuFontSizeChange(const char *str) {
    //TODO;
  }

  void HandleMenuClose(const char *str) {
    ExitHandler();
  }

  bool MenuGenerator(MenuInterface *menu) {
    menu->AddItem(GM_("MENU_ITEM_ADD_GADGETS"), 0,
                  NewSlot(this, &Impl::AddGadgetHandlerWithOneArg));
    menu->AddItem("", MenuInterface::MENU_ITEM_FLAG_SEPARATOR, NULL);
    menu->AddItem(GM_("MENU_ITEM_AUTO_HIDE"),
                  MenuInterface::MENU_ITEM_FLAG_CHECKED,
                  NewSlot(this, &Impl::HandleMenuAutoHide));
    menu->AddItem(GM_("MENU_ITEM_ALWAYS_ON_TOP"),
                  MenuInterface::MENU_ITEM_FLAG_CHECKED,
                  NewSlot(this, &Impl::HandleMenuAlwaysOnTop));
    {
      MenuInterface *sub = menu->AddPopup(GM_("MENU_ITEM_ADD_SIDEBAR"));
      sub->AddItem(GM_("MENU_ITEM_LEFT"), MenuInterface::MENU_ITEM_FLAG_CHECKED,
                  NewSlot(this, &Impl::HandleMenuReplaceSidebar));
      sub->AddItem(GM_("MENU_ITEM_RIGHT"), MenuInterface::MENU_ITEM_FLAG_CHECKED,
                  NewSlot(this, &Impl::HandleMenuReplaceSidebar));
    }
    {
      MenuInterface *sub = menu->AddPopup(GM_("MENU_ITEM_FONT_SIZE"));
      sub->AddItem(GM_("MENU_ITEM_FONT_SIZE_LARGE"),
                   MenuInterface::MENU_ITEM_FLAG_CHECKED,
                   NewSlot(this, &Impl::HandleMenuFontSizeChange));
      sub->AddItem(GM_("MENU_ITEM_FONT_SIZE_DEFAULT"),
                   MenuInterface::MENU_ITEM_FLAG_CHECKED,
                   NewSlot(this, &Impl::HandleMenuFontSizeChange));
      sub->AddItem(GM_("MENU_ITEM_FONT_SIZE_SMALL"),
                   MenuInterface::MENU_ITEM_FLAG_CHECKED,
                   NewSlot(this, &Impl::HandleMenuFontSizeChange));
    }
    menu->AddItem("", MenuInterface::MENU_ITEM_FLAG_SEPARATOR, NULL);
    menu->AddItem(GM_("MENU_ITEM_CLOSE"), 0,
                  NewSlot(this, &Impl::HandleMenuClose));
    return true;
  }

  void ExitHandler() {
    gtk_main_quit();
  }

  void DebugOutput(DebugLevel level, const char *message) const {
    const char *str_level = "";
    switch (level) {
      case DEBUG_TRACE: str_level = "TRACE: "; break;
      case DEBUG_INFO: str_level = "INFO: "; break;
      case DEBUG_WARNING: str_level = "WARNING: "; break;
      case DEBUG_ERROR: str_level = "ERROR: "; break;
      default: break;
    }
    LOG("%s%s", str_level, message);
  }

  void ReportScriptError(const char *message) {
    DebugOutput(DEBUG_ERROR,
                (std::string("Script error: " ) + message).c_str());
  }

  void LoadGadgets() {
    gadget_manager_->EnumerateGadgetInstances(
        NewSlot(this, &Impl::AddGadgetInstanceCallback));
  }

  GadgetBrowserHost gadget_browser_host_;

  typedef std::map<Gadget *, GadgetMoveClosure *> GadgetMoveClosureMap;
  GadgetMoveClosureMap move_slots_;

  typedef std::map<int, Gadget *> GadgetsMap;
  GadgetsMap gadgets_;
  SidebarGtkHost *owner_;

  bool decorated_;
  ViewInterface::DebugMode view_debug_mode_;

  SingleViewHost *view_host_;
  DecoratedViewHost *expanded_original_;
  DecoratedViewHost *expanded_popout_;
  SideBar *side_bar_;

  GadgetManagerInterface *gadget_manager_;
  GtkWidget *main_widget_;
};

SidebarGtkHost::SidebarGtkHost(bool decorated, int view_debug_mode)
  : impl_(new Impl(this, decorated, view_debug_mode)) {
  impl_->SetupUI();
  impl_->InitGadgets();
}

SidebarGtkHost::~SidebarGtkHost() {
  delete impl_;
  impl_ = NULL;
}

ViewHostInterface *SidebarGtkHost::NewViewHost(ViewHostInterface::Type type) {
  return impl_->NewViewHost(type);
}

void SidebarGtkHost::RemoveGadget(Gadget *gadget, bool save_data) {
  return impl_->RemoveGadget(gadget, save_data);
}

void SidebarGtkHost::DebugOutput(DebugLevel level, const char *message) const {
  impl_->DebugOutput(level, message);
}

bool SidebarGtkHost::OpenURL(const char *url) const {
  return ggadget::gtk::OpenURL(url);
}

bool SidebarGtkHost::LoadFont(const char *filename) {
  return ggadget::gtk::LoadFont(filename);
}

void SidebarGtkHost::ShowGadgetAboutDialog(ggadget::Gadget *gadget) {
  ggadget::gtk::ShowGadgetAboutDialog(gadget);
}

void SidebarGtkHost::Run() {
  impl_->LoadGadgets();
  gtk_main();
}

} // namespace gtk
} // namespace hosts
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

#include <sys/time.h>

#include <QtGui/QCursor>
#include <QtGui/QToolTip>
#include <QtGui/QMessageBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QVBoxLayout>
#include <QtGui/QInputDialog>
#include <ggadget/file_manager_interface.h>
#include <ggadget/gadget_consts.h>
#include <ggadget/gadget.h>
#include <ggadget/logger.h>
#include <ggadget/messages.h>
#include <ggadget/options_interface.h>
#include <ggadget/script_context_interface.h>
#include <ggadget/script_runtime_interface.h>
#include <ggadget/script_runtime_manager.h>
#include "utilities.h"
#include "qt_view_host.h"
#include "qt_menu.h"
#include "qt_graphics.h"
#include "qt_view_widget.h"
#include "qt_view_host.moc"

namespace ggadget {
namespace qt {

class QtViewHost::Impl {
 public:
  Impl(ViewHostInterface::Type type,
       double zoom,
       bool decorated,
       bool record_states,
       int debug_mode)
    : view_(NULL),
      type_(type),
      widget_(NULL),
      window_(NULL),
      dialog_(NULL),
      debug_mode_(debug_mode),
      zoom_(zoom),
      decorated_(decorated),
      record_states_(record_states),
      onoptionchanged_connection_(NULL),
      feedback_handler_(NULL),
      composite_(false),
      input_shape_mask_(true),
      keep_above_(false),
      qt_obj_(new QtViewHostObject(this)) {
    if (type == ViewHostInterface::VIEW_HOST_MAIN)
      composite_ = true;
    // debug_mode_ = ViewInterface::DEBUG_ALL;
  }

  ~Impl() {
    if (onoptionchanged_connection_)
      onoptionchanged_connection_->Disconnect();

    Detach();

    if (qt_obj_) delete qt_obj_;
  }

  void Detach() {
    SaveWindowStates();
    view_ = NULL;
    if (window_)
      delete window_;
    if (dialog_)
      delete dialog_;
    window_ = widget_ = NULL;
    dialog_ = NULL;
    if (feedback_handler_) {
      delete feedback_handler_;
      feedback_handler_ = NULL;
    }
  }

  std::string GetViewPositionOptionPrefix() {
    switch (type_) {
      case ViewHostInterface::VIEW_HOST_MAIN:
        return "main_view";
      case ViewHostInterface::VIEW_HOST_OPTIONS:
        return "options_view";
      case ViewHostInterface::VIEW_HOST_DETAILS:
        return "details_view";
      default:
        return "view";
    }
    return "";
  }

  void SaveWindowStates() {
    if (view_ && view_->GetGadget() && type_ == VIEW_HOST_MAIN && window_) {
      OptionsInterface *opt = view_->GetGadget()->GetOptions();
      std::string opt_prefix = GetViewPositionOptionPrefix();
      DLOG("Save:%d, %d", window_->pos().x(), window_->pos().y());
      opt->PutInternalValue((opt_prefix + "_x").c_str(),
                            Variant(window_->pos().x()));
      opt->PutInternalValue((opt_prefix + "_y").c_str(),
                            Variant(window_->pos().y()));
      opt->PutInternalValue((opt_prefix + "_keep_above").c_str(),
                            Variant(keep_above_));
    }
  }

  void LoadWindowStates() {
    if (view_ && view_->GetGadget() && type_ == VIEW_HOST_MAIN && window_) {
      OptionsInterface *opt = view_->GetGadget()->GetOptions();
      std::string opt_prefix = GetViewPositionOptionPrefix();
      Variant vx = opt->GetInternalValue((opt_prefix + "_x").c_str());
      Variant vy = opt->GetInternalValue((opt_prefix + "_y").c_str());
      int x, y;
      if (vx.ConvertToInt(&x) && vy.ConvertToInt(&y)) {
        DLOG("Restore:%d, %d", x, y);
        window_->move(x, y);
      } else {
        // Always place the window to the center of the screen if the window
        // position was not saved before.
        // gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
      }
      Variant keep_above =
          opt->GetInternalValue((opt_prefix + "_keep_above").c_str());
      if (keep_above.type() == Variant::TYPE_BOOL &&
          VariantValue<bool>()(keep_above)) {
        KeepAboveMenuCallback(NULL, true);
      } else {
        KeepAboveMenuCallback(NULL, false);
      }
    }
  }

  bool ShowView(bool modal, int flags,
                Slot1<void, int> *feedback_handler) {
    ASSERT(view_);
    if (feedback_handler_ && feedback_handler_ != feedback_handler)
      delete feedback_handler_;
    feedback_handler_ = feedback_handler;

    if (widget_) {
      // we just move existing widget_ to the front
      widget_->hide();
      widget_->show();
      return true;
    }

    widget_ = new QtViewWidget(view_, composite_, decorated_);
    // Initialize window and widget.
    if (type_ == ViewHostInterface::VIEW_HOST_OPTIONS) {
      QVBoxLayout *layout = new QVBoxLayout();
      widget_->setFixedSize(D2I(view_->GetWidth()), D2I(view_->GetHeight()));
      layout->addWidget(widget_);
      ASSERT(!dialog_);
      dialog_ = new QDialog();

      QDialogButtonBox::StandardButtons what_buttons = 0;
      if (flags & ViewInterface::OPTIONS_VIEW_FLAG_OK)
        what_buttons |= QDialogButtonBox::Ok;

      if (flags & ViewInterface::OPTIONS_VIEW_FLAG_CANCEL)
        what_buttons |= QDialogButtonBox::Cancel;

      if (what_buttons != 0) {
        QDialogButtonBox *buttons = new QDialogButtonBox(what_buttons);
        if (flags & ViewInterface::OPTIONS_VIEW_FLAG_OK)
          dialog_->connect(buttons, SIGNAL(accepted()),
                           qt_obj_, SLOT(OnOptionViewOK()));
        if (flags & ViewInterface::OPTIONS_VIEW_FLAG_CANCEL)
          dialog_->connect(buttons, SIGNAL(rejected()),
                           qt_obj_, SLOT(OnOptionViewCancel()));
        layout->addWidget(buttons);
      }

      dialog_->setLayout(layout);
      dialog_->setWindowTitle(caption_);

      if (modal) {
        dialog_->exec();
      } else {
        dialog_->show();
      }
    } else {
      window_ = widget_;
      window_->setWindowTitle(caption_);
      if (record_states_) LoadWindowStates();
      window_->setAttribute(Qt::WA_DeleteOnClose, true);

      if (type_ == ViewHostInterface::VIEW_HOST_MAIN) {
        widget_->EnableInputShapeMask(input_shape_mask_);
      }
      widget_->connect(widget_, SIGNAL(destroyed(QObject*)),
                       qt_obj_, SLOT(OnViewWidgetClose(QObject*)));
      window_->show();
    }
    return true;
  }

  void KeepAboveMenuCallback(const char *, bool keep_above) {
    if (keep_above_ != keep_above) {
      keep_above_ = keep_above;
      if (window_) {
        widget_->SetKeepAbove(keep_above);
      }
    }
  }

  bool ShowContextMenu(int button) {
    ASSERT(view_);
    context_menu_.clear();
    QtMenu qt_menu(&context_menu_);
    if (view_->OnAddContextMenuItems(&qt_menu) &&
        type_ == VIEW_HOST_MAIN) {
      qt_menu.AddItem(
          GM_("MENU_ITEM_ALWAYS_ON_TOP"),
          keep_above_ ? MenuInterface::MENU_ITEM_FLAG_CHECKED : 0,
          NewSlot(this, &Impl::KeepAboveMenuCallback, !keep_above_),
          MenuInterface::MENU_ITEM_PRI_HOST);
    }

    if (!context_menu_.isEmpty()) {
      context_menu_.popup(QCursor::pos());
      return true;
    } else {
      return false;
    }
  }

  void HandleOptionViewResponse(ViewInterface::OptionsViewFlags flag) {
    if (feedback_handler_) {
      (*feedback_handler_)(flag);
      delete feedback_handler_;
      feedback_handler_ = NULL;
    }
    dialog_->hide();
  }

  void HandleDetailsViewClose() {
    if (feedback_handler_) {
      (*feedback_handler_)(ViewInterface::DETAILS_VIEW_FLAG_NONE);
      delete feedback_handler_;
      feedback_handler_ = NULL;
    }
  }

  void SetVisibility(bool flag) {
    if (!window_) return;
    if (flag) {
      widget_->hide();
      widget_->show();
      widget_->SkipTaskBar();
      LoadWindowStates();
    } else {
      SaveWindowStates();
      widget_->hide();
    }
  }

  ViewInterface *view_;
  ViewHostInterface::Type type_;
  QtViewWidget *widget_;
  QWidget *window_;     // Top level window of the view
  QDialog *dialog_;     // Top level window of the view
  int debug_mode_;
  double zoom_;
  bool decorated_;      // frameless or not
  bool record_states_;
  Connection *onoptionchanged_connection_;

  Slot1<void, int> *feedback_handler_;

  bool composite_;
  bool input_shape_mask_;
  bool keep_above_;
  QtViewHostObject *qt_obj_;    // used for handling qt signal
  QString caption_;
  QMenu context_menu_;
};

void QtViewHostObject::OnOptionViewOK() {
  owner_->HandleOptionViewResponse(ViewInterface::OPTIONS_VIEW_FLAG_OK);
}

void QtViewHostObject::OnOptionViewCancel() {
  owner_->HandleOptionViewResponse(ViewInterface::OPTIONS_VIEW_FLAG_CANCEL);
}

void QtViewHostObject::OnViewWidgetClose(QObject *obj) {
  if (owner_->type_ == ViewHostInterface::VIEW_HOST_DETAILS)
    owner_->HandleDetailsViewClose();
  owner_->window_ = NULL;
  owner_->widget_ = NULL;
}

void QtViewHostObject::OnShow(bool flag) {
  owner_->SetVisibility(flag);
}

QtViewHost::QtViewHost(ViewHostInterface::Type type,
                       double zoom, bool decorated,
                       bool record_states, int debug_mode)
  : impl_(new Impl(type, zoom, decorated, record_states, debug_mode)) {
}

QtViewHost::~QtViewHost() {
  if (impl_) delete impl_;
}

ViewHostInterface::Type QtViewHost::GetType() const {
  return impl_->type_;
}

ViewInterface *QtViewHost::GetView() const {
  return impl_->view_;
}

GraphicsInterface *QtViewHost::NewGraphics() const {
  return new QtGraphics(impl_->zoom_);
}

void *QtViewHost::GetNativeWidget() const {
  return impl_->widget_;
}

void QtViewHost::Destroy() {
  delete this;
}

void QtViewHost::SetView(ViewInterface *view) {
  if (impl_->view_ == view) return;
  impl_->Detach();
  if (view == NULL) return;
  impl_->view_ = view;
}

void QtViewHost::ViewCoordToNativeWidgetCoord(
    double x, double y, double *widget_x, double *widget_y) const {
  double zoom = impl_->view_->GetGraphics()->GetZoom();
  if (widget_x)
    *widget_x = x * zoom;
  if (widget_y)
    *widget_y = y * zoom;
}

void QtViewHost::NativeWidgetCoordToViewCoord(
    double x, double y, double *view_x, double *view_y) const {
  double zoom = impl_->view_->GetGraphics()->GetZoom();
  if (zoom == 0) return;
  if (view_x) *view_x = x / zoom;
  if (view_y) *view_y = y / zoom;
}

void QtViewHost::QueueDraw() {
  if (impl_->widget_)
    impl_->widget_->update();
}

void QtViewHost::QueueResize() {
  // TODO
}

void QtViewHost::EnableInputShapeMask(bool enable) {
  if (impl_->input_shape_mask_ != enable) {
    impl_->input_shape_mask_ = enable;
    if (impl_->widget_) impl_->widget_->EnableInputShapeMask(enable);
  }
}

void QtViewHost::SetResizable(ViewInterface::ResizableMode mode) {
  // TODO:
}

void QtViewHost::SetCaption(const char *caption) {
  impl_->caption_ = QString::fromUtf8(caption);
  if (impl_->window_)
    impl_->window_->setWindowTitle(impl_->caption_);
}

void QtViewHost::SetShowCaptionAlways(bool always) {
}

void QtViewHost::SetCursor(int type) {
  QCursor cursor(GetQtCursorShape(type));
  impl_->widget_->setCursor(cursor);
}

void QtViewHost::SetTooltip(const char *tooltip) {
  QToolTip::showText(QCursor::pos(), QString::fromUtf8(tooltip));
}

bool QtViewHost::ShowView(bool modal, int flags,
                          Slot1<void, int> *feedback_handler) {
  return impl_->ShowView(modal, flags, feedback_handler);
}

void QtViewHost::CloseView() {
  if (impl_->window_) {
    impl_->SaveWindowStates();
    delete impl_->window_;
    impl_->window_ = NULL;
    impl_->widget_ = NULL;
  }
  ASSERT(!impl_->widget_);
}

bool QtViewHost::ShowContextMenu(int button) {
  return impl_->ShowContextMenu(button);
}

void QtViewHost::BeginMoveDrag(int button) {
}

void QtViewHost::Alert(const char *message) {
  QMessageBox::information(
      NULL,
      QString::fromUtf8(impl_->view_->GetCaption().c_str()),
      QString::fromUtf8(message));
}

bool QtViewHost::Confirm(const char *message) {
  int ret = QMessageBox::question(
      NULL,
      QString::fromUtf8(impl_->view_->GetCaption().c_str()),
      QString::fromUtf8(message),
      QMessageBox::Yes| QMessageBox::No,
      QMessageBox::Yes);
  return ret == QMessageBox::Yes;
}

std::string QtViewHost::Prompt(const char *message,
                                const char *default_value) {
  QString s= QInputDialog::getText(
      NULL,
      QString::fromUtf8(impl_->view_->GetCaption().c_str()),
      QString::fromUtf8(message),
      QLineEdit::Normal);
  return s.toStdString();
}

int QtViewHost::GetDebugMode() const {
  return impl_->debug_mode_;
}

QtViewHostObject *QtViewHost::GetQObject() {
  return impl_->qt_obj_;
}

} // namespace qt
} // namespace ggadget

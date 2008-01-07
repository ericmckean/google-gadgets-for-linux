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

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <vector>
#include <cmath>
#include <gtk/gtk.h>
#include <ggadget/logger.h>
#include <ggadget/main_loop_interface.h>
#include <ggadget/scriptable_array.h>
#include <ggadget/string_utils.h>
#include <ggadget/view.h>

#include "browser_element.h"
#include "browser_child.h"

namespace ggadget {
namespace gtkmoz {

class BrowserElement::Impl {
 public:
  Impl(BrowserElement *owner)
      : owner_(owner),
        main_loop_(owner->GetView()->GetMainLoop()),
        content_type_("text/html"),
        container_(NULL),
        container_x_(0), container_y_(0),
        socket_(NULL),
        controller_(BrowserController::get(owner->GetView()->GetMainLoop())),
        browser_id_(controller_->AddBrowserElement(this)),
        x_(0), y_(0), width_(0), height_(0) {
  }

  ~Impl() {
    if (GTK_IS_WIDGET(socket_))
      gtk_widget_destroy(socket_);
    controller_->SendCommand(kCloseBrowserCommand, browser_id_, NULL);
    controller_->RemoveBrowserElement(browser_id_);
  }

  void CreateSocket() {
    if (socket_)
      return;

    socket_ = gtk_socket_new();
    g_signal_connect(socket_, "realize", G_CALLBACK(OnSocketRealize), this);

    owner_->GetView()->GetNativeWidgetInfo(
        reinterpret_cast<void **>(&container_), &container_x_, &container_y_);
    if (!GTK_IS_FIXED(container_)) {
      LOG("BrowserElement needs a GTK_FIXED parent. Actual type: %s",
          G_OBJECT_TYPE_NAME(container_));
      gtk_widget_destroy(socket_);
      socket_ = NULL;
      return;
    }

    x_ = container_x_ + static_cast<gint>(round(owner_->GetPixelX()));
    y_ = container_y_ + static_cast<gint>(round(owner_->GetPixelY()));
    width_ = static_cast<gint>(ceil(owner_->GetPixelWidth()));
    height_ = static_cast<gint>(ceil(owner_->GetPixelHeight()));
    gtk_fixed_put(GTK_FIXED(container_), socket_, x_, y_);
    gtk_widget_set_size_request(socket_, width_, height_);
    gtk_widget_show(socket_);
  }

  static void OnSocketRealize(GtkWidget *widget, gpointer user_data) {
    Impl *impl = static_cast<Impl *>(user_data);
    std::string browser_id_str = StringPrintf("%zd", impl->browser_id_);
    // Convert GdkNativeWindow to intmax_t to ensure the printf format
    // to match the data type and not to loose accuracy.
    std::string socket_id_str = StringPrintf("0x%jx",
        static_cast<intmax_t>(gtk_socket_get_id(GTK_SOCKET(impl->socket_))));
    impl->controller_->SendCommand(kNewBrowserCommand, impl->browser_id_,
                                   socket_id_str.c_str(), NULL);
    impl->SetChildContent();
  }

  void SetChildContent() {
    controller_->SendCommand(kSetContentCommand, browser_id_,
                             content_type_.c_str(), content_.c_str(), NULL);
  }

  void Layout() {
    if (GTK_IS_FIXED(container_) && GTK_IS_SOCKET(socket_)) {
      (DLOG("Layout: %lf %lf %lf %lf", owner_->GetPixelX(), owner_->GetPixelY(),
            owner_->GetPixelWidth(), owner_->GetPixelHeight()));
      gint x = container_x_ + static_cast<gint>(round(owner_->GetPixelX()));
      gint y = container_y_ + static_cast<gint>(round(owner_->GetPixelY()));
      gint width = static_cast<gint>(ceil(owner_->GetPixelWidth()));
      gint height = static_cast<gint>(ceil(owner_->GetPixelHeight()));
  
      if (x != x_ || y != y_) {
        x_ = x;
        y_ = y;
        gtk_fixed_move(GTK_FIXED(container_), socket_, x, y);
      }
      if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        gtk_widget_set_size_request(socket_, width, height);
      }
    }
  }

  void ProcessUpMessage(const std::vector<const char *> &params) {
    std::string result;
    const char *type = params[0];
    if (strcmp(type, kGetPropertyFeedback) == 0) {
      if (params.size() != 3) {
        LOG("%s feedback needs 3 parameters, but %zd is given",
            kGetPropertyFeedback, params.size());
      } else {
        result = get_property_signal_(JSONString(params[2])).value;
      }
    } else if (strcmp(type, kSetPropertyFeedback) == 0) {
      if (params.size() != 4) {
        LOG("%s feedback needs 4 parameters, but %zd is given",
            kSetPropertyFeedback, params.size());
      } else {
        set_property_signal_(JSONString(params[2]), JSONString(params[3]));
      }
    } else if (strcmp(type, kCallbackFeedback) == 0) {
      if (params.size() < 3) {
        LOG("%s feedback needs at least 3 parameters, but %zd is given",
            kCallbackFeedback, params.size());
      } else {
        size_t callback_params_count = params.size() - 3;
        Variant *callback_params = new Variant[callback_params_count];
        for (size_t i = 0; i < callback_params_count; i++)
          callback_params[i] = Variant(JSONString(params[i + 3]));
        JSONString result_json = callback_signal_(
            JSONString(params[2]),
            ScriptableArray::Create(callback_params, callback_params_count));
        result = result_json.value;
      }
    } else if (strcmp(type, kOpenURLFeedback) == 0) {
      if (params.size() != 3) {
        LOG("%s feedback needs 3 parameters, but %zd is given",
            kOpenURLFeedback, params.size());
      }
      open_url_signal_(params[2]);
    } else {
      LOG("Unknown feedback: %s", type);
    }
    DLOG("ProcessUpMessage: %s(%s,%s,%s,%s) result: %s", type,
         params.size() > 1 ? params[1] : "",
         params.size() > 2 ? params[2] : "",
         params.size() > 3 ? params[3] : "",
         params.size() > 4 ? params[4] : "",
         result.c_str());
    result += '\n';
    controller_->Write(controller_->ret_fd_, result.c_str(), result.size());
  }

  void SetContent(const JSONString &content) {
    content_ = content.value;
    if (!GTK_IS_SOCKET(socket_)) {
      // After the child exited, the socket_ will become an invalid GtkSocket.
      CreateSocket();
    } else {
      SetChildContent();
    }
  }

  class BrowserController {
   public:
    BrowserController(MainLoopInterface *main_loop)
        : main_loop_(main_loop),
          child_pid_(0),
          down_fd_(0), up_fd_(0), ret_fd_(0),
          up_fd_watch_(0),
          ping_timer_watch_(main_loop->AddTimeoutWatch(
              kPingInterval * 3 / 2,
              new WatchCallbackSlot(
                  NewSlot(this, &BrowserController::PingTimerCallback)))),
          ping_flag_(false),
          removing_watch_(false) {
      StartChild();
    }

    ~BrowserController() {
      StopChild(false);
      instance_ = NULL;
      // This object may live longer than the main loop, so don't remove timer
      // watch here.
    }

    bool PingTimerCallback(int watch) {
      if (!ping_flag_)
        RestartChild();
      ping_flag_ = false;
      return true;
    }

    static BrowserController *get(MainLoopInterface *main_loop) {
      ASSERT(!instance_ || instance_->main_loop_ == main_loop);
      if (!instance_)
        instance_ = new BrowserController(main_loop);
      return instance_;
    }

    void StartChild() {
      removing_watch_ = false;

      int down_pipe_fds[2], up_pipe_fds[2], ret_pipe_fds[2];
      if (pipe(down_pipe_fds) == -1) {
        LOG("Failed to create downwards pipe to browser child");
        return;
      }
      if (pipe(up_pipe_fds) == -1) {
        LOG("Failed to create upwards pipe to browser child");
        close(down_pipe_fds[0]);
        close(down_pipe_fds[1]);
        return;
      }
      if (pipe(ret_pipe_fds) == -1) {
        LOG("Failed to create return value pipe to browser child");
        close(down_pipe_fds[0]);
        close(down_pipe_fds[1]);
        close(up_pipe_fds[0]);
        close(up_pipe_fds[1]);
        return;
      }

      child_pid_ = fork();
      if (child_pid_ == -1) {
        LOG("Failed to fork browser child");
        close(down_pipe_fds[0]);
        close(down_pipe_fds[1]);
        close(up_pipe_fds[0]);
        close(up_pipe_fds[1]);
        close(ret_pipe_fds[0]);
        close(ret_pipe_fds[1]);
        return;
      }

      if (child_pid_ == 0) {
        // This is the child process.
        close(down_pipe_fds[1]);
        close(up_pipe_fds[0]);
        close(ret_pipe_fds[1]);
        std::string down_fd_str = StringPrintf("%d", down_pipe_fds[0]);
        std::string up_fd_str = StringPrintf("%d", up_pipe_fds[1]); 
        std::string ret_fd_str = StringPrintf("%d", ret_pipe_fds[0]); 
        // TODO: Deal with the situtation that the main program is not run from
        // the directory it is in.
        execl("browser_child", "browser_child",
              down_fd_str.c_str(), up_fd_str.c_str(), ret_fd_str.c_str(), NULL);
        LOG("Failed to execute browser child");
        _exit(-1);
      } else {
        close(down_pipe_fds[0]);
        close(up_pipe_fds[1]);
        close(ret_pipe_fds[0]);
        down_fd_ = down_pipe_fds[1];
        up_fd_ = up_pipe_fds[0];
        ret_fd_ = ret_pipe_fds[1];
        int up_fd_flags = fcntl(up_fd_, F_GETFL);
        up_fd_flags |= O_NONBLOCK;
        fcntl(up_fd_, F_SETFL, up_fd_flags);
        up_fd_watch_ = main_loop_->AddIOReadWatch(up_fd_,
                                                  new UpFdWatchCallback(this));
      }
    }

    void StopChild(bool on_error) {
      if (!removing_watch_) {
        removing_watch_ = true;
        main_loop_->RemoveWatch(up_fd_watch_);
        removing_watch_ = false;
      }
      up_fd_watch_ = 0;
      if (child_pid_) {
        // Don't send QUIT command on error to prevent error loops.
        if (!on_error) {
          std::string quit_command(kQuitCommand);
          quit_command += kEndOfMessageFull;
          Write(down_fd_, quit_command.c_str(), quit_command.size());
        }
        close(down_fd_);
        down_fd_ = 0;
        close(up_fd_);
        up_fd_ = 0;
        close(ret_fd_);
        ret_fd_ = 0;
        child_pid_ = 0;
      }
      browser_elements_.clear();
    }

    void RestartChild() {
      StopChild(true);
      StartChild();
    }

    size_t AddBrowserElement(Impl *impl) {
      // Find an empty slot.
      BrowserElements::iterator it = std::find(browser_elements_.begin(),
                                               browser_elements_.end(),
                                               static_cast<Impl *>(NULL));
      size_t result = it - browser_elements_.begin();
      if (it == browser_elements_.end())
        browser_elements_.push_back(impl);
      else
        *it = impl;
      return result;
    }

    void RemoveBrowserElement(size_t id) {
      browser_elements_[id] = NULL;
    }

    class UpFdWatchCallback : public WatchCallbackInterface {
     public:
      UpFdWatchCallback(BrowserController *controller)
          : controller_(controller) {
      }
      virtual bool Call(MainLoopInterface *main_loop, int watch_id) {
        controller_->OnUpReady();
        return true;
      }
      virtual void OnRemove(MainLoopInterface *main_loop, int watch_id) {
        if (!controller_->removing_watch_) {
          controller_->removing_watch_ = true;
          // Removed by the mainloop when mainloop itself is to be destroyed.
          delete controller_;
        }
        delete this;
      }
     private:
      BrowserController *controller_;
    };

    void OnUpReady() {
      char buffer[4096];
      ssize_t read_bytes;
      while ((read_bytes = read(up_fd_, buffer, sizeof(buffer))) > 0) {
        up_buffer_.append(buffer, read_bytes);
        if (read_bytes < static_cast<ssize_t>(sizeof(buffer)))
          break;
      }
      if (read_bytes < 0)
        RestartChild();
      ProcessUpMessages();
    }

    void ProcessUpMessages() {
      size_t curr_pos = 0;
      size_t eom_pos;
      while ((eom_pos = up_buffer_.find(kEndOfMessageFull, curr_pos)) !=
              up_buffer_.npos) {
        std::vector<const char *> params;
        while (curr_pos < eom_pos) {
          size_t end_of_line_pos = up_buffer_.find('\n', curr_pos);
          ASSERT(end_of_line_pos != up_buffer_.npos);
          up_buffer_[end_of_line_pos] = '\0';
          params.push_back(up_buffer_.c_str() + curr_pos);
          curr_pos = end_of_line_pos + 1;
        }
        ASSERT(curr_pos = eom_pos + 1);
        curr_pos += sizeof(kEndOfMessageFull) - 2;

        if (params.size() == 1 && strcmp(params[0], kPingFeedback) == 0) {
          Write(ret_fd_, kPingAckFull, sizeof(kPingAckFull) - 1);
          ping_flag_ = true;
        } else if (params.size() < 2) {
          LOG("No enough feedback parameters");
        } else {
          size_t id = static_cast<size_t>(strtol(params[1], NULL, 0));
          if (id < browser_elements_.size() && browser_elements_[id] != NULL) {
            browser_elements_[id]->ProcessUpMessage(params);
          } else {
            LOG("Invalid browser id: %s", params[1]);
          }
        }
      }
      up_buffer_.erase(0, curr_pos);
    }

    void SendCommand(const char *type, size_t browser_id, ...) {
      if (down_fd_ > 0) {
        std::string buffer(type);
        buffer += '\n';
        buffer += StringPrintf("%zd", browser_id);

        va_list ap;
        va_start(ap, browser_id);
        const char *param;
        while ((param = va_arg(ap, const char *)) != NULL) {
          buffer += '\n';
          buffer += param;
        }
        buffer += kEndOfMessageFull;
        Write(down_fd_, buffer.c_str(), buffer.size());
      }
    }

    static void OnSigPipe(int sig) {
      instance_->RestartChild();
    }

    void Write(int fd, const char *data, size_t size) {
      sighandler_t old_handler = signal(SIGPIPE, OnSigPipe);
      if (write(fd, data, size) < 0)
        RestartChild();
      signal(SIGPIPE, old_handler);
    }

    static BrowserController *instance_;
    MainLoopInterface *main_loop_;
    int child_pid_;
    int down_fd_, up_fd_, ret_fd_;
    int up_fd_watch_;
    int ping_timer_watch_;
    bool ping_flag_;
    std::string up_buffer_;
    typedef std::vector<Impl *> BrowserElements;
    BrowserElements browser_elements_;
    bool removing_watch_;
  };

  BrowserElement *owner_;
  MainLoopInterface *main_loop_;
  std::string content_type_;
  std::string content_;
  GtkWidget *container_;
  int container_x_, container_y_;
  GtkWidget *socket_;
  BrowserController *controller_;
  size_t browser_id_;
  gint x_, y_, width_, height_;
  Signal1<JSONString, JSONString> get_property_signal_;
  Signal2<void, JSONString, JSONString> set_property_signal_;
  Signal2<JSONString, JSONString, ScriptableArray *> callback_signal_;
  Signal1<void, const std::string &> open_url_signal_;
};

BrowserElement::Impl::BrowserController *
    BrowserElement::Impl::BrowserController::instance_ = NULL;

BrowserElement::BrowserElement(BasicElement *parent, View *view,
                               const char *name)
    : BasicElement(parent, view, "browser", name, true),
      impl_(new Impl(this)) {
  RegisterProperty("contentType",
                   NewSlot(this, &BrowserElement::GetContentType),
                   NewSlot(this, &BrowserElement::SetContentType));
  RegisterProperty("innerText", NULL,
                   NewSlot(this, &BrowserElement::SetContent));
  RegisterSignal("onGetProperty", &impl_->get_property_signal_);
  RegisterSignal("onSetProperty", &impl_->set_property_signal_);
  RegisterSignal("onCallback", &impl_->callback_signal_);
  RegisterSignal("onOpenURL", &impl_->open_url_signal_);
}

BrowserElement::~BrowserElement() {
  delete impl_;
  impl_ = NULL;
}

std::string BrowserElement::GetContentType() const {
  return impl_->content_type_;
}

void BrowserElement::SetContentType(const char *content_type) {
  impl_->content_type_ = content_type && *content_type ? content_type :
                         "text/html";
}

void BrowserElement::SetContent(const JSONString &content) {
  impl_->SetContent(content);
}

void BrowserElement::Layout() {
  BasicElement::Layout();
  impl_->Layout();
}

void BrowserElement::DoDraw(CanvasInterface *canvas) {
}

BasicElement *BrowserElement::CreateInstance(BasicElement *parent, View *view,
                                             const char *name) {
  return new BrowserElement(parent, view, name);
}

} // namespace gtkmoz
} // namespace ggadget

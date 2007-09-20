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

#ifndef GGADGET_VIEW_H__
#define GGADGET_VIEW_H__

#include "common.h"
#include "scriptable_helper.h"
#include "view_interface.h"

namespace ggadget {

namespace internal {
  class ViewImpl;
}

class ElementFactoryInterface;

/**
 * Main View implementation.
 */
class View : public ViewInterface {
 public:
  DEFINE_CLASS_ID(0xc4ee4a622fbc4b7a, ViewInterface)

  View(ElementFactoryInterface *element_factory);
  virtual ~View();
  
  virtual bool AttachHost(HostInterface *host);

  virtual void OnMouseEvent(MouseEvent *event);
  virtual void OnKeyEvent(KeyboardEvent *event);
  virtual void OnOtherEvent(Event *event);
  virtual void OnTimerEvent(TimerEvent *event);

  virtual void OnElementAdd(ElementInterface *element);
  virtual void OnElementRemove(ElementInterface *element);
  virtual void FireEvent(Event *event, const EventSignal &event_signal);
  virtual Event *GetEvent();
  virtual const Event *GetEvent() const;

  virtual bool SetWidth(int width);
  virtual bool SetHeight(int height);
  virtual bool SetSize(int width, int height);

  virtual int GetWidth() const;
  virtual int GetHeight() const;

  virtual const CanvasInterface *Draw(bool *changed);   

  virtual void SetResizable(ResizableMode resizable);
  virtual ResizableMode GetResizable() const;
  virtual void SetCaption(const char *caption);
  virtual const char *GetCaption() const;
  virtual void SetShowCaptionAlways(bool show_always);
  virtual bool GetShowCaptionAlways() const;

  virtual const Elements *GetChildren() const;
  virtual Elements *GetChildren();
  virtual ElementInterface *GetElementByName(const char *name);
  virtual const ElementInterface *GetElementByName(const char *name) const;

  virtual int BeginAnimation(Slot1<void, int> *slot,
                             int start_value,
                             int end_value,
                             unsigned int duration);
  virtual void CancelAnimation(int token);
  virtual int SetTimeout(Slot0<void> *slot, unsigned int duration);
  virtual void ClearTimeout(int token);
  virtual int SetInterval(Slot0<void> *slot, unsigned int duration);
  virtual void ClearInterval(int token);

  DEFAULT_OWNERSHIP_POLICY
  DELEGATE_SCRIPTABLE_INTERFACE(scriptable_helper_)
  virtual bool IsStrict() const { return true; }

 protected:
  DELEGATE_SCRIPTABLE_REGISTER(scriptable_helper_)

 private:
  internal::ViewImpl *impl_;
  ScriptableHelper scriptable_helper_;
  DISALLOW_EVIL_CONSTRUCTORS(View);
};

} // namespace ggadget

#endif // GGADGET_VIEW_H__
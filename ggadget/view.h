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
#include "view_interface.h"

namespace ggadget {

namespace internal {
  class ViewImpl;
}

/**
 * Main View implementation.
 */
class View : public ViewInterface {
 public:
  DEFINE_CLASS_ID(0xc4ee4a622fbc4b7a, ViewInterface)

  /** Creates a new view. */
  View(int width, int height);
  virtual ~View();
  
  virtual bool AttachHost(HostInterface *host);
  
  virtual void OnMouseDown(MouseEvent *event);
  virtual void OnMouseUp(MouseEvent *event);
  virtual void OnClick(MouseEvent *event);
  virtual void OnDblClick(MouseEvent *event);
  virtual void OnMouseMove(MouseEvent *event);
  virtual void OnMouseOut(MouseEvent *event);
  virtual void OnMouseOver(MouseEvent *event);
  virtual void OnMouseWheel(MouseEvent *event);
  
  virtual void OnKeyDown(KeyboardEvent *event);
  virtual void OnKeyRelease(KeyboardEvent *event);  
  virtual void OnKeyPress(KeyboardEvent *event);
  
  virtual void OnFocusOut(Event *event);
  virtual void OnFocusIn(Event *event);

  virtual void OnElementAdded(ElementInterface *element);
  virtual void OnElementRemoved(ElementInterface *element);
  virtual void FireEvent(Event *event, const EventSignal &event_signal);

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

  virtual ElementInterface *AppendElement(const char *tag_name,
                                          const char *name);
  virtual ElementInterface *InsertElement(const char *tag_name,
                                          const ElementInterface *before,
                                          const char *name);
  virtual bool RemoveElement(ElementInterface *child);

  DEFAULT_OWNERSHIP_POLICY
  SCRIPTABLE_INTERFACE_DECL
  virtual bool IsStrict() const;

 private: 
  internal::ViewImpl *impl_;

  DISALLOW_EVIL_CONSTRUCTORS(View);
};

} // namespace ggadget

#endif // GGADGET_VIEW_H__
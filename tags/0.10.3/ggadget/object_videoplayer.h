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

#ifndef GGADGET_OBJECT_VIDEOPLAYER_H__
#define GGADGET_OBJECT_VIDEOPLAYER_H__

#include <ggadget/view.h>
#include <ggadget/signals.h>
#include <ggadget/basic_element.h>

namespace ggadget {

/**
 * This class is a wrapper of video element, but provides more functionalities
 * than it, such as playlist, because it behaves like the wmplayer object (an
 * ActiveX object under Windows).
 *
 * The video player object is designed to be used only with (hosted by) object
 * element, so itself won't export properties that a basic element should
 * provide to outside code, although it indeed inherits from the basic element.
 * Outside code should, instead, operate with these basic properties on object
 * element directly.
 */
class ObjectVideoPlayer : public BasicElement {
 public:
  DEFINE_CLASS_ID(0x8D5F2E792816428F, BasicElement);
  ObjectVideoPlayer(BasicElement *parent, View *view,
                    const char *tag_name, const char *name, bool children);
  virtual ~ObjectVideoPlayer();

  static BasicElement *CreateInstance(BasicElement *parent, View *view,
                                      const char *name);

  /**
   * We have video element as child element, but cannot expose it to the
   * outside code. So we must override Layout to delegate layout operation
   * to the video element.
   */
  void Layout();

 protected:
  /**
   * Register properties, methods, and signals. The real mediaplayer element
   * doesn't need to do any registation, and should never call this function.
   */
  virtual void DoRegister();

  /**
   * Draw a video frame on the canvas @canvas.
   * The real mediaplayer element should call PutImage to pass in the
   * metadata of an image frame that is ready to be shown. PutImage will do
   * a queue draw, and finally this function will be scheduled to actually
   * show the frame.
   */
  virtual void DoDraw(CanvasInterface *canvas);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ObjectVideoPlayer);

  class Impl;
  Impl *impl_;
};

} // namespace ggadget

#endif // GGADGET_OBJECT_VIDEOPLAYER_H__

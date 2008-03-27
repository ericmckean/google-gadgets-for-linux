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

#ifndef GGADGET_GTK_PIXBUF_IMAGE_H__
#define GGADGET_GTK_PIXBUF_IMAGE_H__

#include <ggadget/gtk/cairo_image_base.h>

namespace ggadget {
namespace gtk {

/**
 * This class realizes the ImageInterface using the gdk-pixbuf library.
 */
class PixbufImage : public CairoImageBase {
 public:
  PixbufImage(const CairoGraphics *graphics, const std::string &tag,
              const std::string &data, bool is_mask);
  virtual ~PixbufImage();

  virtual bool IsValid() const;

 public:
  virtual CanvasInterface *GetCanvas() const;
  virtual double GetWidth() const;
  virtual double GetHeight() const;
  virtual bool IsFullyOpaque() const;

 private:
  class Impl;
  Impl *impl_;

  DISALLOW_EVIL_CONSTRUCTORS(PixbufImage);
};

} // namespace gtk
} // namespace ggadget

#endif // GGADGET_GTK_PIXBUF_IMAGE_H__

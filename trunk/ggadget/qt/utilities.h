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

#ifndef GGADGET_QT_UTILITIES_H__
#define GGADGET_QT_UTILITIES_H__

#include <string>
#include <QCursor>

namespace ggadget {

class Gadget;
namespace qt {

/**
 * Shows an about dialog for a specified gadget.
 */
void ShowGadgetAboutDialog(Gadget *gadget);

/** Open the given URL in the user's default web browser. */
bool OpenURL(const char *url);

Qt::CursorShape GetQtCursorShape(int type);

} // namespace qt
} // namespace ggadget

#endif // GGADGET_QT_UTILITIES_H__

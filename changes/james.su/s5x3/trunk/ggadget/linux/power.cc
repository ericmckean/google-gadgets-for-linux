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

#include "power.h"

namespace ggadget {
namespace framework {
namespace linux_os {

bool Power::IsCharging() const {
  // TODO:
  return true;
}

bool Power::IsPluggedIn() const {
  // TODO:
  return true;
}

int Power::GetPercentRemaining() const {
  // TODO:
  return 79;
}

int Power::GetTimeRemaining() const {
  // TODO:
  return 5463;
}

int Power::GetTimeTotal() const {
  // TODO:
  return 9002;
}

} // namespace linux_os
} // namespace framework
} // namespace ggadget
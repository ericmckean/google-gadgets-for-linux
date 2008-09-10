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

#include "edit_element_base.h"
#include "event.h"
#include "logger.h"
#include "scriptable_event.h"
#include "scrolling_element.h"
#include "slot.h"
#include "string_utils.h"
#include "view.h"

namespace ggadget {

static const char *kAlignNames[] = {
  "left", "center", "right", "justify"
};

class EditElementBase::Impl {
 public:
  Impl(EditElementBase *owner) : owner_(owner) { }

  void FireOnChangeEvent() {
    SimpleEvent event(Event::EVENT_CHANGE);
    ScriptableEvent s_event(&event, owner_, NULL);
    owner_->GetView()->FireEvent(&s_event, onchange_event_);
  }

  JSONString GetIdealBoundingRect() {
    int w, h;
    owner_->GetIdealBoundingRect(&w, &h);
    return JSONString(StringPrintf("{\"width\":%d,\"height\":%d}", w, h));
  }

  EditElementBase *owner_;
  EventSignal onchange_event_;
};

EditElementBase::EditElementBase(BasicElement *parent, View *view, const char *name)
    : ScrollingElement(parent, view, "edit", name, false),
      impl_(new Impl(this)) {
  SetEnabled(true);
  SetAutoscroll(true);
}

void EditElementBase::DoClassRegister() {
  ScrollingElement::DoClassRegister();
  RegisterProperty("background",
                   NewSlot(&EditElementBase::GetBackground),
                   NewSlot(&EditElementBase::SetBackground));
  RegisterProperty("bold",
                   NewSlot(&EditElementBase::IsBold),
                   NewSlot(&EditElementBase::SetBold));
  RegisterProperty("color",
                   NewSlot(&EditElementBase::GetColor),
                   NewSlot(&EditElementBase::SetColor));
  RegisterProperty("font",
                   NewSlot(&EditElementBase::GetFont),
                   NewSlot(&EditElementBase::SetFont));
  RegisterProperty("italic",
                   NewSlot(&EditElementBase::IsItalic),
                   NewSlot(&EditElementBase::SetItalic));
  RegisterProperty("multiline",
                   NewSlot(&EditElementBase::IsMultiline),
                   NewSlot(&EditElementBase::SetMultiline));
  RegisterProperty("passwordChar",
                   NewSlot(&EditElementBase::GetPasswordChar),
                   NewSlot(&EditElementBase::SetPasswordChar));
  RegisterProperty("size",
                   NewSlot(&EditElementBase::GetSize),
                   NewSlot(&EditElementBase::SetSize));
  RegisterProperty("strikeout",
                   NewSlot(&EditElementBase::IsStrikeout),
                   NewSlot(&EditElementBase::SetStrikeout));
  RegisterProperty("underline",
                   NewSlot(&EditElementBase::IsUnderline),
                   NewSlot(&EditElementBase::SetUnderline));
  RegisterProperty("value",
                   NewSlot(&EditElementBase::GetValue),
                   NewSlot(&EditElementBase::SetValue));
  RegisterProperty("wordWrap",
                   NewSlot(&EditElementBase::IsWordWrap),
                   NewSlot(&EditElementBase::SetWordWrap));
  RegisterProperty("readonly",
                   NewSlot(&EditElementBase::IsReadOnly),
                   NewSlot(&EditElementBase::SetReadOnly));
  RegisterProperty("idealBoundingRect",
                   NewSlot(&Impl::GetIdealBoundingRect,
                           &EditElementBase::impl_),
                   NULL);

  RegisterMethod("select", NewSlot(&EditElementBase::Select));
  RegisterMethod("selectAll", NewSlot(&EditElementBase::SelectAll));

  RegisterClassSignal(kOnChangeEvent, &Impl::onchange_event_,
                      &EditElementBase::impl_);

  RegisterStringEnumProperty("align",
                             NewSlot(&EditElementBase::GetAlign),
                             NewSlot(&EditElementBase::SetAlign),
                             kAlignNames, arraysize(kAlignNames));
}

EditElementBase::~EditElementBase() {
  delete impl_;
}

Connection *EditElementBase::ConnectOnChangeEvent(Slot0<void> *slot) {
  return impl_->onchange_event_.Connect(slot);
}

void EditElementBase::FireOnChangeEvent() const {
  impl_->FireOnChangeEvent();
}

} // namespace ggadget
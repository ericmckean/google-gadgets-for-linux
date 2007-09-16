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

#include <vector>
#include <algorithm>
#include "elements.h"
#include "elements_impl.h"
#include "common.h"
#include "element_interface.h"
#include "element_factory_interface.h"

namespace ggadget {

namespace internal {

ElementsImpl::ElementsImpl(ElementFactoryInterface *factory,
                           ElementInterface *owner,
                           ViewInterface *view)
    : factory_(factory), owner_(owner), view_(view) {
  ASSERT(factory);
}

ElementsImpl::~ElementsImpl() {
  RemoveAllElements();
}

int ElementsImpl::GetCount() {
  return children_.size();
}

ElementInterface *ElementsImpl::AppendElement(const char *tag_name,
                                              const char *name) {
  ElementInterface *e = factory_->CreateElement(tag_name,
                                                owner_,
                                                view_,
                                                name);
  if (e == NULL)
    return NULL;
  children_.push_back(e);
  return e;
}

ElementInterface *ElementsImpl::InsertElement(
    const char *tag_name, const ElementInterface *before, const char *name) {
  ElementInterface *e = factory_->CreateElement(tag_name,
                                                owner_,
                                                view_,
                                                name);
  if (e == NULL)
    return NULL;
  std::vector<ElementInterface *>::iterator ite = std::find(
      children_.begin(), children_.end(), before);
  children_.insert(ite, e);
  return e;
}

bool ElementsImpl::RemoveElement(ElementInterface *element) {
  std::vector<ElementInterface *>::iterator ite = std::find(
      children_.begin(), children_.end(), element);
  if (ite == children_.end())
    return false;
  (*ite)->Destroy();
  children_.erase(ite);
  return true;
}

void ElementsImpl::RemoveAllElements() {
  for (std::vector<ElementInterface *>::iterator ite =
       children_.begin(); ite != children_.end(); ++ite)
    (*ite)->Destroy();
  std::vector<ElementInterface *> v;
  children_.swap(v);
}

ElementInterface *ElementsImpl::GetItemByIndex(int index) {
  if (index >= 0 && index < static_cast<int>(children_.size()))
    return children_[index];
  return NULL;
}

ElementInterface *ElementsImpl::GetItemByName(const char *name) {
  return GetItemByIndex(GetIndexByName(name));
}

int ElementsImpl::GetIndexByName(const char *name) {
  if (name == NULL || strlen(name) == 0)
    return -1;
  for (std::vector<ElementInterface *>::const_iterator ite = children_.begin();
       ite != children_.end(); ++ite) {
    if (strcmp((*ite)->GetName(), name) == 0)
      return ite - children_.begin();
  }
  return -1;
}

} // namespace internal

Elements::Elements(ElementFactoryInterface *factory,
                   ElementInterface *owner,
                   ViewInterface *view)
    : impl_(new internal::ElementsImpl(factory, owner, view)) {
}

Elements::~Elements() {
  delete impl_;
}

int Elements::GetCount() const {
  ASSERT(impl_);
  return impl_->GetCount();
}

ElementInterface *Elements::GetItemByIndex(int child) {
  ASSERT(impl_);
  return impl_->GetItemByIndex(child);
}

ElementInterface *Elements::GetItemByName(const char *child) {
  ASSERT(impl_);
  return impl_->GetItemByName(child);
}

const ElementInterface *Elements::GetItemByIndex(int child) const {
  ASSERT(impl_);
  return impl_->GetItemByIndex(child);
}

const ElementInterface *Elements::GetItemByName(const char *child) const {
  ASSERT(impl_);
  return impl_->GetItemByName(child);
}

ElementInterface *Elements::AppendElement(const char *tag_name,
                                          const char *name) {
  ASSERT(impl_);
  return impl_->AppendElement(tag_name, name);
}

ElementInterface *Elements::InsertElement(const char *tag_name,
                                          const ElementInterface *before,
                                          const char *name) {
  ASSERT(impl_);
  return impl_->InsertElement(tag_name, before, name);
}

bool Elements::RemoveElement(ElementInterface *element) {
  ASSERT(impl_);
  return impl_->RemoveElement(element);
}

void Elements::RemoveAllElements() {
  ASSERT(impl_);
  impl_->RemoveAllElements();
}

DELEGATE_SCRIPTABLE_INTERFACE_IMPL(Elements, impl_->scriptable_helper_)

} // namespace ggadget
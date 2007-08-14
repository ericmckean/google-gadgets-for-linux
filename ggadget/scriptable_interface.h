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

#ifndef GGADGET_SCRIPTABLE_INTERFACE_H__
#define GGADGET_SCRIPTABLE_INTERFACE_H__

#include "common.h"
#include "variant.h"

namespace ggadget {

class Connection;
template <typename R> class Slot0;

/**
 * Object interface that can be called from script languages.
 * Only objects with dynamic properties or methods need to directly
 * implement this interface.  Other objects should use @c StaticScriptable.
 *
 * An implementation should include a <code>static const int CLASS_ID</code>
 * field which uniquely identifies the class.
 */
class ScriptableInterface {
 public:
  /**
   * This ID uniquely identifies the class.  Each implementation should define
   * this field as a unique integer.  You can simply use the first part of
   * the result of uuidgen.
   */
  static const int CLASS_ID = 0;

  /**
   * Attach this object to the script engine.
   * Normally if the object is always owned by the native side, the
   * implementation should do nothing in this method.
   *
   * If the ownership can be transfered or shared between the native side
   * and the script side, the implementation should do appropriate things,
   * such as reference counting, etc. to manage the ownership.
   */
  virtual void Attach() = 0;

  /**
   * Detach this object from the script engine.
   * @see Attach()
   */
  virtual void Detach() = 0;

  /**
   * Judge if this instance is of a given class.
   */
  virtual bool IsInstanceOf(int class_id) = 0;

  /**
   * Connect a callback @c Slot to the "ondelete" signal.
   * @param slot the callback @c Slot to be called when the @c Scriptable
   *     object is to be deleted.
   * @return the connected @c Connection or @c NULL if fails.
   */
  virtual Connection *ConnectToOnDeleteSignal(Slot0<void> *slot) = 0;

  /**
   * Get the info of a property by its name.
   *
   * Because methods are special properties, if @c name corresponds a method,
   * a prototype of type @c Variant::TYPE_SLOT will be returned, then the
   * caller can get the function details from @c slot_value this prototype.
   *
   * A signal property also expects a script function as the value, and thus
   * also has a prototype of type @c Variant::TYPE_SLOT.
   *
   * @param name the name of the property.
   * @param[out] id return the property's id which can be used in later
   *     @c GetProperty(), @c SetProperty() and @c InvokeMethod() calls.
   *     If the returned id is @c 0, the script engine will treat the property
   *     as a constant.  Otherwise, the value must be a @b negative number.
   * @param[out] prototype return a prototype of the property value, from
   *     which the script engine can get detailed information.
   * @param[out] is_method @c true if this property corresponds a method.
   *     It's useful to distinguish between methods and signal properties.
   * @return @c true if the property is supported and succeeds.
   */
  virtual bool GetPropertyInfoByName(const char *name,
                                     int *id, Variant *prototype,
                                     bool *is_method) = 0;

  /**
   * Get the info of a property by its id.
   * @param id if negative, it is a property id previously returned from
   *     @c GetPropertyInfoByName(); otherwise it is the array index of a
   *     property.
   * @param[out] prototype return a prototype of the property value, from
   *     which the script engine can get detailed information.
   * @param[out] is_method true if this property corresponds a method.
   * @return @c true if the property is supported and succeeds.
   */
  virtual bool GetPropertyInfoById(int id, Variant *prototype,
                                   bool *is_method) = 0;

  /**
   * Get the value of a property by its id.
   * @param id if negative, it is a property id previously returned from
   *     @c GetPropertyInfoByName(); otherwise it is the array index of a
   *     property.
   * @return the property value, or a @c Variant of type @c Variant::TYPE_VOID
   *     if this property is not supported,
   */
  virtual Variant GetProperty(int id) = 0;

  /**
   * Set the value of a property by its id.
   * @param id if negative, it is a property id previously returned from
   *     @c GetPropertyInfoByName(); otherwise it is the array index of a
   *     property.
   * @param value the property value. The type must be compatible with the
   *     prototype returned from @c GetPropertyInfoByName().
   * @return @c true if the property is supported and succeeds.
   */
  virtual bool SetProperty(int id, Variant value) = 0;

  DISALLOW_DIRECT_DELETION(ScriptableInterface);
};

} // namespace ggadget

#endif // GGADGET_SCRIPTABLE_INTERFACE_H__

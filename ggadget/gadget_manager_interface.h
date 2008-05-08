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

#include <string>

#ifndef GGADGET_GADGET_MANAGER_INTERFACE_H__
#define GGADGET_GADGET_MANAGER_INTERFACE_H__

namespace ggadget {

class HostInterface;

class Connection;
template <typename R, typename P1> class Slot1;

/**
 * Manages instances of gadgets.
 */
class GadgetManagerInterface {
 protected:
  virtual ~GadgetManagerInterface() { }

 public:

  /**
   * Returns impl-specific data. Because a @c GadgetManagerInterface impl can
   * only be got from @c GetGadgetManager(), some impl-specific caller can
   * use this method to check if the returned instance is the one expected.
   */
  virtual const char *GetImplTag() = 0;

  /**
   * Creates an new instance of a gadget specified by the file path. Used to
   * open a gadget located in local file system.
   * @param file location of a gadget file. The location can be a full path of
   *     a gadget file, or a location that can be recognized by the global
   *     file manager.
   * @return the gadget instance id (>=0) of the new instance, or -1 on error.
   */
  virtual int NewGadgetInstanceFromFile(const char *file) = 0;

  /**
   * Removes a gadget instance.
   * @param instance_id id of an active instance.
   * @return @c true if succeeded.
   */
  virtual bool RemoveGadgetInstance(int instance_id) = 0;

  /**
   * Returns the name to create the @c OptionsInterface instance for a gadget
   * instance.
   */
  virtual std::string GetGadgetInstanceOptionsName(int instance_id) = 0;

  /**
   * Enunerates all active gadget instances. The callback will receive an int
   * parameter which is the gadget instance id, and can return true if it
   * wants the enumeration to continue, or false to break the enumeration.
   */
  virtual bool EnumerateGadgetInstances(Slot1<bool, int> *callback) = 0;

  /**
   * Get the full path of the file for a gadget instance, either downloaded or
   * opened from local file system.
   * @param instance_id id of an active instance.
   * @return the full path of the file for a gadget instance.
   */
  virtual std::string GetGadgetInstancePath(int instance_id) = 0;

  /**
   * Shows the gadget browser dialog.
   * TODO: parameters and return values.
   */
  virtual void ShowGadgetBrowserDialog(HostInterface *host) = 0;

  /**
   * Checks if a gadget instance can be safely trusted.
   */
  virtual bool IsGadgetInstanceTrusted(int instance_id) = 0;

  /**
   * Gets information of a gadget instance.
   */ 
  virtual bool GetGadgetInstanceInfo(int instance_id, const char *locale,
                                     std::string *author,
                                     std::string *download_url,
                                     std::string *title,
                                     std::string *description) = 0;

 public:
  /**
   * Connects to signals when a gadget instance is added, to be removed or
   * should be updated. The int parameter of the callback is the gadget
   * instance id. The callback of OnNewGadgetInstance can return @c false to
   * cancel the action.
   */
  virtual Connection *ConnectOnNewGadgetInstance(
      Slot1<bool, int> *callback) = 0;
  virtual Connection *ConnectOnRemoveGadgetInstance(
      Slot1<void, int> *callback) = 0;
  virtual Connection *ConnectOnUpdateGadgetInstance(
      Slot1<void, int> *callback) = 0;
};

/**
 * Sets the global GadgetManagerInterface instance.  A GadgetManager extension
 * module can call this function in its @c Initialize() function.
 */
bool SetGadgetManager(GadgetManagerInterface *gadget_parser);

/**
 * Gets the GadgetManagerInterface instance.
 *
 * The returned instance is a singleton provided by a GadgetManager
 * extension module, which is loaded into the global ExtensionManager in
 * advance.
 */
GadgetManagerInterface *GetGadgetManager();

} // namespace ggadget

#endif // GGADGET_GADGET_MANAGER_INTERFACE_H__
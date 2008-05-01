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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>

#include "ggadget/dbus/dbus_proxy.h"
#include "ggadget/tests/native_main_loop.h"
#include "ggadget/logger.h"
#include "ggadget/slot.h"
#include "ggadget/tests/init_extensions.h"
#include "unittest/gtest.h"

using namespace ggadget;
using namespace ggadget::dbus;

namespace {

const char* kName        = "com.google.Gadget";
const char* kPath        = "/com/google/Gadget/Test";
const char* kInterface   = "com.google.Gadget.Test";
const char* kDisconnect  = "Disconnected";
const char* kSystemRule  = "type='signal',interface='"DBUS_INTERFACE_LOCAL "'";
const char* kSessionRule = "type='signal',interface='com.google.Gadget.Test'";

int feed = rand();

DBusHandlerResult FilterFunction(DBusConnection *connection,
                                 DBusMessage *message,
                                 void *user_data) {
  DLOG("Get message, type: %d, sender: %s, path: %s, interface: %s, member: %s",
       dbus_message_get_type(message), dbus_message_get_sender(message),
       dbus_message_get_path(message), dbus_message_get_interface(message),
       dbus_message_get_member(message));
  if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, kDisconnect)) {
    DLOG("server: got system disconnect signal, exit.");
    dbus_connection_close(connection);
    exit(0);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else {
    LOG("server: got other message.");
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void path_unregistered_func(DBusConnection *connection,
                            void *user_data) {
  DLOG("server: connection was finalized");
}

DBusHandlerResult handle_echo(DBusConnection *connection,
                              DBusMessage *message) {
  DLOG("server: sending reply to Echo method");
  DBusError error;
  dbus_error_init (&error);
  DBusMessage *reply = dbus_message_new_method_return(message);

  DBusMessageIter out_iter, in_iter;
  dbus_message_iter_init(message, &out_iter);
  dbus_message_iter_init_append(reply, &in_iter);

  int arg_type = dbus_message_iter_get_arg_type(&out_iter);
  switch (arg_type) {
    case DBUS_TYPE_BYTE:
      {
        unsigned char v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_BYTE, &v);
        break;
      }
    case DBUS_TYPE_BOOLEAN:
      {
        dbus_bool_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_BOOLEAN, &v);
        break;
      }
    case DBUS_TYPE_INT16:
      {
        dbus_int16_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_INT16, &v);
        break;
      }
    case DBUS_TYPE_UINT16:
      {
        dbus_uint16_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_UINT16, &v);
        break;
      }
    case DBUS_TYPE_INT32:
      {
        dbus_int32_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_INT32, &v);
        break;
      }
    case DBUS_TYPE_UINT32:
      {
        dbus_uint32_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_UINT32, &v);
        break;
      }
    case DBUS_TYPE_INT64:
      {
        dbus_int64_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_INT64, &v);
        break;
      }
    case DBUS_TYPE_UINT64:
      {
        dbus_uint64_t v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_UINT64, &v);
        break;
      }
    case DBUS_TYPE_DOUBLE:
      {
        double v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_DOUBLE, &v);
        break;
      }
    case DBUS_TYPE_STRING:
      {
        const char* v;
        dbus_message_iter_get_basic(&out_iter, &v);
        dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_STRING, &v);
        break;
      }
    default:
      DLOG("server: unsupported type met: %d", arg_type);
      ASSERT(false);
  }

  if (!dbus_connection_send(connection, reply, NULL))
    DLOG("server: send reply failed: No memory");
  dbus_message_unref(reply);
  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult path_message_func(DBusConnection *connection,
                                    DBusMessage *message,
                                    void *user_data) {
  DLOG("server: handle message.");
  if (dbus_message_is_method_call(message,
                                  kInterface,
                                  "Echo")) {
    return handle_echo(connection, message);
  } else if (dbus_message_is_method_call(message,
                                         kInterface,
                                         kDisconnect)) {
    DLOG("server: received disconnected call from peer.");
    dbus_connection_close(connection);
    exit(0);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_method_call(message,
                                         kInterface,
                                         "Signal")) {
    DLOG("server: received signal echo call from peer.");
    DBusMessage *signal = dbus_message_new_signal(kPath, kInterface, "signal1");
    dbus_connection_send(connection, signal, NULL);
    dbus_connection_flush(connection);
    dbus_message_unref(signal);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_method_call(message,
                                         kInterface,
                                         "Hello")) {
    DLOG("server: received Hello message.");
    DBusMessage *reply = dbus_message_new_method_return(message);
    dbus_int32_t rand_feed = *(reinterpret_cast<dbus_int32_t*>(user_data));
    DLOG("server: feed: %d", rand_feed);
    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 100000000;
    nanosleep(&tm, NULL);
    dbus_message_append_args(reply,
                             DBUS_TYPE_INT32, &rand_feed,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  DLOG("server: the message was not handled.");
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusObjectPathVTable echo_vtable = {
  path_unregistered_func,
  path_message_func,
  NULL,
};

void StartDBusServer(int feed) {
  DBusError error;
  dbus_error_init(&error);
  DBusConnection *bus;
  bus = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
  if (!bus) {
    LOG("server: Failed to connect to the D-BUS daemon: %s", error.message);
    dbus_error_free(&error);
    return;
  }
  DLOG("server: name of the connection: %s", dbus_bus_get_unique_name(bus));

  if (!dbus_connection_add_filter(bus, FilterFunction, NULL, NULL))
    LOG("server: add filter failed.");

  dbus_error_init(&error);
  dbus_bus_request_name(bus, kName, 0, &error);
  if (dbus_error_is_set(&error)) {
    DLOG("server: %s: %s", error.name, error.message);
  }

  dbus_bus_add_match(bus, kSystemRule, &error);
  if (dbus_error_is_set(&error)) {
    DLOG("server: %s: %s", error.name, error.message);
  }

  dbus_bus_add_match(bus, kSessionRule, &error);
  if (dbus_error_is_set(&error)) {
    DLOG("server: %s: %s", error.name, error.message);
  }

  int f = feed;  /** to avoid user pass a number in */
  if (!dbus_connection_register_object_path(bus, kPath, &echo_vtable,
                                            (void*)&f))
    DLOG("server: register failed.");

  while (dbus_connection_read_write_dispatch(bus, -1))
    ;

  return;
}

void KillServer() {
  DBusMessage *message = dbus_message_new_method_call(kName, kPath,
                                                      kInterface, kDisconnect);
  DBusError error;
  dbus_error_init(&error);
  DBusConnection *bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
  dbus_connection_send(bus, message, NULL);
  dbus_connection_flush(bus);
  dbus_message_unref(message);
  dbus_error_free(&error);
}

class IntValue {
 public:
  IntValue() : value_(0) {
  }
  ~IntValue() {}
  bool Callback(int id, const Variant &value) {
    DLOG("id: %d, value: %s", id, value.Print().c_str());
    ASSERT(value.type() == Variant::TYPE_INT64);
    int64_t v = VariantValue<int64_t>()(value);
    value_ = static_cast<int>(v);
    return true;
  }
  int value() const { return value_; }
 private:
  int value_;
};

class BoolValue {
 public:
  BoolValue() : value_(false) {}
  void Set(bool v) { value_ = v; }
  bool value() const { return value_; }
  bool Callback(int id, const Variant &value) {
    ASSERT(value.type() == Variant::TYPE_BOOL);
    value_ = VariantValue<bool>()(value);
    return true;
  }
 private:
  bool value_;
};

// if crash, we should kill testing server
void ExitHandler(int signo) {
  KillServer();
}

void RegisterSignalHander() {
  signal(SIGQUIT, ExitHandler);
  signal(SIGSEGV, ExitHandler);
  signal(SIGTERM, ExitHandler);
  signal(SIGSTOP, ExitHandler);
  signal(SIGABRT, ExitHandler);
}

}  // anonymous namespace

TEST(DBusProxy, SystemCall) {
  const char *kDBusName = "org.freedesktop.DBus";
  DBusProxyFactory *factory = new DBusProxyFactory(NULL);
  DBusProxy *proxy =
      factory->NewSystemProxy(kDBusName,
                              "/org/freedeskop/DBus",
                              kDBusName,
                              false);
  BoolValue obj;
  proxy->Call("NameHasOwner", true, -1,
              NewSlot(&obj, &BoolValue::Callback),
              MESSAGE_TYPE_STRING, kDBusName, MESSAGE_TYPE_INVALID);
  DLOG("result of NameHasOwner: %d", obj.value());
  EXPECT_TRUE(obj.value());
  delete proxy;
}

TEST(DBusProxy, SyncCall) {
  int id = fork();
  if (id == 0) {
    DLOG("server start");
    StartDBusServer(feed);
    StartDBusServer(feed * 2);
    exit(0);
  } else {
    RegisterSignalHander();
    sleep(1);  /** wait server start */
    DLOG("client start");
    DBusProxyFactory *factory = new DBusProxyFactory(NULL);
    DBusProxy *proxy =
        factory->NewSessionProxy(kName, kPath, kInterface, false);
    IntValue obj;
    EXPECT_TRUE(proxy->Call("Hello", true, -1,
                            NewSlot(&obj, &IntValue::Callback),
                            MESSAGE_TYPE_INVALID));
    DLOG("read feed: %d", obj.value());
    EXPECT_EQ(feed, obj.value());
    delete proxy;
    delete factory;
  }
}

class Timeout : public WatchCallbackInterface {
 public:
  virtual bool Call(MainLoopInterface *main_loop, int watch_id) {
    main_loop->Quit();
    return false;
  }
  virtual void OnRemove(MainLoopInterface *main_loop, int watch_id) {
    delete this;
  }
};

TEST(DBusProxy, AsyncCall) {
  NativeMainLoop mainloop;
  DBusProxyFactory factory(&mainloop);
  DBusProxy *proxy = factory.NewSessionProxy(kName, kPath, kInterface, false);
  mainloop.AddTimeoutWatch(200, new Timeout);
  IntValue obj;
  EXPECT_TRUE(proxy->Call("Hello", false, 50,
                          NewSlot(&obj, &IntValue::Callback),
                          MESSAGE_TYPE_INVALID));
  mainloop.Run();
  EXPECT_EQ(0, obj.value());
  mainloop.AddTimeoutWatch(1000, new Timeout);
  EXPECT_TRUE(proxy->Call("Hello", false, -1,
                          NewSlot(&obj, &IntValue::Callback),
                          MESSAGE_TYPE_INVALID));
  mainloop.Run();
  EXPECT_EQ(feed, obj.value());
  delete proxy;
}

class SignalSlot {
 public:
  SignalSlot() : value_(0) {}
  int value() const {return value_;}
  void Callback() { ++value_; }
 private:
  int value_;
};

TEST(DBusProxy, ConnectToSignal) {
  NativeMainLoop mainloop;
  DBusProxyFactory factory(&mainloop);
  DBusProxy *proxy = factory.NewSessionProxy(kName, kPath, kInterface, false);
  ASSERT(proxy);
  SignalSlot slot;
  proxy->ConnectToSignal("signal1", NewSlot(&slot, &SignalSlot::Callback));
  EXPECT_TRUE(proxy->Call("Signal", true, -1, NULL, MESSAGE_TYPE_INVALID));
  Timeout *timeout = new Timeout;
  mainloop.AddTimeoutWatch(1000, timeout);
  mainloop.Run();
  EXPECT_NE(0, slot.value());
  delete proxy;
}

TEST(DBusProxy, ConnectToSignalByName) {
  NativeMainLoop mainloop;
  DBusProxyFactory factory(&mainloop);
  DBusProxy *proxy = factory.NewSessionProxy(kName, kPath, kInterface, true);
  ASSERT(proxy);
  SignalSlot slot;
  Timeout *timeout = new Timeout;
  mainloop.AddTimeoutWatch(2000, timeout);
  proxy->ConnectToSignal("signal1", NewSlot(&slot, &SignalSlot::Callback));
  EXPECT_TRUE(proxy->Call("Signal", true, -1, NULL, MESSAGE_TYPE_INVALID));
  mainloop.Run();
  int old = slot.value();
  EXPECT_NE(0, old);
  KillServer();
  timeout = new Timeout;
  mainloop.AddTimeoutWatch(2000, timeout);
  proxy->ConnectToSignal("signal1", NewSlot(&slot, &SignalSlot::Callback));
  EXPECT_TRUE(proxy->Call("Signal", true, -1, NULL, MESSAGE_TYPE_INVALID));
  mainloop.Run();
  EXPECT_EQ(old, slot.value());
  delete proxy;
}

int main(int argc, char **argv) {
  testing::ParseGTestFlags(&argc, argv);

  static const char *kExtensions[] = {
    "libxml2_xml_parser/libxml2-xml-parser",
  };
  INIT_EXTENSIONS(argc, argv, kExtensions);

  int result = RUN_ALL_TESTS();
  KillServer();
  return result;
}
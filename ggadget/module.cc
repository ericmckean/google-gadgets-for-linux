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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <vector>
#include <cstdlib>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ltdl.h>
#include "module.h"
#include "common.h"
#include "gadget_consts.h"
#include "logger.h"
#include "system_utils.h"

namespace ggadget {

static const char *kModulePathEnv = "GGL_MODULE_PATH";
static const char *kModuleInitializeSymbol = "Initialize";
static const char *kModuleFinalizeSymbol = "Finalize";

class Module::Impl {
 public:
  Impl() : handle_(0), initialize_(0), finalize_(0) {
    if (lt_dlinit() != 0)
      LOG("Failed to initialize the module system: %s", lt_dlerror());
  }

  ~Impl() {
    Unload();
    if (lt_dlexit() != 0)
      LOG("Failed to finalize the module system: %s", lt_dlerror());
  }

  bool Load(const char *name) {
    ASSERT(name && *name);

    // If the module is already loaded and made resident, then return false.
    if (IsResident() || !name || !*name) return false;

    lt_dlhandle new_handle = NULL;
    std::string module_name;
    std::vector<std::string> paths;

    PrepareModuleName(name, &paths, &module_name);

    std::string module_path;
    for (std::vector<std::string>::iterator it = paths.begin();
         it != paths.end(); ++it) {
      module_path = BuildFilePath(it->c_str(), module_name.c_str(), NULL);
      new_handle = lt_dlopenext(module_path.c_str());
      if (new_handle) break;
#ifdef _DEBUG
      const char *err = lt_dlerror();
      if (err)
        DLOG("Failed to load module %s: %s", name, err);
#endif
    }

    if (!new_handle)
      return false;

    const lt_dlinfo *module_info = lt_dlgetinfo(new_handle);
    ASSERT(module_info);

    module_path = module_info->filename;

    // Use module name provided by ltdl if available.
    if (module_info->name)
      module_name = module_info->name;

    NormalizeNameString(&module_name);

    InitializeFunction new_initialize = reinterpret_cast<InitializeFunction>(
        GetModuleSymbol(new_handle, module_name.c_str(),
                        kModuleInitializeSymbol));
    FinalizeFunction new_finalize = reinterpret_cast<FinalizeFunction>(
        GetModuleSymbol(new_handle, module_name.c_str(),
                        kModuleFinalizeSymbol));

    // Failed to load the module, because of missing.
    if (!new_initialize) {
      lt_dlclose(new_handle);
      return false;
    }

    if (!handle_ || Unload()) {
      handle_ = new_handle;
      initialize_ = new_initialize;
      finalize_ = new_finalize;
      path_ = module_path;
      name_ = module_name;

      // Only call Initialize() when loading the module the first time.
      if (module_info->ref_count == 1) {
        if (!initialize_()) {
          Unload();
          return false;
        }
      }
      return true;
    }
    lt_dlclose(new_handle);
    return false;
  }

  bool Unload() {
    if (!handle_)
      return false;

    if (IsResident()) {
      LOG("Can't unload a resident module: %s", name_.c_str());
      return false;
    }

    const lt_dlinfo *info = lt_dlgetinfo(handle_);
    ASSERT(info);

    // Only call Finalize() when the module is actually being unloaded.
    if (info->ref_count == 1 && finalize_)
      finalize_();

    lt_dlclose(handle_);

    handle_ = NULL;
    initialize_ = NULL;
    finalize_ = NULL;
    path_ = std::string();
    name_ = std::string();

    return true;
  }

  bool IsValid() {
    return handle_ && initialize_;
  }

  bool MakeResident() {
    bool result = false;
    if (handle_) {
      result = (lt_dlmakeresident(handle_) == 0);
      if (!result)
        LOG("Failed to make the module %s resident: %s",
            name_.c_str(), lt_dlerror());
    }
    return result;
  }

  bool IsResident() {
    if (handle_)
      return lt_dlisresident(handle_) == 1;
    return false;
  }

  std::string GetPath() { return path_; }
  std::string GetName() { return name_; }

  void *GetSymbol(const char *symbol_name) {
    ASSERT(symbol_name && *symbol_name);
    if (handle_)
      return GetModuleSymbol(handle_, name_.c_str(), symbol_name);
    return NULL;
  }

 public:
  // Get all available module searching paths.
  // If dir is an absolute path, then just returns dir, otherwise get all
  // searching paths from GGL_MODULE_PATH env and built-in GGL_MODULE_DIR
  // macro, and append dir to each path.
  static size_t GetModulePaths(const char *dir, std::vector<std::string> *paths) {
    if (dir && *dir == kDirSeparator) {
      paths->push_back(dir);
      return paths->size();
    }

    const char *env = getenv(kModulePathEnv);

    if (env) {
      const char *p = env;
      while (*p) {
        while(*p != 0 && *p != kSearchPathSeparator) ++p;
        // Only care about the absolute paths
        if (p != env && *env == kDirSeparator) {
          std::string path(env, p);
          if (dir && *dir)
            path = BuildFilePath(path.c_str(), dir, NULL);
          // Remove duplicated paths.
          if (std::find(paths->begin(), paths->end(), path) == paths->end())
            paths->push_back(path);
        }
        if (*p) env = ++p;
      }
    }

    if (dir && *dir) {
      paths->push_back(BuildFilePath(GGL_MODULE_DIR, dir, NULL));
    } else {
      paths->push_back(GGL_MODULE_DIR);
    }

    return paths->size();
  }

  // Get all modules found in the specified subdir in each module searching
  // path.
  static size_t GetModuleList(const char *path,
                              std::vector<std::string> *mod_list) {
    std::vector<std::string> paths;

    GetModulePaths(path, &paths);
    std::string search_path = PathListToString(paths);

    lt_dlforeachfile(search_path.c_str(), GetModuleListCallback,
                     reinterpret_cast<lt_ptr>(mod_list));

    return mod_list->size();
  }

 private:
  static int GetModuleListCallback(const char *filename, lt_ptr data) {
    std::vector<std::string> *vec =
        reinterpret_cast<std::vector<std::string>*>(data);
    vec->push_back(filename);
    return 0;
  }

  static std::string PathListToString(const std::vector<std::string> &paths) {
    std::string result;
    for (std::vector<std::string>::const_iterator it = paths.begin();
         it != paths.end(); ++it) {
      result.append(*it);
      if (it != paths.end() - 1)
        result.append(kSearchPathSeparatorStr);
    }
    return result;
  }

  /**
   * Gets a module name with optional directory components, returns
   * module search path and the name without directory components.
   */
  static void PrepareModuleName(const char *name,
                                std::vector<std::string> *search_paths,
                                std::string *module_name) {
    // for name with absolute path, just return.
    if (name && *name == kDirSeparator) {
      *module_name = std::string(name);
      return;
    }

    std::string dirname;
    if (name && *name) {
      std::string str(name);
      size_t pos = str.rfind(kDirSeparator);
      if (pos != std::string::npos) {
        dirname = str.substr(0, pos);
        str.erase(0, pos+1);
      }
      *module_name = str;
    }

    GetModulePaths(dirname.c_str(), search_paths);
  }

  static void NormalizeNameString(std::string *name) {
    for (std::string::iterator it = name->begin();
         it != name->end(); ++it) {
      // Convert all characters other than alpha and numeric to underscore,
      // because a symbol can't contain these characters.
      if (!isalnum(static_cast<int>(*it)))
        *it = '_';
    }
  }

  static std::string ConcatenateLtdlPrefix(const char *name,
                                           const char *symbol) {
    std::string prefix(name);
    NormalizeNameString(&prefix);
    return prefix + std::string("_LTX_") + std::string(symbol);
  }

  static void *GetModuleSymbol(lt_dlhandle handle,
                               const char *module_name,
                               const char *symbol_name) {
    void *result = lt_dlsym(handle, symbol_name);

    // If symbol load failed, try to add LTX prefix and load again.
    // In case ltdl didn't strip the symbol prefix.
    if (!result) {
#ifdef _DEBUG
      const char *err = lt_dlerror();
      if (err)
        DLOG("Failed to get symbol %s from module %s: %s",
             symbol_name, module_name, err);
#endif
      std::string symbol = ConcatenateLtdlPrefix(module_name, symbol_name);
      result = lt_dlsym(handle, symbol.c_str());

      // Failed again? Try to prepend a under score to the symbol name.
      if (!result) {
#ifdef _DEBUG
        const char *err = lt_dlerror();
        if (err)
          DLOG("Failed to get symbol %s from module %s: %s",
               symbol.c_str(), module_name, err);
#endif
        symbol.insert(symbol.begin(), '_');
        result = lt_dlsym(handle, symbol.c_str());
#ifdef _DEBUG
        if (!result) {
          const char *err = lt_dlerror();
          if (err)
            DLOG("Failed to get symbol %s from module %s: %s",
                 symbol.c_str(), module_name, err);
        }
#endif
      }
    }

    return result;
  }

 private:
  typedef bool (*InitializeFunction)(void);
  typedef void (*FinalizeFunction)(void);

  lt_dlhandle handle_;
  InitializeFunction initialize_;
  FinalizeFunction finalize_;
  std::string path_;
  std::string name_;
};

Module::Module()
  : impl_(new Impl()) {
}

Module::Module(const char *name)
  : impl_(new Impl()) {
  Load(name);
}

Module::~Module() {
  delete impl_;
}

bool Module::Load(const char *name) {
  return impl_->Load(name);
}

bool Module::Unload() {
  return impl_->Unload();
}

bool Module::IsValid() const {
  return impl_->IsValid();
}

bool Module::MakeResident() {
  return impl_->MakeResident();
}

bool Module::IsResident() const {
  return impl_->IsResident();
}

std::string Module::GetPath() const {
  return impl_->GetPath();
}

std::string Module::GetName() const {
  return impl_->GetName();
}

void *Module::GetSymbol(const char *symbol_name) const {
  return impl_->GetSymbol(symbol_name);
}

bool Module::EnumerateModulePaths(Slot1<bool, const char *> *callback) {
  ASSERT(callback);

  std::vector<std::string> paths;
  Impl::GetModulePaths(NULL, &paths);

  bool result = false;
  for (std::vector<std::string>::iterator it = paths.begin();
       it != paths.end(); ++it) {
    result = (*callback)(it->c_str());
    if (!result) break;
  }

  delete callback;
  return result;
}

bool Module::EnumerateModuleFiles(const char *path,
                                  Slot1<bool, const char *> *callback) {
  ASSERT(callback);

  std::vector<std::string> modules;
  Impl::GetModuleList(path, &modules);

  bool result = false;
  for (std::vector<std::string>::iterator it = modules.begin();
       it != modules.end(); ++it) {
    result = (*callback)(it->c_str());
    if (!result) break;
  }

  delete callback;
  return result;
}

} // namespace ggadget
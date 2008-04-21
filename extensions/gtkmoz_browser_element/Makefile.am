#
# Copyright 2007 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if GGL_BUILD_GTKMOZ_BROWSER_ELEMENT

INCLUDES		= -I$(top_builddir) \
			  -I$(top_srcdir)

noinst_HEADERS		= browser_child.h \
			  browser_element.h

extension_LTLIBRARIES	= gtkmoz-browser-element.la
extensiondir		= $(GGL_MODULE_DIR)

gtkmoz_browser_element_la_SOURCES = \
			  browser_element.cc

gtkmoz_browser_element_la_CXXFLAGS = \
			  $(DEFAULT_COMPILE_FLAGS)

gtkmoz_browser_element_la_CPPFLAGS = \
			  @GTK2_CFLAGS@ \
			  $(PREDEFINED_MACROS)

gtkmoz_browser_element_la_LIBADD = \
			  @GTK2_LIBS@ \
			  $(top_builddir)/ggadget/libggadget@GGL_EPOCH@.la

gtkmoz_browser_element_la_LDFLAGS = \
			  -avoid-version \
			  -module


ggllibexec_PROGRAMS	= gtkmoz-browser-child
ggllibexecdir		= $(GGL_LIBEXEC_DIR)

gtkmoz_browser_child_SOURCES= browser_child.cc \
			      ../smjs_script_runtime/json.cc \
			      ../../ggadget/digest_utils.cc \
			      ../../ggadget/logger.cc \
			      ../../ggadget/string_utils.cc \
			      ../../ggadget/unicode_utils.cc

gtkmoz_browser_child_CXXFLAGS	= $(DEFAULT_COMPILE_FLAGS) \
				  -Wno-non-virtual-dtor \
				  -Wno-strict-aliasing

gtkmoz_browser_child_CPPFLAGS	= @GTK2_CFLAGS@ \
				  @GTKMOZEMBED_CFLAGS@ \
				  $(PREDEFINED_MACROS)

gtkmoz_browser_child_LDADD	= @GTK2_LIBS@ \
				  @GTKMOZEMBED_LIBS@
endif
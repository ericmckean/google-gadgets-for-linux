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

function view_onOpen() {
  LoadMetadata();
}

function language_label_onsize() {
  language_box_div.x = language_label.offsetX +
                       language_label.offsetWidth + 7;
}

function plugin_description_onsize() {
  var max_height = 2 * plugin_title.offsetHeight;
  if (plugin_description.offsetHeight > max_height) {
    plugin_description.height = max_height;
    plugin_other_data.y = plugin_description.offsetY + max_height;
  } else {
    plugin_other_data.y = plugin_description.offsetY +
                          plugin_description.offsetHeight + 2;
  }
}

var gPageButtonAdjusted = false;
function page_label_onsize() {
  if (!gPageButtonAdjusted && page_label.innerText) {
    page_label.width = page_label.offsetWidth + 20;
    page_label.x = next_button.x - page_label.width;
    previous_button.x = page_label.x - previous_button.offsetWidth;
    gPageButtonAdjusted = true;
  }
}

function previous_button_onclick() {
  if (gCurrentPage > 0)
    SelectPage(gCurrentPage - 1);
}

function next_button_onclick() {
  if (gCurrentPage < GetTotalPages() - 1)
    SelectPage(gCurrentPage + 1);
}

function language_box_onchange() {
  SelectLanguage(language_box.children.item(language_box.selectedIndex).name);
}

var kPluginPrefix = "http://desktop.google.com";

// Plugin updated in recent two months are listed in the "new" category.
var kNewPluginPeriod = 2 * 30 * 86400000;
var kIGoogleFeedModuleId = 25;

var kSupportedLanguages = [
  "bg", "ca", "cs", "da", "de", "el", "en", "en-gb", "es", "fi", "fr", "hi",
  "hr", "hu", "id", "it", "ja", "ko", "nl", "no", "pl", "pt-br", "pt-pt",
  "ro", "ru", "sk", "sl", "sv", "th", "tr", "zh-cn", "zh-tw",
];

var kPluginTypeSearch = "search";
var kPluginTypeSidebar = "sidebar";
var kPluginTypeIGoogle = "igoogle";
var kPluginTypeFeed = "feed";

var kCategoryAll = "all";
var kCategoryNew = "new";
var kCategoryRecommendations = "recommendations";
var kCategoryGoogle = "google";
var kCategoryRecentlyUsed = "recently_used";
var kCategoryUpdates = "updates";

var kCategoryNews = "news";
var kCategorySports = "sports";
var kCategoryLifestyle = "lifestyle";
var kCategoryTools = "tools";
var kCategoryFinance = "finance";
var kCategoryFunGames = "fun_games";
var kCategoryTechnology = "technology";
var kCategoryCommunication = "communication";
var kCategoryHoliday = "holiday";

var kTopCategories = [
  kCategoryAll, kCategoryNew, kCategoryRecommendations, kCategoryGoogle,
  kCategoryRecentlyUsed, kCategoryUpdates,
];

var kBottomCategories = [
  kCategoryNews, kCategorySports, kCategoryLifestyle, kCategoryTools,
  kCategoryFinance, kCategoryFunGames, kCategoryTechnology,
  kCategoryCommunication, kCategoryHoliday,
];

// gPlugins is a two-level table. It is first indexed with language names,
// then with category names. The elements of the table are arrays of plugins.
// For example, gPlugins may contain the following value:
// {en:{news:[plugin1,plugin2,...],...}}.
var gPlugins = {};
var gCurrentCategory = "";
var gCurrentLanguage = "";
var gCurrentPage = 0;
var gCurrentPlugins = [];

// UI constants.
var kCategoryButtonHeight = category_active_img.height;
var kCategoryGap = 15;
var kPluginWidth = 134;
var kPluginHeight = 124;
var kPluginRows = 3;
var kPluginColumns = 4;
var kPluginsPerPage = kPluginRows * kPluginColumns;

// lang should be in kSupportedLanguages or equal to "others".
function GetDisplayLanguage(language) {
  return strings["LANGUAGE_" + language.replace("-", "_").toUpperCase()];
}

// category should be in kCategories, or "google" or "others".
function GetDisplayCategory(category) {
  return strings["CATEGORY_" + category.toUpperCase()];
}

function SplitValues(src) {
  return src ? src.split(",") : [];
}

function LoadMetadata() {
  debug.trace("begin loading metadata");
  var metadata = gadgetBrowserUtils.gadgetMetadata;
  gPlugins = {};
  var currentTime = new Date().getTime();
  for (var i = 0; i < metadata.length; i++) {
    var plugin = metadata[i];
    var attrs = plugin.attributes;
    if ((attrs.module_id != kIGoogleFeedModuleId ||
         attrs.type != kPluginTypeIGoogle) &&
        attrs.sidebar == "false") {
      // This plugin is not for desktop, ignore it.
      // IGoogle feeding gadgets are supported even if marked sidebar="false".
      continue;
    }

    var languages = SplitValues(attrs.language);
    if (languages.length == 0)
      languages.push("en");
    var categories = SplitValues(attrs.category);
    // TODO: other special categories.
    plugin.rank = parseFloat(attrs.rank);

    for (var lang_i in languages) {
      var language = languages[lang_i];
      var language_categories = gPlugins[language];
      if (!language_categories) {
        language_categories = gPlugins[language] = {};
        language_categories[kCategoryAll] = [];
        language_categories[kCategoryNew] = [];
        language_categories[kCategoryRecentlyUsed] = [];
        language_categories[kCategoryUpdates] = [];
        // TODO: recommendations.
      }

      language_categories[kCategoryAll].push(plugin);
      if (currentTime - plugin.updated_date.getTime() < kNewPluginPeriod)
        language_categories[kCategoryNew].push(plugin);

      for (var cate_i in categories) {
        var category = categories[cate_i];
        var category_plugins = language_categories[category];
        if (!category_plugins)
          category_plugins = language_categories[category] = [];
        category_plugins.push(plugin);
      }
    }
  }
  debug.trace("finished loading metadata");
  UpdateLanguageBox();
}

function UpdateLanguageBox() {
  language_box.removeAllElements();
  for (var i in kSupportedLanguages) {
    var language = kSupportedLanguages[i];
    if (language in gPlugins) {
      language_box.appendElement(
        "<item name='" + language +
        "'><label valign='middle'>" + GetDisplayLanguage(language) +
        "</label></item>");
    }
  }
  // TODO: Default language.
  SelectLanguage("en");
}

function SelectLanguage(language) {
  gCurrentLanguage = language;
  UpdateCategories();
}

function AddCategoryButton(category, y) {
  categories_div.appendElement(
    "<label align='left' valign='middle' enabled='true' color='#FFFFFF' name='" +
    category +
    "' width='100%' height='" + kCategoryButtonHeight + "' y='" + y +
    "' onmouseover='category_onmouseover()' onmouseout='category_onmouseout()'" +
    " onclick='SelectCategory(\"" + category + "\")'>&#160;" +
    GetDisplayCategory(category) + "</label>");
}

function category_onmouseover() {
  category_hover_img.y = event.srcElement.offsetY;
  category_hover_img.visible = true;
}

function category_onmouseout() {
  category_hover_img.visible = false;
}

function ComparePluginDate(p1, p2) {
  var date1 = p1.updated_date.getTime();
  var date2 = p2.updated_date.getTime();
  return date1 == date2 ? 0 : date1 < date2 ? 1 : -1;
}

function ComparePluginRank(p1, p2) {
  var rank1 = p1.rank;
  var rank2 = p2.rank;
  return rank1 == rank2 ? 0 : rank1 < rank2 ? 1 : -1;
}

function SelectCategory(category) {
  gCurrentCategory = category;
  if (category) {
    category_active_img.y = categories_div.children.item(category).offsetY;
    category_active_img.visible = true;
    gCurrentPlugins = gPlugins[gCurrentLanguage][gCurrentCategory];
    if (!gCurrentPlugins.sorted) {
      debug.trace("begin sorting");
      if (gCurrentCategory == kCategoryRecentlyUsed)
        ; // TODO: sort by recency
      else if (gCurrentCategory == kCategoryNew)
        gCurrentPlugins.sort(ComparePluginDate);
      else
        gCurrentPlugins.sort(ComparePluginRank);
      gCurrentPlugins.sorted = true;
      debug.trace("end sorting");
    }
    SelectPage(0);
    ResetSearchBox();
  } else {
    category_active_img.visible = false;
    gCurrentPlugins = [];
  }
}

function GetPluginTitle(plugin) {
  if (!plugin) return "";
  var result = plugin.titles[gCurrentLanguage];
  return result ? result : plugin.attributes.name;
}

function GetPluginDescription(plugin) {
  if (!plugin) return "";
  var result = plugin.descriptions[gCurrentLanguage];
  return result ? result : plugin.attributes.product_summary;
}

function GetPluginOtherData(plugin) {
  if (!plugin) return "";
  var result = "";
  var attrs = plugin.attributes;
  if (attrs.version) {
    result += strings.PLUGIN_VERSION;
    result += attrs.version;
  }
  if (attrs.size_kilobytes) {
    if (result) result += strings.PLUGIN_DATA_SEPARATOR;
    result += attrs.size_kilobytes;
    result += strings.PLUGIN_KILOBYTES;
  }
  if (plugin.updated_date) {
    if (result) result += strings.PLUGIN_DATA_SEPARATOR;
    result += plugin.updated_date.toLocaleDateString();
  }
  if (attrs.author) {
    if (result) result += strings.PLUGIN_DATA_SEPARATOR;
    result += attrs.author;
  }
  if (result)
    result = strings.PLUGIN_DATA_BULLET + result;
  // For debug:
  // result += "/" + attrs.uuid;
  // result += "/" + attrs.id;
  // result += "/" + attrs.rank;
  return result;
}

function AddPluginBox(plugin, index, row, column) {
  var box = plugins_div.appendElement(
    "<div x='" + (column * kPluginWidth) + "' y='" + (row * kPluginHeight) +
    "' width='" + kPluginWidth + "' height='" + kPluginHeight +
    "' enabled='true' backgroundMode='stretchMiddle'" +
    " onmouseover='pluginbox_onmouseover(" + index + ")' onmouseout='pluginbox_onmouseout()'>" +
    " <label x='7' y='6' width='120' align='center' color='#FFFFFF' trimming='character-ellipsis'/>" +
    " <img x='16' y='75' opacity='70' src='images/thumbnails_shadow.png'/>" +
    " <div x='27' y='33' width='80' height='83' background='#FFFFFF'>" +
    "  <img width='80' height='60' src='images/default_thumbnail.jpg'/>" +
    "  <img y='60' width='80' height='60' src='images/default_thumbnail.jpg' flip='vertical'/>" +
    "  <img src='images/thumbnails_default_mask.png'/>" +
    " </div>" +
    " <button x='22' y='94' width='90' height='30' visible='false' stretchMiddle='true'" +
    "  image='images/add_button_default.png' downImage='images/add_button_down.png'" +
    "  overImage='images/add_button_hover.png'" +
    "  onmouseout='add_button_onmouseout()' onclick='AddPlugin(" + index + ")'" +
    "  color='#FFFFFF' caption='" + strings.ADD_BUTTON_LABEL + "'/>" +
    "</div>");

  // Set it here to prevent problems caused by special chars in the title.
  box.children.item(0).innerText = GetPluginTitle(plugin);

  var thumbnail_element1 = box.children.item(2).children.item(0);
  var thumbnail_element2 = box.children.item(2).children.item(1);
  AddThumbnailTask(plugin, thumbnail_element1, thumbnail_element2);
}

function AddPlugin(index) {
  var real_index = gCurrentPage * kPluginsPerPage + index;
}

function pluginbox_onmouseover(index) {
  MouseOverPlugin(event.srcElement, index);
}

function MouseOverPlugin(box, index) {
  box.background = "images/thumbnails_hover.png";
  box.children.item(2).children.item(2).src = "images/thumbnails_hover_mask.png";
  // Show the "Add" button.
  box.children.item(3).visible = true;

  var plugin = gCurrentPlugins[index];
  plugin_title.innerText = GetPluginTitle(plugin);
  plugin_description.innerText = GetPluginDescription(plugin);
  plugin_description.height = undefined;
  plugin_other_data.innerText = GetPluginOtherData(plugin);
}

function pluginbox_onmouseout() {
  var box = event.srcElement;
  // Exclude the case that the mouse moved over the "Add" button.
  if (!view.mouseOverElement || view.mouseOverElement.parentElement != box)
    MouseOutPlugin(box);
}

function add_button_onmouseout() {
  var box = event.srcElement.parentElement;
  // The mouse may directly moved out of the pluginbox.
  if (!view.mouseOverElement || view.mouseOverElement != box)
    MouseOutPlugin(box);
}

function MouseOutPlugin(box) {
  box.background = "";
  box.children.item(2).children.item(2).src = "images/thumbnails_default_mask.png";
  // Hide the "Add" button.
  box.children.item(3).visible = false;
  plugin_title.innerText = "";
  plugin_description.innerText = "";
  plugin_other_data.innerText = "";
}

function GetTotalPages() {
  if (!gCurrentPlugins || !gCurrentPlugins.length) {
    // Return 1 instead of 0 to avoid '1 of 0'.
    return 1;
  }
  return Math.ceil(gCurrentPlugins.length / kPluginsPerPage);
}

function SelectPage(page) {
  plugins_div.removeAllElements();
  plugin_title.innerText = "";
  plugin_description.innerText = "";
  plugin_other_data.innerText = "";

  ClearThumbnailTasks();
  if (!gCurrentPlugins || !gCurrentPlugins.length) {
    gCurrentPage = 0;
  } else {
    gCurrentPage = page;
    var start = page * kPluginsPerPage;
outer:
    for (var r = 0; r < kPluginRows; r++) {
      for (var c = 0; c < kPluginColumns; c++) {
        var i = start + c;
        if (i >= gCurrentPlugins.length)
          break outer;
        AddPluginBox(gCurrentPlugins[i], i, r, c);
      }
      start += kPluginColumns;
    }
  }
  page_label.innerText = strings.PAGE_LABEL.replace("{{PAGE}}", page + 1)
                         .replace("{{TOTAL}}", GetTotalPages());
  previous_button.visible = next_button.visible = page_label.visible = true;
}

function UpdateCategories() {
  category_hover_img.visible = category_active_img.visible = false;
  var y = 0;
  categories_div.removeAllElements();
  var language_categories = gPlugins[gCurrentLanguage];
  for (var i in kTopCategories) {
    var category = kTopCategories[i];
    if (category in language_categories) {
      AddCategoryButton(category, y);
      y += kCategoryButtonHeight;
    }
  }
  y += kCategoryGap;
  for (var i in kBottomCategories) {
    var category = kBottomCategories[i];
    if (category in language_categories) {
      AddCategoryButton(category, y);
      y += kCategoryButtonHeight;
    }
  }
  // TODO: ....
  SelectCategory(kCategoryAll);
}

function ResetSearchBox() {
  search_box.value = strings.SEARCH_GADGETS;
  search_box.color = "#808080";
}

var kSearchDelay = 500;
var gSearchTimer = 0;
var gInFocusHandler = false;

function search_box_onfocusout() {
  gInFocusHandler = true
  if (search_box.value == "")
    ResetSearchBox();
  gInFocusHandler = false;
}

function search_box_onfocusin() {
  gInFocusHandler = true;
  if (search_box.value == strings.SEARCH_GADGETS) {
    search_box.value = "";
    search_box.color = "#000000";
  }
  gInFocusHandler = false;
}

function search_box_onchange() {
  if (gSearchTimer) cancelTimer(gSearchTimer);
  if (gInFocusHandler || search_box.value == strings.SEARCH_GADGETS)
    return;

  if (search_box.value == "") {
    SelectCategory(kCategoryAll); // TODO: kCategoryRecommendations.
    // Undo the actions in ResetSearchBox() called by SelectCategory().
    search_box_onfocusin();
  } else {
    setTimeout(DoSearch, kSearchDelay);
  }
}

function DoSearch() {
  if (!search_box.value || search_box.value == strings.SEARCH_GADGETS)
    return;

  debug.trace("search begins");
  var terms = search_box.value.toUpperCase().split(" ");
  var plugins = gPlugins[gCurrentLanguage][kCategoryAll];
  var result = [];
  if (plugins && plugins.length) {
    for (var i = 0; i < plugins.length; i++) {
      var plugin = plugins[i];
      var attrs = plugin.attributes;
      var indexable_text = [
        attrs.author ? attrs.author : "",
        plugin.updated_date ? plugin.updated_date.toLocaleDateString() : "",
        attrs.download_url ? attrs.download_url : "",
        attrs.info_url ? attrs.info_url : "",
        attrs.keywords ? attrs.keywords : "",
        GetPluginTitle(plugin),
        GetPluginDescription(plugin),
      ].join(" ").toUpperCase();

      var ok = true;
      for (var j in terms) {
        if (terms[j] && indexable_text.indexOf(terms[j]) == -1) {
          ok = false;
          break;
        }
      }
      if (ok) result.push(plugin);
    }
  }

  SelectCategory(null);
  gCurrentPlugins = result;
  SelectPage(0);
  debug.trace("search ends");
}
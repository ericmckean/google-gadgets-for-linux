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

#include <cstdio>
#include "ggadget/unicode_utils.h"
#include "unittest/gtest.h"

using namespace ggadget;

static const UTF32Char utf32_string[] = {
  0x0061, 0x0080, 0x07ff, 0x0800, 0x1fff, 0x2000, 0xd7ff,
  0xe000, 0xffff, 0x10000, 0x22000, 0xeffff,
  0xf0000, 0x10aaff, 0
};

static const size_t utf8_length[] = {
  1, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 0
};

static const char utf8_string[] = {
  0x61,0xc2,0x80,0xdf,0xbf,0xe0,0xa0,0x80,0xe1,0xbf,0xbf,0xe2,0x80,0x80,
  0xed,0x9f,0xbf,0xee,0x80,0x80,0xef,0xbf,0xbf,0xf0,0x90,0x80,0x80,
  0xf0,0xa2,0x80,0x80,0xf3,0xaf,0xbf,0xbf,0xf3,0xb0,0x80,0x80,
  0xf4,0x8a,0xab,0xbf, 0
};

static const size_t utf16_length[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 0
};

static const UTF16Char utf16_string[] = {
  0x0061, 0x0080, 0x07ff, 0x0800, 0x1fff, 0x2000, 0xd7ff,
  0xe000, 0xffff, 0xd800, 0xdc00, 0xd848, 0xdc00, 0xdb7f,
  0xdfff, 0xdb80, 0xdc00, 0xdbea, 0xdeff, 0
};

static const size_t invalid_utf8_length = 8;
static const char invalid_utf8_string[] = {
  //---------------------------------------v
  0x61,0xc2,0x80,0xdf,0xbf,0xe0,0xa0,0x80,0xb1,0xbf,0xbf,0xe2,0x80,0x80, 0
};

static const size_t invalid_utf16_length = 9;
static const UTF16Char invalid_utf16_string[] = {
  0x0061, 0x0080, 0x07ff, 0x0800, 0x1fff, 0x2000, 0xd7ff,
  0xe000, 0xffff, 0xd800, 0xc200, 0xd848, 0xdc00, 0xdb7f,
  //-------------------------^
  0
};

static const size_t invalid_utf32_length = 7;
static const UTF32Char invalid_utf32_string[] = {
  0x0061, 0x0080, 0x07ff, 0x0800, 0x1fff, 0x2000, 0xd7ff,
  0xd820, 0xffff, 0
};


TEST(UnicodeUtils, ConvertChar) {
  const char *utf8_ptr = utf8_string;
  const UTF16Char *utf16_ptr = utf16_string;
  const UTF32Char *utf32_ptr = utf32_string;
  UTF32Char utf32;
  char utf8[6];
  UTF16Char utf16[2];
  for (size_t i = 0; utf8_length[i]; ++i) {
    EXPECT_EQ(utf8_length[i],
              ConvertCharUTF8ToUTF32(utf8_ptr, utf8_length[i], &utf32));
    EXPECT_EQ(*utf32_ptr, utf32);
    EXPECT_EQ(utf8_length[i],
              ConvertCharUTF32ToUTF8(utf32, utf8, 6));
    EXPECT_EQ(0,
              strncmp(utf8, utf8_ptr, utf8_length[i]));
    EXPECT_EQ(utf16_length[i],
              ConvertCharUTF16ToUTF32(utf16_ptr, utf16_length[i], &utf32));
    EXPECT_EQ(*utf32_ptr, utf32);
    EXPECT_EQ(utf16_length[i],
              ConvertCharUTF32ToUTF16(utf32, utf16, 2));
    EXPECT_EQ(0, memcmp(utf16, utf16_ptr, sizeof(UTF16Char) * utf16_length[i]));
    utf8_ptr += utf8_length[i];
    utf16_ptr += utf16_length[i];
    utf32_ptr ++;
  }
}

TEST(UnicodeUtils, ConvertString) {
  std::string utf8;
  std::string orig_utf8(utf8_string);
  UTF16String utf16;
  UTF16String orig_utf16(utf16_string);
  UTF32String utf32;
  UTF32String orig_utf32(utf32_string);

  EXPECT_EQ(orig_utf8.length(), ConvertStringUTF8ToUTF32(orig_utf8, &utf32));
  EXPECT_TRUE(utf32 == orig_utf32);
  EXPECT_EQ(orig_utf32.length(), ConvertStringUTF32ToUTF8(orig_utf32, &utf8));
  EXPECT_TRUE(utf8 == orig_utf8);
  EXPECT_EQ(orig_utf16.length(), ConvertStringUTF16ToUTF32(orig_utf16, &utf32));
  EXPECT_TRUE(utf32 == orig_utf32);
  EXPECT_EQ(orig_utf32.length(), ConvertStringUTF32ToUTF16(orig_utf32, &utf16));
  EXPECT_TRUE(utf16 == orig_utf16);
  EXPECT_EQ(orig_utf8.length(), ConvertStringUTF8ToUTF16(orig_utf8, &utf16));
  EXPECT_TRUE(utf16 == orig_utf16);
  EXPECT_EQ(orig_utf16.length(), ConvertStringUTF16ToUTF8(orig_utf16, &utf8));
  EXPECT_TRUE(utf8 == orig_utf8);
}

TEST(UnicodeUtils, Invalid) {
  std::string utf8;
  std::string orig_utf8(invalid_utf8_string);
  UTF16String utf16;
  UTF16String orig_utf16(invalid_utf16_string);
  UTF32String utf32;
  UTF32String orig_utf32(invalid_utf32_string);

  EXPECT_EQ(invalid_utf8_length, ConvertStringUTF8ToUTF32(orig_utf8, &utf32));
  EXPECT_EQ(invalid_utf32_length, ConvertStringUTF32ToUTF8(orig_utf32, &utf8));
  EXPECT_EQ(invalid_utf16_length, ConvertStringUTF16ToUTF32(orig_utf16, &utf32));
  EXPECT_EQ(invalid_utf32_length, ConvertStringUTF32ToUTF16(orig_utf32, &utf16));
  EXPECT_EQ(invalid_utf8_length, ConvertStringUTF8ToUTF16(orig_utf8, &utf16));
  EXPECT_EQ(invalid_utf16_length, ConvertStringUTF16ToUTF8(orig_utf16, &utf8));
  EXPECT_EQ(0U, ConvertStringUTF8ToUTF32(NULL, 0, &utf32));
  EXPECT_EQ(0U, ConvertStringUTF32ToUTF8(NULL, 0, &utf8));
  EXPECT_EQ(0U, ConvertStringUTF16ToUTF32(NULL, 0, &utf32));
  EXPECT_EQ(0U, ConvertStringUTF32ToUTF16(NULL, 0, &utf16));
  EXPECT_EQ(0U, ConvertStringUTF8ToUTF16(NULL, 0, &utf16));
  EXPECT_EQ(0U, ConvertStringUTF16ToUTF8(NULL, 0, &utf8));
}

TEST(UnicodeUtils, IsLegalString) {
  EXPECT_TRUE(IsLegalUTF8String("", 0));
  EXPECT_FALSE(IsLegalUTF8String(NULL, 0));
  EXPECT_TRUE(IsLegalUTF8String(std::string("")));
  EXPECT_TRUE(IsLegalUTF8String(utf8_string, strlen(utf8_string)));
  EXPECT_TRUE(IsLegalUTF8String(std::string(utf8_string)));
  EXPECT_FALSE(IsLegalUTF8String(invalid_utf8_string,
                                 strlen(invalid_utf8_string)));
  EXPECT_FALSE(IsLegalUTF8String(std::string(invalid_utf8_string)));
  EXPECT_TRUE(IsLegalUTF8String(invalid_utf8_string, invalid_utf8_length));

  EXPECT_TRUE(IsLegalUTF16String(utf16_string, 0));
  EXPECT_FALSE(IsLegalUTF16String(NULL, 0));
  EXPECT_TRUE(IsLegalUTF16String(UTF16String(utf16_string, 0)));
  EXPECT_TRUE(IsLegalUTF16String(utf16_string, arraysize(utf16_string) - 1));
  EXPECT_TRUE(IsLegalUTF16String(UTF16String(utf16_string)));
  EXPECT_FALSE(IsLegalUTF16String(invalid_utf16_string,
                                  arraysize(invalid_utf16_string) - 1));
  EXPECT_FALSE(IsLegalUTF16String(UTF16String(invalid_utf16_string)));
  EXPECT_TRUE(IsLegalUTF16String(invalid_utf16_string, invalid_utf16_length));
}

TEST(UnicodeUtils, DetectUTFEncoding) {
  std::string encoding("Garbage");
  EXPECT_FALSE(DetectUTFEncoding(std::string(""), &encoding));
  EXPECT_STREQ("", encoding.c_str());
  EXPECT_FALSE(DetectUTFEncoding(std::string("ABCDEF"), &encoding));
  EXPECT_STREQ("", encoding.c_str());

  std::string utf8input(kUTF8BOM, sizeof(kUTF8BOM));
  EXPECT_TRUE(DetectUTFEncoding(utf8input, &encoding));
  EXPECT_STREQ("UTF-8", encoding.c_str());
  utf8input += "Some";
  EXPECT_TRUE(DetectUTFEncoding(utf8input, &encoding));
  EXPECT_STREQ("UTF-8", encoding.c_str());

  std::string utf16le_input(kUTF16LEBOM, sizeof(kUTF16LEBOM));
  EXPECT_TRUE(DetectUTFEncoding(utf16le_input, &encoding));
  EXPECT_STREQ("UTF-16LE", encoding.c_str());
  utf16le_input.append("S\0o\0m\0e\0", 8);
  EXPECT_TRUE(DetectUTFEncoding(utf16le_input, &encoding));
  EXPECT_STREQ("UTF-16LE", encoding.c_str());
  // BOM-less UTF16.
  utf16le_input.assign("S\0o\0m\0e\0", 8);
  EXPECT_TRUE(DetectUTFEncoding(utf16le_input, &encoding));
  EXPECT_STREQ("UTF-16LE", encoding.c_str());
  utf16le_input.assign("S\0o\0m\0\0\0", 8);
  EXPECT_FALSE(DetectUTFEncoding(utf16le_input, &encoding));
  EXPECT_STREQ("", encoding.c_str());
  utf16le_input.assign("S\0oo\0m\0e\0", 8);
  EXPECT_FALSE(DetectUTFEncoding(utf16le_input, &encoding));
  EXPECT_STREQ("", encoding.c_str());
  utf16le_input.assign("S\0o\0m\0e", 7);
  EXPECT_FALSE(DetectUTFEncoding(utf16le_input, &encoding));
  EXPECT_STREQ("", encoding.c_str());

  std::string utf16be_input(kUTF16BEBOM, sizeof(kUTF16BEBOM));
  EXPECT_TRUE(DetectUTFEncoding(utf16be_input, &encoding));
  EXPECT_STREQ("UTF-16BE", encoding.c_str());
  utf16be_input.append("\0S\0o\0m\0e", 8);
  EXPECT_TRUE(DetectUTFEncoding(utf16be_input, &encoding));
  EXPECT_STREQ("UTF-16BE", encoding.c_str());
  // BOM-less UTF16.
  utf16be_input.assign("\0S\0o\0m\0e", 8);
  EXPECT_TRUE(DetectUTFEncoding(utf16be_input, &encoding));
  EXPECT_STREQ("UTF-16BE", encoding.c_str());
  utf16be_input.assign("\0S\0o\0m\0\0", 8);
  EXPECT_FALSE(DetectUTFEncoding(utf16be_input, &encoding));
  EXPECT_STREQ("", encoding.c_str());
  utf16be_input.assign("\0Soo\0m\0e", 8);
  EXPECT_FALSE(DetectUTFEncoding(utf16be_input, &encoding));
  EXPECT_STREQ("", encoding.c_str());
  utf16be_input.assign("\0S\0o\0m\0", 7);
  EXPECT_FALSE(DetectUTFEncoding(utf16be_input, &encoding));
  EXPECT_STREQ("", encoding.c_str());

  std::string utf32le_input(kUTF32LEBOM, sizeof(kUTF32LEBOM));
  EXPECT_TRUE(DetectUTFEncoding(utf32le_input, &encoding));
  EXPECT_STREQ("UTF-32LE", encoding.c_str());
  utf32le_input.append("S\0\0\0o\0\0\0m\0\0\0e\0\0\0", 16);
  EXPECT_TRUE(DetectUTFEncoding(utf32le_input, &encoding));
  EXPECT_STREQ("UTF-32LE", encoding.c_str());

  std::string utf32be_input(kUTF32BEBOM, sizeof(kUTF32BEBOM));
  EXPECT_TRUE(DetectUTFEncoding(utf32be_input, &encoding));
  EXPECT_STREQ("UTF-32BE", encoding.c_str());
  utf32le_input.append("\0\0\0S\0\0\0o\0\0\0m\0\0\0e", 16);
  EXPECT_TRUE(DetectUTFEncoding(utf32be_input, &encoding));
  EXPECT_STREQ("UTF-32BE", encoding.c_str());
}

TEST(UnicodeUtils, DetectAndConvertStreamToUTF8) {
  std::string encoding("Garbage");
  std::string result("Garbage");
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(std::string(""), &result,
                                           &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("UTF-8", encoding.c_str());
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(std::string("ABCDEF"), &result,
                                           NULL));
  EXPECT_STREQ("ABCDEF", result.c_str());

  std::string utf8bom(kUTF8BOM, sizeof(kUTF8BOM));
  std::string utf8input(kUTF8BOM, sizeof(kUTF8BOM));
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf8input, &result, &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("UTF-8", encoding.c_str());
  utf8input += "Some";
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf8input, &result, &encoding));
  EXPECT_STREQ("Some", result.c_str());
  EXPECT_STREQ("UTF-8", encoding.c_str());

  std::string utf16le_input(kUTF16LEBOM, sizeof(kUTF16LEBOM));
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf16le_input, &result, &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("UTF-16LE", encoding.c_str());
  utf16le_input.append("S\0o\0m\0e\0", 8);
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf16le_input, &result, &encoding));
  EXPECT_STREQ("Some", result.c_str());
  EXPECT_STREQ("UTF-16LE", encoding.c_str());
  std::string utf16le_input_extra = utf16le_input + "1";
  EXPECT_FALSE(DetectAndConvertStreamToUTF8(utf16le_input_extra, &result,
                                           &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("", encoding.c_str());

  std::string utf16be_input(kUTF16BEBOM, sizeof(kUTF16BEBOM));
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf16be_input, &result, &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("UTF-16BE", encoding.c_str());
  utf16be_input.append("\0S\0o\0m\0e", 8);
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf16be_input, &result, &encoding));
  EXPECT_STREQ("Some", result.c_str());
  EXPECT_STREQ("UTF-16BE", encoding.c_str());
  std::string utf16be_input_extra = utf16be_input + "1";
  EXPECT_FALSE(DetectAndConvertStreamToUTF8(utf16be_input_extra, &result,
                                           &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("", encoding.c_str());

  std::string utf32le_input(kUTF32LEBOM, sizeof(kUTF32LEBOM));
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf32le_input, &result, &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("UTF-32LE", encoding.c_str());
  utf32le_input.append("S\0\0\0o\0\0\0m\0\0\0e\0\0\0", 16);
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf32le_input, &result, &encoding));
  EXPECT_STREQ("Some", result.c_str());
  EXPECT_STREQ("UTF-32LE", encoding.c_str());
  std::string utf32le_input_extra = utf32le_input + "123";
  EXPECT_FALSE(DetectAndConvertStreamToUTF8(utf32le_input_extra, &result,
                                            &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("", encoding.c_str());

  std::string utf32be_input(kUTF32BEBOM, sizeof(kUTF32BEBOM));
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf32be_input, &result, &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("UTF-32BE", encoding.c_str());
  utf32be_input.append("\0\0\0S\0\0\0o\0\0\0m\0\0\0e", 16);
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf32be_input, &result, &encoding));
  EXPECT_STREQ("Some", result.c_str());
  EXPECT_STREQ("UTF-32BE", encoding.c_str());
  std::string utf32be_input_extra = utf32be_input + "123";
  EXPECT_FALSE(DetectAndConvertStreamToUTF8(utf32be_input_extra, &result,
                                           &encoding));
  EXPECT_STREQ("", result.c_str());
  EXPECT_STREQ("", encoding.c_str());

  std::string invalid_input("\x61\xc2\x80\xdf\xbf\xe0");
  EXPECT_TRUE(DetectAndConvertStreamToUTF8(invalid_input, &result, &encoding));
  EXPECT_STREQ("\x61\xc3\x82\xc2\x80\xc3\x9f\xc2\xbf\xc3\xa0", result.c_str());
  EXPECT_STREQ("ISO8859-1", encoding.c_str());

  EXPECT_TRUE(DetectAndConvertStreamToUTF8(utf8bom + invalid_input, &result,
                                           &encoding));
  EXPECT_STREQ("\xc3\xaf\xc2\xbb\xc2\xbf\x61\xc3\x82\xc2\x80\xc3\x9f\xc2\xbf\xc3\xa0",
               result.c_str());
  EXPECT_STREQ("ISO8859-1", encoding.c_str());
}

TEST(UnicodeUtils, ConvertLocaleStringToUTF16) {
  UTF16String result;
  EXPECT_TRUE(ConvertLocaleStringToUTF16("", &result));
  EXPECT_TRUE(UTF16String() == result);
  UTF16Char expected[] = { 'a', 'b', 'c', 'd', 0 };
  EXPECT_TRUE(ConvertLocaleStringToUTF16("abcd", &result));
  EXPECT_TRUE(UTF16String(expected) == result);
  // TODO: Test locale-specific features.
}

TEST(UnicodeUtils, ConvertLocaleStringToUTF8) {
  std::string result;
  EXPECT_TRUE(ConvertLocaleStringToUTF8("", &result));
  EXPECT_EQ(std::string(), result);
  EXPECT_TRUE(ConvertLocaleStringToUTF8("abcd", &result));
  EXPECT_EQ(std::string("abcd"), result);
  // TODO: Test locale-specific features.
}

TEST(UnicodeUtils, ConvertUTF16ToLocaleString) {
  UTF16Char input[] = { 0 };
  std::string result;
  EXPECT_TRUE(ConvertUTF16ToLocaleString(input, &result));
  EXPECT_EQ(std::string(), result);
  UTF16Char input1[] = { 'a', 'b', 'c', 'd', 0 };
  EXPECT_TRUE(ConvertUTF16ToLocaleString(input1, &result));
  EXPECT_EQ("abcd", result);
  // TODO: Test locale-specific features.
}

TEST(UnicodeUtils, ConvertUTF8ToLocaleString) {
  std::string result;
  EXPECT_TRUE(ConvertUTF8ToLocaleString("", &result));
  EXPECT_EQ(std::string(), result);
  EXPECT_TRUE(ConvertUTF8ToLocaleString("abcd", &result));
  EXPECT_EQ("abcd", result);
  // TODO: Test locale-specific features.
}

TEST(UnicodeUtils, CompareLocaleStrings) {
  EXPECT_EQ(0, CompareLocaleStrings("", ""));
  EXPECT_GT(0, CompareLocaleStrings("", "a"));
  EXPECT_LT(0, CompareLocaleStrings("a", ""));
  EXPECT_EQ(0, CompareLocaleStrings("abc", "abc"));
  EXPECT_GT(0, CompareLocaleStrings("abc", "def"));
  EXPECT_LT(0, CompareLocaleStrings("def", "abc"));
  // TODO: Test locale-specific features.
}

int main(int argc, char **argv) {
  testing::ParseGTestFlags(&argc, argv);

  return RUN_ALL_TESTS();
}
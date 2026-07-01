// C++ demo for UTF-8-ready routines.
//
// Certain functions in this file comprise machine-generated code and are 
// described as such in the relevant comments.  All other code in this file 
// is copyright 2026 Kirk J Krauss and is a Derivative Work based on material 
// that is copyright 2025 Kirk J Krauss and available at
//
//     https://developforperformance.com/MatchingWildcardsInGoSwiftAndCpp.html
// 
// Licensed under the Apache License, Version 2.0 (the "License"); you may not 
// use this file except in compliance with the License.  You may obtain a copy 
// of the License at
// 
//     https://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software 
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT 
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the 
// License for the specific language governing permissions and limitations 
// under the License.
//
#if !defined(__cplusplus)
#error "This is a demo program for the FastUtf8 C++ class."
#error "Testcases for the C-style functions in fastutf8.cpp are available."
#error "See testutf8.cpp and its documentation."
#endif

#if defined(_WIN32)
#include <windows.h>      // For SetConsoleOutputCP()
#endif  // _WIN32

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>
#include <chrono>
#include "fastutf8.h"
using namespace FastUtf8;

// Demonstrates this FastUtf8::Uniseries functionality for ASCII:
//
//   Fast one-call whole content case folding (lowercasing);
//   Case-insensitive whole content comparison;
//   Case-insensitive partial content comparison and find (returning an index);
//   Wildcard comparison, with / without case sensitivity;
//   Content separation over a buffer that remains in place (speedy!); and
//   Trimming of outboard white space.
//
int AsciiUniseriesMultiFuncDemo(void)
{
   // Declarations of multilingual variants of "Uniseries ASCII demo."
   Uniseries sDemo = "Uniseries ASCII demo";

   // Case-folding demo.
   std::cout << "Case-folding (lowercasing) example:"  << std::endl;

   // This first call is for UTF-8 compatibility.  Can use strlen() for ASCII, 
   // as an alternative.
   size_t sizeFolded = Uniseries::getSizeFolded(sDemo);
   Uniseries sDemoFolded1 = Uniseries::getFolded(sDemo, sizeFolded);
   Uniseries sDemoFolded2 = "uniseries ascii demo";

   if (sDemoFolded1 == sDemoFolded2)
   {
      std::cout << "      Mixed case ASCII: " << 
         sDemo << std::endl;
      std::cout << "     Case-folded ASCII: " << 
         sDemoFolded1 << std::endl;
   }

   std::cout << std::endl;

   // Demo of case-insensitive whole content comparison.
   std::cout << "Case-insensitive whole content comparison:"  << std::endl;

   if (sDemo.caseCompare(sDemoFolded1))
   {
      std::cout << "     " << sDemoFolded1 << "  matches  " << 
         sDemo << std::endl;
   }

   std::cout << std::endl;

   // Demo of case-insensitive partial content comparison.
   std::cout << "Case-insensitive partial content comparison:"  << std::endl;

   Uniseries sAscii1 = sDemo + ", added text including MiXeD cAsE words";

   sAscii1.setCaseInsensitive();

   if (sAscii1.contains("mixed case"))
   {
      std::cout << "     This content..." << std::endl;
      std::cout << "        " << sAscii1 << std::endl;
      std::cout << "     ...contains the words..." << std::endl;
	  std::cout << "       mixed case" << std::endl;
   }

   std::cout << std::endl;

   // Demo of case-sensitive and case-insensitive partial content lookup.
   std::cout << "Case-insensitive partial content lookup:"  << std::endl;
   Uniseries sAscii2 = "This ASCII demo, with more text, has yet more";
   Uniseries sAscii3 = "The words, in multiple lines, are space-separated ";
   Uniseries sAscii4 = "Commas, too, can be separator characters";
   Uniseries sAsciiCombo = sAscii1 + "\n" + sAscii2 + "\n" + sAscii3 + 
      "\n" + sAscii4;

   // When we call ::find() on a Uniseries object that's set up for case-
   // sensitive comparison, we won't find an all-lowercase portion unless 
   // there's an all-lowercase match.
   std::cout << "  -- Setting case sensitivity to true -- "  << std::endl;
   sAsciiCombo.setCaseSensitivity(true);

   int iCommas1 = sAsciiCombo.find(sAscii4);
   int iCommas2 = sAsciiCombo.find("commas");

   if (iCommas1 > 0)
   {
      std::cout << "     " << sAscii4 << "  has index  " << 
         iCommas1 << std::endl;
   }

   // Here, "Commas" fails to match "commas".
   if (iCommas2 > 0)
   {
      std::cout << "     " << "\"commas\"" << "  has index  " << 
         iCommas2 << std::endl;
   }
   else
   {
      std::cout << 
         "     Cannot find \"commas\"" << 
         std::endl;
   }

   // By switching case sensitivity off, we change the default behavior for 
   // the comparison methods including ::find().
   std::cout << "  -- Setting case sensitivity to false -- "  << std::endl;
   sAsciiCombo.setCaseSensitivity(false);

   // Now that we've made that change, "Commas" matches "commas".
   iCommas2 = sAsciiCombo.find("commas");

   if (iCommas2 > 0)
   {
      std::cout << "     " << "\"commas\"" << "  has index  " << 
         iCommas2 << std::endl;
   }
   else
   {
      std::cout << 
         "     Cannot find \"commas\"" << 
         std::endl;
   }

   std::cout << std::endl;

   // Matching wildcards demo.
   std::cout << "Matching wildcards:"  << std::endl;

   if (sAsciiCombo.caseCompareWild("*ascii DEMO? WITH more*"))
   {
      std::cout << 
         "     This content..." << std::endl << sAsciiCombo << std::endl;
      std::cout << 
         "     ...matches the wildcarded and inverse-cased sequence..." << 
         std::endl;
	  std::cout <<  "       *ascii DEMO? WITH more*" << std::endl;
   }

   std::cout << std::endl;

   // Demo of content separation and trimming.
   std::cout << "Content separation and trimming:"  << std::endl;

   // We'll extract "separated" from the four lines of text we've created.
   // First we duplicate the Uniseries object.  The pSeparate() calls modify 
   // the content by replacing separator tokens with null terminators.  The 
   // tokens are the code points (in this case, ASCII characters) passed to 
   // Uniseries::pSeparate().
   Uniseries sComboDup = sAsciiCombo;

   // Separate the content by '-' characters.  For this demo, we capture 
   // but ignore the first portion.  (The compiler may warn us about that.)
   // The pSeparate() method relies on std::make_unique<FastUtf8::Uniseries> 
   // to get the pointer that it returns.
   std::unique_ptr<FastUtf8::Uniseries> psComboPart1 = 
                      sComboDup.pSeparate("-");

   // Separate the second portion by either newline or space characters.
   std::unique_ptr<FastUtf8::Uniseries> psComboPart2 = 
                      sComboDup.pSeparate("\n", /* bTrim = */ true);

   if (*psComboPart2 == "separated")  // note: without the trailing space
   {
      std::cout << "     Extracted this trimmed content..." << std::endl;
      std::cout << "        " << *psComboPart2 << std::endl;
      std::cout << "     ... from this content..." << 
         std::endl << sAsciiCombo << std::endl;
      std::cout << "     ... via two Uniseries::pSeparate() calls" << std::endl;
   }

   std::cout << std::endl;

   // Another example.
   // We'll find a numeric sequence in text separated by spaces and lines.
   Uniseries s123 = "123";
   Uniseries sTwoSeparatorCombo = "abc def ghi\n123 456 789\nrst uvw xyz";

   // We duplicate the Uniseries object like we did previously.
   sComboDup = sTwoSeparatorCombo;

   // Separate the text by newline characters only, then separate the second 
   // portion by either newline or space characters.  The content buffers 
   // previously allocated for the reused objects get deallocated here.  The 
   // Uniseries objects otherwise remain allocated until they go out of scope.
   psComboPart1 = sComboDup.pSeparate("\n");
   psComboPart2 = sComboDup.pSeparate(" \n");

   if (*psComboPart2 == s123)
   {
      std::cout << "     Extracted this content..." << std::endl;
      std::cout << "        " << *psComboPart2 << std::endl;
      std::cout << "     ... from this content..." << 
         std::endl << sTwoSeparatorCombo << std::endl;
      std::cout << "     ... via two Uniseries::pSeparate() calls" << std::endl;
   }

   std::cout << std::endl;
   return 0;
}

// Demonstrates this FastUtf8::Uniseries functionality for UTF-8:
//
//   UTF-8 support;
//   Easy-to-use case folding;
//   Case-insensitive whole content comparison;
//   Case-insensitive partial content comparison and find (returning an index);
//   Wildcard comparison, with / without case sensitivity;
//   Content separation over a buffer that remains in place; and
//   Trimming of outboard white space;
//   Validations:
//     that content comprises 7-bit ASCII characters, or
//     that content comprises valid UTF-8 code points.
//
// This function demonstrates those aspects of FastUtf8.
//
int Utf8UniseriesMultiFuncDemo(void)
{
   // Declarations of multilingual variants of "This is a UTF-8 demo."
   Uniseries sAdlam = "𞤚𞤸𞤭𞤧 𞤭𞤧 𞤢 𞤓𞤚𞤊-𞥘 𞤣𞤫𞤥𞤮";
   Uniseries sAmharic = "ይህ የ UTF-8 ማሳያ ነው።";
   Uniseries sArmenian = "Սա UTF-8 դեմո է։";
   Uniseries sBangla = "এটি UTF-8 এর একটি ডেমো।";
   Uniseries sCantonese = "呢個系UTF-8嘅演示。";
   Uniseries sCherokee = "ᎯᎠ ᎤᏣᏔ-8 ᏗᎦᏙᎤᏍᏗ ᎠᏍᎦᏯ.";
   Uniseries sDeseret = "𐐜𐐮𐑅 𐐮𐑆 𐐩 UTF-8 𐐼𐐯𐑋𐐬.";
   Uniseries sEnglish = "This is a UTF-8 demo.";
   Uniseries sGreek = "Αυτό είναι ένα demo UTF-8.";
   Uniseries sHebrew = "זהו הדגמה של UTF-8.";
   Uniseries sHindi = "यह एक UTF-8 डेमो है।";
   Uniseries sInuktitut = "ᐅᓇ UTF-8 ᑕᑯᒃᓴᐅᑎᑕᐅᔪᖅ.";
   Uniseries sJapanese = "これはUTF-8のデモです。";
   Uniseries sKlingon = "   -8 .";
   Uniseries sNepali = "यो UTF-8 को डेमो हो।";
   Uniseries sOdia = "ଏହା UTF-8 ର ଏକ ଡେମୋ ।";
   Uniseries sRunic = "ᛏᚺᛁᛊ ᛁᛊ ᚨ ᚢᛏᚠ-ᚹ ᛞᛖᛗᛟ·";
   Uniseries sRussian = "Это демонстрация UTF-8.";
   Uniseries sPashto = "دا د UTF-8 یوه نمونه ده.";
   Uniseries sPersian = "این یک دمو از UTF-8 است.";
   Uniseries sTamil = "இது UTF-8 இன் டெமோ ஆகும்.";
   Uniseries sTigrinya = "እዚ ናይ UTF-8 ዲሞ እዩ።";
   Uniseries sTelugu = "นఇది UTF-8 యొక్క డెమో.";
   Uniseries sThai = "นี่คือการสาธิตของ UTF-8";
   Uniseries sUrdu = "یہ UTF-8 کا ایک ڈیمو ہے۔";

   // Case-folding demo.
   std::cout << "Case-folding examples:"  << std::endl;

   size_t sizeFolded = Uniseries::getSizeFolded(sAdlam);
   Uniseries sAdlamFolded1 = Uniseries::getFolded(sAdlam, sizeFolded);
   Uniseries sAdlamFolded2 = "𞤼𞤸𞤭𞤧 𞤭𞤧 𞤢 𞤵𞤼𞤬-𞥘 𞤣𞤫𞤥𞤮";

   if (sAdlamFolded1 == sAdlamFolded2)
   {
      std::cout << "      Mixed case Adlam script: " << 
         sAdlam << std::endl;
      std::cout << "     Case-folded Adlam script: " << 
         sAdlamFolded1 << std::endl;
   }

   sizeFolded = Uniseries::getSizeFolded(sEnglish);
   Uniseries sEnglishFolded1 = Uniseries::getFolded(sEnglish, sizeFolded);
   Uniseries sEnglishFolded2 = "this is a utf-8 demo.";

   if (sEnglishFolded1 == sEnglishFolded2)
   {
      std::cout << "      Mixed case English text: " << 
         sEnglish << std::endl;
      std::cout << "     Case-folded English text: " << 
         sEnglishFolded1 << std::endl;
   }

   sizeFolded = Uniseries::getSizeFolded(sGreek);
   Uniseries sGreekFolded1 = Uniseries::getFolded(sGreek, sizeFolded);
   Uniseries sGreekFolded2 = "αυτό είναι ένα demo utf-8.";

   if (sGreekFolded1 == sGreekFolded2)
   {
      std::cout << "      Mixed-case Greek stichos: " << 
         sGreek << std::endl;
      std::cout << "     Case-folded Greek stichos: " << 
         sGreekFolded1 << std::endl;
   }

   std::cout << std::endl;

   // Demo of case-insensitive whole content comparison.
   std::cout << "Case-insensitive whole content comparison:"  << std::endl;
   sEnglish.setCaseSensitivity(/* bCaseSensitive = */ false);
   sGreek.setCaseInsensitive();

   if (sAdlam.caseCompare(sAdlamFolded1))
   {
      std::cout << "     " << sAdlamFolded1 << "  matches  " << 
         sAdlam << std::endl;
   }

   if (sEnglishFolded1 == "this is a utf-8 demo.")
   {
      std::cout << "     " << sEnglishFolded1 << "  matches  " << 
         "This is a UTF-8 demo." << std::endl;
   }

   if (sGreekFolded1 == sGreek)
   {
      std::cout << "     " << sEnglishFolded1 << "  matches  " << 
         sEnglish << std::endl;
   }

   std::cout << std::endl;

   // Demo of case-insensitive partial content comparison.
   std::cout << "Case-insensitive partial content comparison:"  << std::endl;

   Uniseries sMulti1 = sHindi + ", " + sRussian + ", " + 
      sPashto + ", " + sPersian + ", " + sTamil + ", " + sTigrinya;

   sMulti1.setCaseInsensitive();

   if (sMulti1.contains("это демонстрация utf-8"))
   {
      std::cout << "     This content..." << std::endl;
      std::cout << "        " << sMulti1 << std::endl;
      std::cout << "     ...contains the Russian stroka..." << std::endl;
	  std::cout << "       это демонстрация utf-8" << std::endl;
   }

   std::cout << std::endl;

   // Demo of case-sensitive and case-insensitive partial content lookup.
   std::cout << "Case-insensitive partial content lookup:"  << std::endl;
   Uniseries sMulti2 = sAmharic + ", " + sBangla + ", " + 
      sCantonese + ", " + sCherokee + ", " + sDeseret + ", " + sHebrew;
   Uniseries sMulti3 = sRunic + ", " + sInuktitut + ", " + 
      sJapanese + ", " + sKlingon + ", " + sNepali + ", " + sOdia;
   Uniseries sMulti4 = sTelugu + ", " + sThai + ", " + sUrdu;
   Uniseries sMultiCombo = sMulti1 + "\n" + sMulti2 + "\n" + sMulti3 + 
      "\n" + sMulti4;

   std::cout << "  -- Setting case sensitivity to true -- "  << std::endl;
   sMultiCombo.setCaseSensitivity(true);

   int iOdiaLain = sMultiCombo.find(sOdia);
   int iBindrune = sMultiCombo.find("ᛁᛊ");

   if (iOdiaLain > 0)
   {
      std::cout << "     " << sOdia << "  has index  " << 
         iOdiaLain << std::endl;
   }

   if (iBindrune > 0)
   {
      std::cout << "     " << "ᛁᛊ" << "  has index  " << 
         iBindrune << std::endl;
   }

   int iRussianStroka = sMultiCombo.find("это демонстрация utf-8");

   if (iRussianStroka > 0)
   {
      std::cout << "     " << "это демонстрация utf-8" << "  has index  " << 
         iRussianStroka << std::endl;
   }
   else
   {
      std::cout << 
         "     Cannot find Russian stroka / Не могу найти русскую строку" << 
         std::endl;
   }

   std::cout << "  -- Setting case sensitivity to false -- "  << std::endl;
   sMultiCombo.setCaseSensitivity(false);
   iRussianStroka = sMultiCombo.find("это демонстрация utf-8");

   if (iRussianStroka > 0)
   {
      std::cout << "     " << "это демонстрация utf-8" << "  has index  " << 
         iRussianStroka << std::endl;
   }
   else
   {
      std::cout << 
         "     Cannot find Russian stroka / Не могу найти русскую строку" << 
         std::endl;
   }

   std::cout << std::endl;

   // Matching wildcards demo.
   std::cout << "Matching wildcards:"  << std::endl;

   if (sMulti1.caseCompareWild("*ДЕМОНСТРА?ИЯ utf-8*"))
   {
      std::cout << "     This content..." << std::endl;
      std::cout << "        " << sMulti1 << std::endl;
      std::cout << "     ...matches the wildcarded and inverse-cased Cyrillic / Latin sequence..." << std::endl;
	  std::cout <<  "       *ДЕМОНСТРА?ИЯ utf-8*" << std::endl;
   }

   std::cout << std::endl;

   // Demo of content separation and trimming.
   std::cout << "Content separation and trimming:"  << std::endl;

   Uniseries sMultiComboDup = sMultiCombo;
   std::unique_ptr<FastUtf8::Uniseries> psMultiComboPart1 = 
                      sMultiComboDup.pSeparate("\n");
   std::unique_ptr<FastUtf8::Uniseries> psMultiComboPart2 = 
                      sMultiComboDup.pSeparate(" \n", /* bTrim = */ true);

   if (sAmharic == *psMultiComboPart2)
   {
      std::cout << "     Extracted this content..." << std::endl;
      std::cout << "        " << *psMultiComboPart2 << std::endl;
      std::cout << "     ... from this content..." << 
         std::endl << sMultiCombo << std::endl;
      std::cout << "     ... via two Uniseries::pSeparate() calls" << std::endl;
   }

   std::cout << std::endl;

   // Demo of 7-bit character validation and of converting 8-bit ASCII to 
   // valid UTF-8.
   std::cout << "Validations:"  << std::endl;
   char sz8BitAscii[6] = "\x80\x81\x82\xA5\xEA"; 
   Uniseries sFormer8Bit = sz8BitAscii;

   if (sEnglish.is7Bit())
   {
      std::cout << "     This content...  " << std::endl;
      std::cout << "        " << sEnglish << std::endl;
      std::cout << "     ...is 7-bit ASCII text" << std::endl;
   }

   std::cout << std::endl;

   if (sFormer8Bit.validate())
   {
      std::cout << "     This 8-bit ASCII content...  " << std::endl;
#if defined(_WIN32)
SetConsoleOutputCP(437);
#endif
      std::cout << "        " << sz8BitAscii << std::endl;
#if defined(_WIN32)
   SetConsoleOutputCP(CP_UTF8);
#endif
      std::cout << "     ...has been converted to this UTF-8 content..." << std::endl;
      std::cout << "        " << sFormer8Bit << std::endl;
   }

   std::cout << std::endl;
   return 0;
}

// Entry point for demo.
//
int main(int argc, char **argv)
{
   int  iPart = 0;

#if defined(_WIN32)
   SetConsoleOutputCP(CP_UTF8);
   SetConsoleCP(CP_UTF8);
#endif

   // Demo of functionality not provided with the std::string class, for ASCII.
   ++iPart;
   std::cout << std::endl << "Part " << iPart << std::endl;
   std::cout << 
      "FastUtf8::Uniseries features beyond std::string demo (ASCII)" << 
      std::endl << std::endl;
   AsciiUniseriesMultiFuncDemo();

   // Demo of functionality not provided with the std::string class, for UTF-8.
   ++iPart;
   std::cout << std::endl << "Part " << iPart << std::endl;
   std::cout << 
      "FastUtf8::Uniseries features beyond std::string demo (UTF-8)" << 
	  std::endl << std::endl;
   Utf8UniseriesMultiFuncDemo();
   return 0;
}
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

// Demonstrates the static FastUtf8::Uniseries methods:
//
//   getLength()      Get content length in code points.
//   getSize()        Get content size in bytes.
//   getSizeFolded()  Get content size anticipated after case folding.
//   getFolded()      Get case-folded content.
//   is7Bit()         Is the current content all 7-bit ASCII characters?
//
// The static methods are provided for ease of access without requiring a 
// Uniseries instance.
//
int UniseriesStaticMethodsDemo(void)
{
   // Declare an ASCII string and a circled Latin series.
   char szBerryQuote[88] = 
    "I don't remember when I did not know Port William, the town and the neighborhood.";
   uint8_t uzCircledLatinQuote[353] = 
    "Ⓘ ⓓⓞⓝ’ⓣ ⓡⓔⓜⓔⓜⓑⓔⓡ ⓦⓗⓔⓝ Ⓘ ⓓⓘⓓ ⓝⓞⓣ ⓚⓝⓞⓦ Ⓟⓞⓡⓣ Ⓦⓘⓛⓛⓘⓐⓜ, ⓣⓗⓔ ⓣⓞⓦⓝ ⓐⓝⓓ ⓣⓗⓔ ⓝⓔⓘⓖⓗⓑⓞⓡⓗⓞⓞⓓ.";

   // Demonstrate methods that get the length and size of each.
   std::cout << "Length and size example:"  << std::endl;

   int     lenBerry = FastUtf8::Uniseries::getLength(szBerryQuote);
   int     lenCircled = FastUtf8::Uniseries::getLength(uzCircledLatinQuote);
   size_t  sizeBerry = FastUtf8::Uniseries::getSize(szBerryQuote);
   size_t  sizeCircled = FastUtf8::Uniseries::getSize(uzCircledLatinQuote);

   std::cout << "ASCII quote:"  << std::endl << szBerryQuote << std::endl;
   std::cout << "     Length: " << lenBerry << std::endl;
   std::cout << "      Bytes: " << sizeBerry << std::endl << std::endl;
   std::cout << "In circled Latin:"  << std::endl << uzCircledLatinQuote << 
                   std::endl;
   std::cout << "     Length: " << lenCircled << std::endl;
   std::cout << "      Bytes: " << sizeCircled << std::endl << std::endl;

   // Allocate buffers sufficient for the case-folded ASCII and circled Latin 
   // content.
   size_t  sizeFoldedBerry = FastUtf8::Uniseries::getSizeFolded(szBerryQuote);
   size_t  sizeFoldedCircled = FastUtf8::Uniseries::getSizeFolded(
                   uzCircledLatinQuote);
   char    *pszFoldedBerry = 
                   reinterpret_cast<char *> (std::malloc(sizeFoldedBerry));
   uint8_t *puzFoldedCircled = 
                   reinterpret_cast<uint8_t *> (std::malloc(sizeFoldedCircled));

   // Place the case-folded ASCII and circled Latin content into the buffers.
   if (pszFoldedBerry && puzFoldedCircled)
   {
      Uniseries sFoldedBerry = FastUtf8::Uniseries::getFolded(szBerryQuote);
      Uniseries sFoldedCircled = FastUtf8::Uniseries::getFolded(
                   uzCircledLatinQuote);

      strncpy(pszFoldedBerry, 
              reinterpret_cast<char *> (sFoldedBerry.getContent()), 
              sizeFoldedBerry);
      strncpy(reinterpret_cast<char *> (puzFoldedCircled), 
              reinterpret_cast<char *> (sFoldedCircled.getContent()), 
              sizeFoldedCircled);

      // Show the case-folded content, and demonstrate 7-bit ASCII check.
      std::cout << "Case-folded ASCII quote:"  << std::endl << 
                   pszFoldedBerry << std::endl;

      if (FastUtf8::Uniseries::is7Bit(pszFoldedBerry))
      {
         std::cout << "     This is 7-bit ASCII text."  << std::endl;
      }
      else
      {
         std::cout << "     This is not 7-bit ASCII text."  << std::endl;
      }
      
      std::cout << std::endl << 
                   "Case-folded circled Latin quote:"  << std::endl << 
                   puzFoldedCircled << std::endl;

      if (FastUtf8::Uniseries::is7Bit(puzFoldedCircled))
      {
         std::cout << "     This is 7-bit ASCII text."  << std::endl;
      }
      else
      {
         std::cout << "     This is not 7-bit ASCII text."  << std::endl;
      }
      
      free(pszFoldedBerry);
      free(puzFoldedCircled);
   }
   
   return 0;
}

// Entry point for demo.
//
int main(void)
{
#if defined(_WIN32)
   SetConsoleOutputCP(CP_UTF8);
   SetConsoleCP(CP_UTF8);
#endif

   // Demo of static FastUtf8::Uniseries methods.
   std::cout << "Static FastUtf8::Uniseries methods demo" << std::endl << std::endl;
   UniseriesStaticMethodsDemo();
   return 0;
}
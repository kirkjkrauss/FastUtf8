// C++ demo for FastUtf8::Uniseries iterator.
//
// All code in this file is copyright 2026 Kirk J Krauss and is a Derivative 
// Work based on material that is copyright 2025 Kirk J Krauss and available 
// at
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
//#include <cstdio>
//#include <cstring>
//#include <string>
//#include <cmath>
//#include <chrono>
#include <sstream>
#include "fastutf8.h"
using namespace FastUtf8;

// Returns true if the query content includes Japanese code points, and false 
// if it doesn't.
bool ContainsJapanese(FastUtf8::Uniseries sQuery)
{
   bool bRetVal = false;

   for (FastUtf8::Uniseries::Iterator itr = sQuery.begin(); 
                      itr != sQuery.end(); ++itr)
   {
      uint32_t n = *itr;                       // UTF-8 code point in sQuery

      if ((n >= 0xE38180 && n <= 0xE383BF) ||  // Hiragana/Katakana range
          (n >= 0xE4B880 && n <= 0xE9BFBF) ||  // CJK unified ideographs
          (n >= 0xEFBDA5 && n <= 0xEFBE9F))    // Shift-JIS encodings
      {
         bRetVal = true;
      }
   }

   return bRetVal;
}

int UniseriesIteratorDemo(void)
{
   FastUtf8::Uniseries sEnglish = u8"Mr. Roboto says domo arigato.";
   FastUtf8::Uniseries sMixed = u8"Mr. Roboto says どうもありがとうございます。";

   if (!ContainsJapanese(sEnglish))
   {
      std::cout << std::endl << "    This content..." << std::endl << 
                      "        " << sEnglish;
      std::cout << std::endl << "    ...includes no Japanese code points." << 
                      std::endl;
   }

   if (ContainsJapanese(sMixed))
   {
      std::cout << std::endl << "    This content..." << std::endl << 
                      "        " << sMixed;
      std::cout << std::endl << "    ...includes Japanese code points." << 
                      std::endl;
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

   std::cout << "FastUtf8::Uniseries iterator demo" << std::endl;
   UniseriesIteratorDemo();
   return 0;
}
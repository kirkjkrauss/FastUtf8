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

int UniseriespFindTokenDemo(void)
{
   // Define a pair of short Japanese buns and equivalent English sentences, 
   // plus a mix of the two.
   FastUtf8::Uniseries sJapanesePart1 = u8"これがミスター・ロボットに秘密を話すとこうなるんだ。 ";
   FastUtf8::Uniseries sEnglishPart1 = 
      u8"This is what happens when you tell Mr Robot a secret. ";
   FastUtf8::Uniseries sJapanesePart2 = u8"ミスター・ロボットがどうもありがとうと言っています。 ";
   FastUtf8::Uniseries sEnglishPart2 = u8"Mr Roboto says domo arigato.";
   FastUtf8::Uniseries sMixedPart2 = u8"Mr Roboto says どうもありがとうございます。";

   // Combine uniseries in the two languages.
   FastUtf8::Uniseries sJapaneseSentences = sJapanesePart1 + sJapanesePart2;
   FastUtf8::Uniseries sEnglishSentences = sEnglishPart1 + sEnglishPart2;
   FastUtf8::Uniseries sMixedSentences = sJapanesePart1 + sMixedPart2;

   // Define a token set with both the Latin period (.) and Japanese kuten (。).
   FastUtf8::Uniseries sKutenSet = u8"。.";

   // Define a search pattern with a space followed by a wildcard.
   FastUtf8::Uniseries sSearchPattern = u8" *";

   // Extract the second portion of the combined Japanese bun.
   uint8_t *puzKuten = sJapaneseSentences.pFindToken(sKutenSet);
   uint8_t *puzFirst = sJapaneseSentences.pFindWild(sSearchPattern, &puzKuten);
   uint8_t *puzLast = sJapaneseSentences.pFindToken(puzFirst, sKutenSet);
   FastUtf8::Uniseries sJapanesePart2a(puzFirst, puzLast);

   std::cout << std::endl << "    From this combined content..." << 
                      std::endl << "        " << sJapanesePart1 << 
                      sJapanesePart2;
   std::cout << std::endl << "    ...extracted the second portion:" << 
                      std::endl <<"        " << sJapanesePart2a << std::endl;

   // Extract the second portion of the combined English sentences.
   puzKuten = sEnglishSentences.pFindToken(sKutenSet);
   puzFirst = sEnglishSentences.pFindWild(sSearchPattern, &puzKuten);
   puzLast = sEnglishSentences.pFindToken(puzFirst, sKutenSet);
   FastUtf8::Uniseries sEnglishPart2a(puzFirst, puzLast);

   std::cout << std::endl << "    From this combined content..." << 
                      std::endl << "        " << sEnglishPart1 << 
                      sEnglishPart2;
   std::cout << std::endl << "    ...extracted the second portion:" << 
                      std::endl <<"        " << sEnglishPart2a << std::endl;

   // Extract the second portion of the mixed combination.
   puzKuten = sMixedSentences.pFindToken(sKutenSet);
   puzFirst = sMixedSentences.pFindWild(sSearchPattern, &puzKuten);
   puzLast = sMixedSentences.pFindToken(puzFirst, sKutenSet);
   FastUtf8::Uniseries sMixedPart2a(puzFirst, puzLast);

   std::cout << std::endl << "    From this combined content..." << 
                      std::endl << "        " << sMixedSentences;
   std::cout << std::endl << "    ...extracted the second portion:" << 
                      std::endl << "        " << sMixedPart2a << std::endl;

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

   // Demo of search for any of a set of tokens.
   std::cout << "FastUtf8::Uniseries.pFindToken() demo" << std::endl;
   UniseriespFindTokenDemo();
   return 0;
}
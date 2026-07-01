// UTF-8-ready C routines.
//
// Copyright 2026 Kirk J Krauss.  This is a Derivative Work based on 
// material that is copyright 2025 Kirk J Krauss and available at
//
//     https://developforperformance.com/MatchingWildcardsInGoSwiftAndCpp.html
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     https://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// UTF-8 case mappings coded in this file and in the casemappings.cpp file 
// are derived from material that is copyright 2019 Unicode®, Inc. and 
// available at
//
//     https://www.unicode.org/Public/12.1.0/ucd/CaseFolding.txt
//
// The case mappings have been derived in accordance with the free and open-
// source Unicode License v3, and in accordance with the Unicode standard 
// available at
//
//     https://www.unicode.org/standard/standard.html
//
// and transformed from the original Unicode case mappings based on the UTF-8 
// Unicode Transformation Format described in connection with the Unicode 
// standard, e.g., at
//
//     https://unicode.org/faq/utf_bom
//
// or described more succinctly at
//
//     https://en.wikipedia.org/wiki/UTF-8
//
// The code for arranging the transformation comprises a Microsoft® Excel® 
// macro included in the CaseFolding-12.1.0.xls file associated with this 
// project.  The case mappings are arranged to perform simple case folding as 
// described in the "Usage" comments near the top of CaseFolding.txt.
//
#if !defined(CASEMAPPINGS_H)
#define CASEMAPPINGS_H
#if defined(_MSC_VER) && (_MSC_VER < 1600)  // Pre-C99
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef __int64            int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned __int64   uint64_t;
#else   // C99 or later
#include <stdint.h>
#endif  // C99

   // Declarations for case mapping arrays:

   // For one-byte code points: the folded byte mapping is hard-coded in 
   // casemappings.cpp.
#define CM08DataLen 0x100
   extern const uint8_t CM08[CM08DataLen];
   
   // For two-byte code points: the folded byte mapping is initialized in 
   // CaseMappingSetupUtf8().
#define CM00nDataLen 0x1F00
   extern uint16_t CM00n[CM00nDataLen];

#define CM00nLen 0x1C6
   extern const uint16_t CM00nSetup[CM00nLen][2];
   
   // For three-byte code points, an array of 32-bit values representing the 
   // 0x00Ennnnn range is initialized in CaseMappingSetupUtf8() based on the 
   // mappings that follow.
#define CMEnnDataLen 0x100000
   extern uint32_t CMEnn[CMEnnDataLen];

   // Three-byte code point mappings for the 0xE18nnn range.
#define CME18Len 0x2F
   extern const uint32_t CME18Setup[CME18Len][2];

   // Three-byte code point mappings for the 0xE1Bnnn range.
#define CME1BLen 0x116
   extern const uint32_t CME1BSetup[CME1BLen][2];
   
   // Three-byte code point mappings for the 0xE28nnn range.
#define CME28Len 0x16
   extern const uint32_t CME28Setup[CME28Len][2];
   
   // Three-byte code point mappings for the 0xE29nnn range.
#define CME29Len 0x1B
   extern const uint32_t CME29Setup[CME29Len][2];
   
   // Three-byte code point mappings for the 0xE2Bnnn range.
#define CME2BLen 0x74
   extern const uint32_t CME2BSetup[CME2BLen][2];

   // Three-byte code point mappings for the 0xEA9nnn range.
#define CMEA9Len 0x75
   extern const uint32_t CMEA9Setup[CMEA9Len][2];

   // Three-byte code point mappings for the 0xEAAnnn range.
#define CMEAALen 0x51
   extern const uint32_t CMEAASetup[CMEAALen][2];

   // Three-byte code point mappings for the 0xEFBnnn range.
#define CMEFBLen 0x1B
   extern const uint32_t CMEFBSetup[CMEFBLen][2];

   // For four-byte code points, arrays of 32-bit values representing portions 
   // of the 0xF09nnnnn range are initialized in CaseMappingSetupUtf8().
   
   // Four-byte code point mappings for the 0xF0909nnn range.
#define CM9nnDataLen 0x1000
   extern uint32_t CM909[CM9nnDataLen];
   
#define CM909Len 0x4D
   extern const uint32_t CM909Setup[CM909Len][2];

   // Four-byte code point mappings for the 0xF090Bnnn range.
   extern uint32_t CM90B[CM9nnDataLen];
   
#define CM90BLen 0x34
   extern const uint32_t CM90BSetup[CM90BLen][2];

   // Four-byte code point mappings for the 0xF091Annn range.
   extern uint32_t CM91A[CM9nnDataLen];

#define CM91ALen 0x21
   extern const uint32_t CM91ASetup[CM91ALen][2];
   
   // Four-byte code point mappings for the 0xF096Bnnn range.
   extern uint32_t CM96B[CM9nnDataLen];
   
#define CM96BLen 0x21
   extern const uint32_t CM96BSetup[CM96BLen][2];

   // Four-byte code point mappings for the 0xF09EAnnn range.
   extern uint32_t CM9EA[CM9nnDataLen];

#define CM9EALen 0x23
   extern const uint32_t CM9EASetup[CM9EALen][2];

  // This function initializes sets of mappings for case folding.  The 
  // mappings reside in the arrays declared above and are used by the 
  // code in fastutf8.cpp for case-insensitive content matching.
  //
  void CaseMappingSetupUtf8(void);

#endif  // CASEMAPPINGS_H
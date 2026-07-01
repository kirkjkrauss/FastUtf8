// C testcases for UTF-8-ready routines.
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
#if defined(_MSC_VER)
#include <windows.h>
#include <shlwapi.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strcasestr StrStrIA
#define strdup _strdup
#endif

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>    // For isspace()

#if __cplusplus < 201103L
// Timings can't be arranged via cross-platform code with the legacy C build.
#define HIGH_RES_CLOCK_NOW (NULL)
#define TIME_POINT int
#define DURATION uint64_t 
#define COUNT 
#define nullptr (0)
// This definition of POW() is not accurate, just an oversimplistic stand-in.
#define POW(base, exp) (base * exp)
#else 
#define nullptr (0)
#define HIGH_RES_CLOCK_NOW std::chrono::high_resolution_clock::now()
#define TIME_POINT std::chrono::time_point<std::chrono::high_resolution_clock>
#define DURATION std::chrono::duration_cast<std::chrono::nanoseconds>
#define COUNT .count()
#include <cmath>
#endif

#include "fastutf8.h"
#include "fastwildcompare.h"

// File-scope variables for low-latency accumulation of performance data.
//
uint64_t  g_uModeA_AccumulatedTimeAscii;
uint64_t  g_uModeB_AccumulatedTimeAscii;
uint64_t  g_uModeA_AccumulatedTimeLenAscii;
uint64_t  g_uModeB_AccumulatedTimeLenAscii;
uint64_t  g_uModeA_AccumulatedTimeUtf8;
uint64_t  g_uModeB_AccumulatedTimeUtf8;
uint64_t  g_uModeA_AccumulatedTimeLenUtf8;
uint64_t  g_uModeB_AccumulatedTimeLenUtf8;
int       g_iModeA_CallsUtf8vAscii;
int       g_iModeB_CallsUtf8vAscii;
int       g_iModeA_CallsLenUtf8vLenAscii;
int       g_iModeB_CallsLenUtf8vLenAscii;

// File-scope flag set to TRUE in main() if any argument beginning with a 
// 'w', 'p', or 'c' is passed in, or set to FALSE otherwise.
//
int      g_bComparePerformance;
int       g_iTestRepetitions;

// Flag set for those test sets that comprise ASCII strings for performance 
// comparison.
//
int      g_bAccumulateFunctionTimes;

// Value for an expected non-matching result.
size_t    g_noMatch = ~(size_t) 0;

//
// Calls to the UTF-8 routines under test are wrapped in code that compares 
// expected results against actual results.  Each wrapper returns a passing 
// result only if the call(s) get the expected outcomes.  Some of these 
// wrappers also accumulate timings, along with counts of calls to the 
// routines under test, for rollup once testing for a given set of UTF-8 
// routines is complete.
//

// Validates UTF-8 content via the two validation routines.
//
int testvalidate(uint8_t *pContent, int *piCount, int bExpectedResult)
{
   int bPassed = TRUE;

    if (bExpectedResult != ValidateUtf8(pContent, piCount))
    {
       bPassed = FALSE;
    }

    if (bExpectedResult != LenValidateUtf8(pContent, 
                                           CodePointCountUtf8(pContent)))
    {
       bPassed = FALSE;
    }

   return bPassed;
}

// Produces UTF-8 content from 8-bit ASCII text via the two conversion 
// routines.  Allocates a block to contain the converted form of any actual 
// 8-bit content.
//
int testconvert(uint8_t *pContent, uint8_t **pConvertedContentA)
{
   uint8_t *pConvertedContentB;
   int     lenContent;
   size_t  sizeA = 0;
   size_t  sizeB = 0;
   int    bPassed = TRUE;

   *pConvertedContentA = Convert8BitAsciiToUtf8((char *) pContent, &lenContent);

   if (*pConvertedContentA)
   {
      sizeA = strlen((char *) *pConvertedContentA);
   }
   else
   {
      *pConvertedContentA = pContent;
   }

   pConvertedContentB = LenConvert8BitAsciiToUtf8((char *) pContent,
                                                   strlen((char *) pContent));

   if (pConvertedContentB)
   {
      sizeB = strlen((char *) pConvertedContentB);
      free((char *) pConvertedContentB);
   }

   if (sizeA != sizeB)
   {
      bPassed = FALSE;
   }

   return bPassed;
}

// Determines whether the code point at the given index matches the expected 
// code point.
//
int testindex(uint8_t *pContent, int iIndex, uint32_t uExpectedCodePoint)
{
   int bPassed = TRUE;

   if (uExpectedCodePoint != IndexUtf8(pContent, iIndex))
   {
       bPassed = FALSE;
   }

   return bPassed;
}

// Produces case-insensitive content via the included case folding routine.
//
int testtofolded(uint8_t *pContent, uint8_t *pExpectedContent)
{
   uint8_t *pFoldedContent;
   int    bPassed = TRUE;
   size_t  sizeContentA = SizeOfFoldedUtf8(pContent);
   size_t  sizeContentB = SizeOfFoldedLenUtf8(pContent, 
                                              CodePointCountUtf8(pContent));
   
   if (sizeContentA != sizeContentB)
   {
      bPassed = FALSE;
   }
   else
   {
      pFoldedContent = (uint8_t *) malloc(sizeContentA);

      if (pFoldedContent)
      {
         pFoldedContent = ToFoldedUtf8(
                                   pFoldedContent, pContent, sizeContentA);

         if (!CompareUtf8(pFoldedContent, pExpectedContent))
         {
            bPassed = FALSE;
         }

         free(pFoldedContent);
      }
   }

   return bPassed;
}

// Determines whether UTF-8 content comprises entirely 7-bit ASCII text.
//
int testisascii(uint8_t *pContent, int lenContent, int bExpectedResult)
{
   int bPassed = TRUE;

    if (bExpectedResult != Is7BitUtf8(pContent))
    {
        bPassed = FALSE;
    }

    if (bExpectedResult != IsLen7BitUtf8(pContent, lenContent))
    {
        bPassed = FALSE;
    }

   return bPassed;
}

// Copies UTF-8 content via the four included routines for that purpose:
//
//   CopyUtf8()
//   LenCopyUtf8()
//   DuplicateUtf8()
//   LenDuplicateUtf8()
//
// Verifies that the copies match the original content.  Optionally collects 
// performance timings against equivalent ASCII-only routines from the 
// standard library.  For these timings, Mode A accumulates findings for 
// *CopyUtf8() vs. str*cpy(), and Mode B accumulates findings for 
// *DuplicateUtf8() vs. str*dup().
//
int testcopyandduplicate(uint8_t *pContent, int lenContent)
{
   // Allocate blocks for use with *CopyUtf8().
   size_t sizeContent = 1 + SizeOfLenUtf8(pContent, lenContent);
   size_t strlenContent = 1 + strlen((char *) pContent);
   uint8_t *pContentCopyTerm = (uint8_t *) malloc(sizeContent);
   uint8_t *pContentCopyLen = (uint8_t *) malloc(sizeContent);

   int bPassed = TRUE;

   // Blocks for use with str*() and *DuplicateUtf8() are allocated as needed.
   uint8_t *pContentDuplicateTerm = nullptr;
   uint8_t *pContentDuplicateLen = nullptr;
   char *pAsciiCopyTerm = nullptr;
   char *pAsciiCopyLen = nullptr;
   char *pAsciiDuplicateTerm = nullptr;
   char *pAsciiDuplicateLen = nullptr;
   TIME_POINT timeStart;
   TIME_POINT timeFinish;

   // Verify that the results of strlen() and SizeOfLenUtf8() are a match.
   if (sizeContent != strlenContent)
   {
      if (pContentCopyTerm)
      {
         free(pContentCopyTerm);
      }

      if (pContentCopyLen)
      {
         free(pContentCopyLen);
      }

      return FALSE;
   }

   // Null-terminated tests.
   if (g_bAccumulateFunctionTimes)
   {
      // Allocate blocks for use with str*cpy().
      pAsciiCopyTerm = (char *) malloc(strlenContent);
      pAsciiCopyLen = (char *) malloc(strlenContent);

      if (pAsciiCopyTerm)
      {
         // Get strcpy() timing.
         timeStart = HIGH_RES_CLOCK_NOW;
         pAsciiCopyTerm = strcpy(pAsciiCopyTerm, (char *) pContent);
         timeFinish = HIGH_RES_CLOCK_NOW;
         g_uModeA_AccumulatedTimeAscii += 
              (DURATION(timeFinish - timeStart))COUNT;
      }

      // Get strdup() timing.
      timeStart = HIGH_RES_CLOCK_NOW;
      pAsciiDuplicateTerm = strdup((char *) pContent);
      timeFinish = HIGH_RES_CLOCK_NOW;
      g_uModeB_AccumulatedTimeAscii += 
           (DURATION(timeFinish - timeStart))COUNT;
   }

   if (pContentCopyTerm)
   {
      // Get CopyUtf8() timing.
      if (g_bAccumulateFunctionTimes)
      {
         timeStart = HIGH_RES_CLOCK_NOW;
      }

      pContentCopyTerm = CopyUtf8(pContentCopyTerm, pContent);

      if (g_bAccumulateFunctionTimes)
      {
         timeFinish = HIGH_RES_CLOCK_NOW;
         g_uModeA_AccumulatedTimeUtf8 += 
            (DURATION(timeFinish - timeStart))COUNT;
         g_iModeA_CallsUtf8vAscii++;
      }
   }

   // Get DuplicateUtf8() timing.
   if (g_bAccumulateFunctionTimes)
   {
      timeStart = HIGH_RES_CLOCK_NOW;
   }

   pContentDuplicateTerm = DuplicateUtf8(pContent);

   if (g_bAccumulateFunctionTimes && pContentDuplicateTerm)
   {
      timeFinish = HIGH_RES_CLOCK_NOW;
      g_uModeB_AccumulatedTimeUtf8 += 
         (DURATION(timeFinish - timeStart))COUNT;
      g_iModeB_CallsUtf8vAscii++;

      if (!pAsciiCopyTerm || !pAsciiDuplicateTerm ||
          strcmp((char *)pContent,pAsciiCopyTerm) ||
          strcmp((char *)pContent,pAsciiDuplicateTerm))
      {
          printf("testcopyandduplicate() sanity check: mismatch with calls to \
standard library.  Expected...\n\t\t%s\n  Got...\n\t\t%s\n  And...\n\t\t%s\n",
                (char *) pContent, pAsciiCopyTerm, pAsciiDuplicateTerm);
      }
   }

   // Verify results for null-terminated tests.
   if (!pContentCopyTerm || !pContentDuplicateTerm || 
       !CompareUtf8(pContent, pContentCopyTerm) || 
       !CompareUtf8(pContent, pContentDuplicateTerm))
   {
      bPassed = FALSE;
   }

   // Length-limited tests.
   if (g_bAccumulateFunctionTimes)
   {
      // Get strncpy() timing.
      if (pAsciiCopyLen)
      {   
         timeStart = HIGH_RES_CLOCK_NOW;
         pAsciiCopyLen = strncpy(pAsciiCopyLen, (char *) pContent, 
                                 strlenContent);
         timeFinish = HIGH_RES_CLOCK_NOW;
         g_uModeA_AccumulatedTimeLenAscii += 
              (DURATION(timeFinish - timeStart))COUNT;
      }

      // Get strdup() timing.
      // (There's no MSVC strndup() as of ver. 19.44.35209.)
      timeStart = HIGH_RES_CLOCK_NOW;
      pAsciiDuplicateLen = strdup((char *) pContent);
      timeFinish = HIGH_RES_CLOCK_NOW;
      g_uModeB_AccumulatedTimeLenAscii += 
           (DURATION(timeFinish - timeStart))COUNT;
   }

   if (pContentCopyLen)
   {
      // Get LenCopyUtf8() timing.
      if (g_bAccumulateFunctionTimes)
      {
         timeStart = HIGH_RES_CLOCK_NOW;
      }

      pContentCopyLen = LenCopyUtf8(pContentCopyLen, pContent, sizeContent);

      if (g_bAccumulateFunctionTimes)
      {
         timeFinish = HIGH_RES_CLOCK_NOW;
         g_uModeA_AccumulatedTimeLenUtf8 += 
            (DURATION(timeFinish - timeStart))COUNT;
         g_iModeA_CallsLenUtf8vLenAscii++;
      }
   }

   // Get DuplicateUtf8() timing.
   if (g_bAccumulateFunctionTimes)
   {
      timeStart = HIGH_RES_CLOCK_NOW;
   }

   pContentDuplicateLen = LenDuplicateUtf8(pContent, sizeContent);

   if (g_bAccumulateFunctionTimes && pContentDuplicateTerm)
   {
      timeFinish = HIGH_RES_CLOCK_NOW;
      g_uModeB_AccumulatedTimeLenUtf8 += 
         (DURATION(timeFinish - timeStart))COUNT;
      g_iModeB_CallsLenUtf8vLenAscii++;

      if (!pAsciiCopyLen || !pAsciiDuplicateLen || 
          strcmp((char *) pContent, pAsciiCopyLen) || 
          strcmp((char *) pContent, pAsciiDuplicateLen))
      {
         printf("testcopyandduplicate() sanity check: mismatch with calls to \
standard library.  Expected...\n\t\t%s\n  Got...\n\t\t%s\n  And...\n\t\t%s\n",
                (char *) pContent, pAsciiCopyLen, pAsciiDuplicateLen);
      }
   }

   // Verify results for length-limited tests.
   if (!pContentCopyLen || !pContentDuplicateLen || 
       !LenCompareUtf8(pContent, pContentCopyLen, lenContent) || 
       !LenCompareUtf8(pContent, pContentDuplicateLen, lenContent))
   {
      bPassed = FALSE;
   }

   // Dellocate the blocks used for these tests.
   if (pAsciiCopyTerm)
   {
      free(pAsciiCopyTerm);
   }

   if (pAsciiCopyLen)
   {
      free(pAsciiCopyLen);
   }

   if (pAsciiDuplicateTerm)
   {
      free(pAsciiDuplicateTerm);
   }

   if (pAsciiDuplicateLen)
   {
      free(pAsciiDuplicateLen);
   }

   if (pContentCopyTerm)
   {
      free((char *) pContentCopyTerm);
   }

   if (pContentCopyLen)
   {
      free((char *) pContentCopyLen);
   }

   if (pContentDuplicateTerm)
   {
      free((char *) pContentDuplicateTerm);
   }

   if (pContentDuplicateLen)
   {
      free((char *) pContentDuplicateLen);
   }

   return bPassed;
}

// Separates token-delimited UTF-8 content, concatenates the separated 
// portions so as to rebuild the content with single spaces where the tokens 
// were, then slices the concatenated content (selects a substring), all via 
// the routines for those purposes:
//
//   SeparateUtf8()
//   LenSeparateUtf8()
//   ConcatenateUtf8()
//   LenConcatenateUtf8()
//   SliceUtf8()
//   LenSliceUtf8()
//
// Verifies that a selected slice of the recombined content matches a  
// passed-in parameter comprising expected content, by calling CompareUtf8() 
// or LenCompareUtf8().
//
// Optionally collects performance timings against equivalent ASCII-only 
// routines including SeparateAscii(), provided with the FastUtf8 routines, 
// and strcat() from the standard library.  For these timings, Mode A 
// accumulates findings for... 
//
//  TrimUtf8() + SeparateUtf8() 
//     vs. 
//          TrimAscii() + SeparateAscii()  [ASCII implementation]
//
// ...and Mode B accumulates findings for... 
//
//  *ConcatenateUtf8() vs. str*cat()
//
// The closest ASCII-specific equivalent for *SliceUtf8() in C/C++ is 
// strncpy(), for which performance comparisons are arranged against 
// CopyUtf8() in the above function, not in this one.
//
int testseparateconcatenateandslice(uint8_t *pContent, uint8_t *pDelimiter, 
                                     int iFirst, int iLast,  // slice indices
                                     int lenContent, uint8_t *pExpectedSlice)
{
   // Detokenized content gets rebuilt, via concatenation, into a buffer 
   // allocated up front.  For performance comparison, create buffers for 
   // both ASCII and UTF-8 content.
   size_t sizeBuf = 1 + SizeOfUtf8(pContent);
   char *pszDetokenizedAscii = g_bAccumulateFunctionTimes ? 
                          (char *) malloc(sizeBuf) : nullptr;
   uint8_t *pzDetokenizedUtf8 = (uint8_t *) malloc(sizeBuf);

   // Rather than walk the inbound tokenized content itself, walk duplicates 
   // that SeparateUtf8() and SeparateAscii() can modify.
   char *pszDuplicateAsciiBase = g_bAccumulateFunctionTimes ? 
                                     strdup((char *) pContent) : nullptr;
   char *pszDuplicateAscii = pszDuplicateAsciiBase;
   uint8_t *pzDuplicateUtf8Base = DuplicateUtf8(pContent);
   uint8_t *pzDuplicateUtf8 = pzDuplicateUtf8Base;

   // For both the ASCII-specific and and UTF-8-ready code, results of 
   // computation end up in string / content slices.
   char *pszSlice1 = (char *) malloc(1 + strlen((char *) pContent));
   char *pszSlice2 = nullptr;
   uint8_t *pzSlice = nullptr;   // Allocated via call to *SliceUtf8().
   int bPassed = TRUE;

   // These variables are needed during steps along the way.
   char    *pszAsciiPortion;
   uint8_t *pzUtf8Portion;
   size_t  sizeAscii;
   int    bTopOfContent;
   TIME_POINT timeStart;
   TIME_POINT timeFinish;

   if (!pszSlice1 || !pzDetokenizedUtf8 || !pzDuplicateUtf8)
   {
      // Memory allocation failure.
      return FALSE;
   }
   else
   {
      // Initialize the new ASCII and UTF-8 buffers.
      if (pszDetokenizedAscii)
      {
         pszDetokenizedAscii[0] = '\0';
      }

      pzDetokenizedUtf8[0] = 0;
      sizeAscii = 0;
      bTopOfContent = TRUE;

      // Concatenate tokenized content, a portion at a time, into the 
      // buffers.
      if (pszDuplicateAscii && pszDetokenizedAscii) do
      {
         // Get SeparateAscii() + TrimAscii() timing.
         timeStart = HIGH_RES_CLOCK_NOW;
         pszAsciiPortion = TrimAscii(SeparateAscii(&pszDuplicateAscii, 
                                                   (char *) pDelimiter));
         timeFinish = HIGH_RES_CLOCK_NOW;
         g_uModeA_AccumulatedTimeAscii += 
              (DURATION(timeFinish - timeStart))COUNT;

         if (pszAsciiPortion)
         {
            if (lenContent)
            {
               if (bTopOfContent)
               {
                  // Get strncat() timing.
                  timeStart = HIGH_RES_CLOCK_NOW;
                  pszDetokenizedAscii = strncat(
                      pszDetokenizedAscii, pszAsciiPortion, 
                      sizeBuf - strlen(pszDetokenizedAscii) - 1);
                  timeFinish = HIGH_RES_CLOCK_NOW;
                  g_uModeB_AccumulatedTimeLenAscii += 
                         (DURATION(timeFinish - timeStart))COUNT;
                  bTopOfContent = FALSE;
               }
               else
               {
                   // Get more strncat() timing.
                   timeStart = HIGH_RES_CLOCK_NOW;
                   pszDetokenizedAscii = strncat(
                       pszDetokenizedAscii, " ", 
                       sizeBuf - strlen(pszDetokenizedAscii) - 1);
                   pszDetokenizedAscii = strncat(
                       pszDetokenizedAscii, pszAsciiPortion,
                       sizeBuf - strlen(pszDetokenizedAscii) - 1);
                   timeFinish = HIGH_RES_CLOCK_NOW;
                   g_uModeB_AccumulatedTimeLenAscii +=
                       (DURATION(timeFinish - timeStart))COUNT;
               }
            }
            else
            {
               if (bTopOfContent)
               {
                  // Get strcat() timing.
                  timeStart = HIGH_RES_CLOCK_NOW;
                  pszDetokenizedAscii = strcat(
                                         pszDetokenizedAscii, pszAsciiPortion);
                  timeFinish = HIGH_RES_CLOCK_NOW;
                  g_uModeB_AccumulatedTimeAscii += 
                         (DURATION(timeFinish - timeStart))COUNT;
                  bTopOfContent = FALSE;
               }
               else
               {
                   // Get more strcat() timing.
                   timeStart = HIGH_RES_CLOCK_NOW;
                   pszDetokenizedAscii = strcat(
                                         pszDetokenizedAscii, " ");
                   pszDetokenizedAscii = strcat(
                                         pszDetokenizedAscii, pszAsciiPortion);
                   timeFinish = HIGH_RES_CLOCK_NOW;
                   g_uModeB_AccumulatedTimeAscii +=
                       (DURATION(timeFinish - timeStart))COUNT;
               }
            }
         }
      } while (pszDuplicateAscii);

      bTopOfContent = TRUE;

      if (pzDuplicateUtf8) do
      {
         // Get SeparateUtf8() + TrimUtf8() timing [Mode A].
         timeStart = HIGH_RES_CLOCK_NOW;
         pzUtf8Portion = TrimUtf8(SeparateUtf8(
                                      &pzDuplicateUtf8, pDelimiter));
         timeFinish = HIGH_RES_CLOCK_NOW;
         g_uModeA_AccumulatedTimeUtf8 += 
              (DURATION(timeFinish - timeStart))COUNT;
         g_iModeA_CallsUtf8vAscii++;

         if (pzUtf8Portion)
         {
            if (lenContent)
            {
               if (bTopOfContent)
               {
                  // Get LenConcatenateUtf8() timing: Mode B with length check.
                  timeStart = HIGH_RES_CLOCK_NOW;
                  pzDetokenizedUtf8 = LenConcatenateUtf8(
                                        pzDetokenizedUtf8, sizeBuf, 
                                        pzUtf8Portion,
                                        CodePointCountUtf8(pzDetokenizedUtf8),
                                        CodePointCountUtf8(pzUtf8Portion));
                  timeFinish = HIGH_RES_CLOCK_NOW;
                  g_uModeB_AccumulatedTimeLenUtf8 += 
                         (DURATION(timeFinish - timeStart))COUNT;
        
                  // Just called both strncat() and LenConcatenateUtf8().
                  g_iModeB_CallsLenUtf8vLenAscii++;
                  bTopOfContent = FALSE;
               }
               else
               {
                   // Get LenConcatenateUtf8() timing [Mode B with length check].
                   timeStart = HIGH_RES_CLOCK_NOW;
                   pzDetokenizedUtf8 = LenConcatenateUtf8(
                                        pzDetokenizedUtf8, sizeBuf,
                                        (uint8_t *) " ",
                                        CodePointCountUtf8(pzDetokenizedUtf8), 
                                        1);
                   pzDetokenizedUtf8 = LenConcatenateUtf8(
                                        pzDetokenizedUtf8, sizeBuf,
                                        pzUtf8Portion, 
                                        CodePointCountUtf8(pzDetokenizedUtf8),
                                        CodePointCountUtf8(pzUtf8Portion));
                   timeFinish = HIGH_RES_CLOCK_NOW;
                   g_uModeB_AccumulatedTimeLenUtf8 +=
                         (DURATION(timeFinish - timeStart))COUNT;
        
                   // Just called both strncat() and LenConcatenateUtf8() twice.
                   g_iModeB_CallsLenUtf8vLenAscii += 2;
               }
            }
            else
            {
               if (bTopOfContent)
               {
                  // Get ConcatenateUtf8() timing [Mode B with terminating null].
                  timeStart = HIGH_RES_CLOCK_NOW;
                  pzDetokenizedUtf8 = ConcatenateUtf8(
                                        pzDetokenizedUtf8, sizeBuf, 
                                        pzUtf8Portion);
                  timeFinish = HIGH_RES_CLOCK_NOW;
                  g_uModeB_AccumulatedTimeUtf8 += 
                         (DURATION(timeFinish - timeStart))COUNT;
        
                  // Just called both strcat() and ConcatenateUtf8().
                  g_iModeB_CallsUtf8vAscii++;
                  bTopOfContent = FALSE;
               }
               else
               {
                  // Get more ConcatenateUtf8() timing.
                  timeStart = HIGH_RES_CLOCK_NOW;
                  pzDetokenizedUtf8 = ConcatenateUtf8(
                                        pzDetokenizedUtf8, 
                                        sizeBuf, (uint8_t *) " ");
                  pzDetokenizedUtf8 = ConcatenateUtf8(
                                        pzDetokenizedUtf8, sizeBuf, 
                                        pzUtf8Portion);
                  timeFinish = HIGH_RES_CLOCK_NOW;
                  g_uModeB_AccumulatedTimeUtf8 +=
                         (DURATION(timeFinish - timeStart))COUNT;
         
                  // Just called both strcat() and ConcatenateUtf8() twice.
                  g_iModeB_CallsUtf8vAscii += 2;
               }
            }
         }
      } while (pzDuplicateUtf8);

      if (pszDuplicateAsciiBase)
      {
         free(pszDuplicateAsciiBase);
         pszDuplicateAsciiBase = nullptr;
      }
      
      if (pzDuplicateUtf8Base)
      {
         free((char *) pzDuplicateUtf8Base);
         pzDuplicateUtf8Base = nullptr;
      }
   }

   if (!pszDetokenizedAscii)
   {
      if (g_bAccumulateFunctionTimes)
      {
         // Content separation or concatenation failure.
         bPassed = (sizeAscii != 0);
      }
   }
   else
   {
      // Get a slice of each new buffer, comprising the specified number of 
      // code points.  The first code blocks flesh out an ASCII version that 
      // could serve for performance comparison, but this test presently 
      // skips any actual comparison.
      if (g_bAccumulateFunctionTimes)
      {
         if (iFirst < 0)
         {
            // The given index values are treated as offsets from the end 
            // of the string.  Get the text based on them.
            sizeAscii = strlen((char *) pszDetokenizedAscii);
            pszSlice1 = strncpy(pszSlice1, 
                        pszDetokenizedAscii + sizeAscii + (size_t) iFirst, 
                           iLast ? (size_t) (iLast - iFirst) : 
                                   (size_t) (0 - iFirst));
            pszSlice1[iLast - iFirst] = '\0';
         }
         else
         {
            // Get the text between the first and last index.
            pszSlice1 = strncpy(pszSlice1, 
                               pszDetokenizedAscii + (size_t) iFirst, 
                               (size_t) (iLast - iFirst));
            pszSlice1[iLast - iFirst] = '\0';
         }
      }

      if (strcmp((char *) pszSlice1, (char *) pExpectedSlice))
      {
         printf("testseparateconcatenateandslice() sanity check: mismatch with\
 calls to ASCII functions.  Expected...\n\t\t%s\n  Got...\n\t\t%s\n",
                (char *) pExpectedSlice, (char *) pszSlice1);
      }

      if (g_bAccumulateFunctionTimes)
      {
         // Invoke our ASCII slice routines to get the content between the first 
         // and last index.
         if (lenContent)
         {
            // Select the content via the length-limited routine.
            pszSlice2 = LenSliceAscii(
                      pszDetokenizedAscii, iFirst, iLast, lenContent);

            if (pszSlice2)
            {
               if (!LenCompareUtf8(
                      (uint8_t *) pszSlice2, pExpectedSlice, lenContent))
               {
                  // Mismatched slices.
                  printf("testseparateconcatenateandslice(): mismatch with\
 calls to built-in ASCII functions.  Expected...\n\t\t%s\n  Got...\n\t\t%s\n",
                (char *) pExpectedSlice, (char *) pszSlice2);
                  bPassed = FALSE;
               }
            }
         }
         else
         {
            // Select the content via the routine that checks for a 
            // terminating null.
            pszSlice2 = SliceAscii(pszDetokenizedAscii, iFirst, iLast);

            if (pszSlice2)
            {
               if (!CompareUtf8((uint8_t *) pszSlice2, pExpectedSlice))
               {
                  // Mismatched slices.
                  printf("testseparateconcatenateandslice(): mismatch with\
 calls to built-in ASCII functions.  Expected...\n\t\t%s\n  Got...\n\t\t%s\n",
                   (char *) pExpectedSlice, (char *) pszSlice2);
                  bPassed = FALSE;
               }
            }
         }
      }

      // Get the UTF-8 content between the first and last index.
      if (lenContent)
      {
         // Select the content via the length-limited routine.
         pzSlice = LenSliceUtf8(pzDetokenizedUtf8, iFirst, iLast, lenContent);

         if (pzSlice)
         {
            if (!LenCompareUtf8(pzSlice, pExpectedSlice, lenContent))
            {
               // Mismatched slices.
               bPassed = FALSE;
            }
         }
      }
      else
      {
         // Select the content via the routine that checks for a 
         // terminating null.
         pzSlice = SliceUtf8(pzDetokenizedUtf8, iFirst, iLast);

         if (pzSlice)
         {
            if (!CompareUtf8(pzSlice, pExpectedSlice))
            {
               // Mismatched slices.
               bPassed = FALSE;
            }
         }
      }
   }

   if (pszSlice1)
   {
      free(pszSlice1);
   }

   if (pszSlice2)
   {
      free(pszSlice2);
   }

   if (pzSlice)
   {
       free((char *) pzSlice);
   }

   if (pszDetokenizedAscii)
   {
      free(pszDetokenizedAscii);
   }

   if (pzDetokenizedUtf8)
   {
      free((char *) pzDetokenizedUtf8);
   }

   return bPassed;
}

// Compares content via each included whole-string comparison routine:
//
//   CompareUtf8()
//   CaseCompareUtf8()
//   LenCompareUtf8()
//   LenCaseCompareUtf8()
//
// Accumulates performance timimgs for these and for the similar standard 
// library routines, for ASCII input.
//
int testcompare(uint8_t *pContentA, uint8_t *pContentB, int lenContent, 
                 int bCase, int bExpectedResult)
{
   size_t nSize;           // Size of longer inbound content, in bytes
   size_t nSizeContentB;   // Size of content B
   int    iCmp;            // str*cmp() result
   int   bPassed = TRUE;
   TIME_POINT timeStart;
   TIME_POINT timeFinish;

   if (!lenContent)
   {
      if (bCase)
      {
         // Null-terminated, case-insensitive test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            iCmp = strcasecmp((char *) pContentA, (char *) pContentB);
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeAscii += 
               (DURATION(timeFinish - timeStart))COUNT;

            if ((bExpectedResult && iCmp) || (!bExpectedResult && !iCmp))
            {
               bPassed = FALSE;
            }

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         if (bExpectedResult != CaseCompareUtf8(pContentA, pContentB))
         {
            bPassed = FALSE;
         }

         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeB_CallsUtf8vAscii++;
         }
      }
      else
      {
         // Null-terminated, case-sensitive test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            iCmp = strcmp((char *) pContentA, (char *) pContentB);
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeAscii += 
                (DURATION(timeFinish - timeStart))COUNT;

            if ((bExpectedResult && iCmp) || (!bExpectedResult && !iCmp))
            {
               bPassed = FALSE;
            }

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         if (bExpectedResult != CompareUtf8(pContentA, pContentB))
         {
            bPassed = FALSE;
         }

         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;                  
         }
      }
   }
   else
   {
      if (bCase)
      {
         // Length-limited, case-insensitive test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            iCmp = strncasecmp((char *) pContentA, 
                               (char *) pContentB, 
                               (size_t) lenContent);
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenAscii += 
               (DURATION(
                                 timeFinish - timeStart))COUNT;

            if ((bExpectedResult && iCmp) || (!bExpectedResult && !iCmp))
            {
               bPassed = FALSE;
            }

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         if (bExpectedResult != LenCaseCompareUtf8(pContentA, pContentB, 
                                                   lenContent))
         {
            bPassed = FALSE;
         }

         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeB_CallsLenUtf8vLenAscii++;
         }
      }
      else
      {
         // Length-limited, case-sensitive test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            iCmp = strncmp((char *) pContentA, 
                           (char *) pContentB, 
                           (size_t) lenContent);
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeLenAscii += 
               (DURATION(timeFinish - timeStart))COUNT;
         
            if ((bExpectedResult && iCmp) || (!bExpectedResult && !iCmp))
            {
               bPassed = FALSE;
            }

            timeStart = HIGH_RES_CLOCK_NOW;
         }
         
         if (bExpectedResult != LenCompareUtf8(pContentA, pContentB, 
                                      lenContent))
         {
            bPassed = FALSE;
         }
         
         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeLenUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsLenUtf8vLenAscii++;
         }
      }
   }

   if (bPassed && lenContent)
   {
      // Size-limited tests.
      nSize = SizeOfLenUtf8(pContentA, lenContent);
      nSizeContentB = SizeOfLenUtf8(pContentB, lenContent);

      if (nSizeContentB > nSize)
      {
         nSize = nSizeContentB;
      }

      if (bCase)
      {
         if (bExpectedResult != SizeCaseCompareUtf8(pContentA, pContentB, 
                                                    nSize))
         {
            bPassed = FALSE;
         }
      }
      else
      {
         if (bExpectedResult != SizeCompareUtf8(pContentA, pContentB, 
                                                nSize))
         {
            bPassed = FALSE;
         }
      }
   }

   return bPassed;
}

// Compares content via each included substring comparison routine.
//
//   FindUtf8()
//   CaseFindUtf8()
//   LenFindUtf8()
//   LenCaseFindUtf8()
//   LenFindAscii()
//   LenCaseFindAscii()
//
// Accumulates performance timimgs for these and for the similar standard 
// library routines, for ASCII input.
//
int testfind(uint8_t *pContent, uint8_t *pPattern, 
                      int lenContent, int lenSliceContent,
                      int bCase, int iExpectedOffset)
{
   uint8_t *pSlice, *pSliceEnd;
   char    *pszSlice, *pszSliceEnd;
   int     iOffset;
   int    bPassed = TRUE;
   TIME_POINT timeStart;
   TIME_POINT timeFinish;

   if (!lenContent && !lenSliceContent)
   {
      if (bCase)
      {
         // Null-terminated, case-insensitive (Mode B) test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            pszSlice = strcasestr((char *) pContent, (char *) pPattern);
            timeFinish = HIGH_RES_CLOCK_NOW;

            g_uModeB_AccumulatedTimeAscii += 
               (DURATION(timeFinish - timeStart))COUNT;
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pSlice = CaseFindUtf8(pContent, pPattern, &pSliceEnd);

         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeB_CallsUtf8vAscii++;
         }
      }
      else
      {
         // Null-terminated, case-sensitive (Mode A) test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            pszSlice = strstr((char *) pContent, (char *) pPattern);
            timeFinish = HIGH_RES_CLOCK_NOW;

            g_uModeA_AccumulatedTimeAscii += 
               (DURATION(timeFinish - timeStart))COUNT;
            timeStart = HIGH_RES_CLOCK_NOW;
         }
         
         pSlice = FindUtf8(pContent, pPattern, &pSliceEnd);
         
         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;
         }
      }
   }
   else
   {
      if (bCase)
      {
         // Length-limited, case-insensitive (Mode B) test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            pszSlice = LenCaseFindAscii((char *) pContent, 
                      (char *) pPattern, (size_t) lenContent,
                      (size_t) lenSliceContent, &pszSliceEnd);
            timeFinish = HIGH_RES_CLOCK_NOW;

            g_uModeB_AccumulatedTimeLenAscii += 
               (DURATION(timeFinish - timeStart))COUNT;
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pSlice = LenCaseFindUtf8(pContent, pPattern, 
                      lenContent, lenSliceContent, &pSliceEnd);

         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeB_CallsLenUtf8vLenAscii++;
         }
      }
      else
      {
         // Length-limited, case-sensitive (Mode A) test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
            pszSlice = LenFindAscii((char *) pContent, 
                      (char *) pPattern, (size_t) lenContent,
                      (size_t) lenSliceContent, &pszSliceEnd);
            timeFinish = HIGH_RES_CLOCK_NOW;

            g_uModeA_AccumulatedTimeLenAscii += 
               (DURATION(timeFinish - timeStart))COUNT;
            timeStart = HIGH_RES_CLOCK_NOW;
         }
         
         pSlice = LenFindUtf8(pContent, pPattern, 
                      lenContent, lenSliceContent, &pSliceEnd);
         
         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeLenUtf8 += 
               (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsLenUtf8vLenAscii++;
         }
      }
   }

   if (pSlice)
   {
      iOffset = LenSizeOfUtf8(pContent, (size_t) (pSlice - pContent));

      if (iExpectedOffset != iOffset)
      {
         bPassed = FALSE;
      }

      if (g_bAccumulateFunctionTimes)
      {
         if ((uint8_t *) pszSlice != pSlice)
         {
            bPassed = FALSE;
         }
      }
   }
   else if (iExpectedOffset >= 0)
   {
      bPassed = FALSE;
   }
      
   return bPassed;
}

// Compares a tame/wild content pair via each included routine for matching 
// wildcards:
//
//   WildCompareUtf8()
//   WildCaseCompareUtf8()
//   WildLenCompareUtf8()
//   WildLenCaseCompareUtf8()
//
// For ASCII strings, accumulates timings for performance comparison with the 
// pre-existing FastWildCompare() routine for matching wildcards.
//
int testwildcompare(uint8_t *pTame, uint8_t *pWild, 
                      int lenTame, int lenWild,
                      int bCase, int bExpectedResult)
{
   int bPassed = TRUE;
   TIME_POINT timeStart;
   TIME_POINT timeFinish;

   if (!lenTame && !lenWild)
   {
      if (bCase)
      {
         // Null-terminated, case-insensitive test.
         if (bExpectedResult != WildCaseCompareUtf8(pWild, pTame))
         {
            bPassed = FALSE;
         }
      }
      else
      {
         // Null-terminated, case-sensitive test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;

            if (bExpectedResult != FastWildCompare(
                                         (char *) pWild, (char *) pTame))
            {
               bPassed = FALSE;
            }

            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeAscii += 
                (DURATION(timeFinish - timeStart))COUNT;
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         if (bExpectedResult != WildCompareUtf8(pWild, pTame))
         {
            bPassed = FALSE;
         }

         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;                  
         }
      }
   }
   else
   {
      if (bCase)
      {
         // Length-limited, case-insensitive test.
         if (bExpectedResult != WildLenCaseCompareUtf8(
                                      pWild, pTame, lenWild, lenTame))
         {
            bPassed = FALSE;
         }
      }
      else
      {
         // Length-limited, case-sensitive test.
         if (g_bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
         }
         
         if (bExpectedResult != WildLenCompareUtf8(
                                      pWild, pTame, lenWild, lenTame))
         {
            bPassed = FALSE;
         }
         
         if (g_bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeA_AccumulatedTimeLenUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsLenUtf8vLenAscii++;
         }
      }
   }

   return bPassed;
}

// Compares a content pair via each included routine for full-pattern-match 
// search and for targeted wildcard search.
//
// When expectedTarget == g_noMatch (-1), an exact match is expected, not a 
// wildcard match.  The test will verify results from these functions:
//
//   FindUtf8()
//   CaseFindUtf8()
//   LenFindUtf8()
//   LenCaseFindUtf8()
//   LenFindAscii()
//   LenCaseFindAscii()
// 
// When expectedMatch == g_noMatch, neither a wildcard match nor an exact 
// match is expected.
//
// When expectedMatch and expectedTarget are set to positive integers, the 
// test will verify results from these functions:
//
//   WildFindUtf8()
//   WildCaseFindUtf8()
//   WildLenFindUtf8()
//   WildLenCaseFindUtf8()
//
// Accumulates timings for performance rollups for ASCII tests.  The g_noMatch 
// values for expectedMatch and bExpectedTarget apply to content without 
// wildcards and are used to direct verification toward [Len][Case]FindUtf8() 
// rather than toward Wild[Len][Case]FindUtf8().  Performance rollups 
// nevertheless include the timings related to positive and negative findings 
// from the functions under test.
//
int testwildfind(uint8_t *pContent, uint8_t *pPattern, int lenContent, 
                  int lenPattern, size_t expectedFirst, size_t expectedLast, 
                  size_t expectedMatch, size_t expectedTarget, int bCase)
{
   uint8_t *pMatch;
   uint8_t *pTarget;
   uint8_t *pFirst;
   uint8_t *pLast;
   int bPassed = TRUE;
   int  len = CodePointCountUtf8(pContent);
   int bAscii = !LenValidateUtf8(pContent, lenContent ? lenContent : len);
   int bAccumulateFunctionTimes = bAscii ? g_bAccumulateFunctionTimes : FALSE;
   TIME_POINT timeStart;
   TIME_POINT timeFinish;

   if (!lenContent && !lenPattern)
   {
      if (bCase)
      {
         // Null-terminated, case-insensitive test.
         if (bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = CaseFindUtf8(pContent, pPattern, &pLast);

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
         }

         if (expectedTarget == g_noMatch)
         {
            if ((size_t) (pFirst - pContent) != expectedFirst ||
                (size_t) (pLast - pContent) != expectedLast)
            {
               bPassed = FALSE;
            }
         }
         else if (pFirst && len)
         {
            bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            g_uModeB_AccumulatedTimeUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
            pFirst = (uint8_t *) CaseFindAscii(
                      (char *) pContent, (char *) pPattern, (char **) &pLast);

            if (expectedTarget == g_noMatch)
            {
               if ((size_t) (pFirst - pContent) != expectedFirst ||
                   (size_t) (pLast - pContent) != expectedLast)
               {
                  bPassed = FALSE;
               }
            }
            else if (pFirst && len)
            {
               bPassed = FALSE;
            }

            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeAscii += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = pContent;
         pMatch = WildCaseFindUtf8(&pFirst, pPattern, &pLast, &pTarget);

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
         }

         if (expectedMatch != g_noMatch)
         {
             if (len &&
                 ((size_t) (pFirst - pContent) != expectedFirst ||
                  (size_t) (pLast - pContent) != expectedLast ||
                  (size_t) (pMatch - pContent) != expectedMatch ||
                  (size_t) (pTarget - pContent) != expectedTarget))
             {
                 bPassed = FALSE;
             }
         }
         else if (pMatch)
         {
             bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            g_uModeA_AccumulatedTimeUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;                  
         }
      }
      else
      {
         // Null-terminated, case-sensitive test.
         if (bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = FindUtf8(pContent, pPattern, &pLast);

         if (expectedTarget == g_noMatch)
         {
            if ((size_t) (pFirst - pContent) != expectedFirst ||
                (size_t) (pLast - pContent) != expectedLast)
            {
               bPassed = FALSE;
            }
         }
         else if (pFirst && len)
         {
            bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeAscii += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
            pFirst = (uint8_t *) strstr((char *) pContent, (char *) pPattern);

            if (expectedTarget == g_noMatch)
            {
               if ((size_t) (pFirst - pContent) != expectedFirst)
               {
                  bPassed = FALSE;
               }
            }
            else if (pFirst && len)
            {
                bPassed = FALSE;
            }

            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeAscii += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = pContent;
         pMatch = WildFindUtf8(&pFirst, pPattern, &pLast, &pTarget);

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
         }

         if (expectedMatch != g_noMatch)
         {
            if (len && 
                ((size_t) (pFirst - pContent) != expectedFirst ||
                 (size_t) (pLast - pContent) != expectedLast ||
                 (size_t) (pMatch - pContent) != expectedMatch ||
                 (size_t) (pTarget - pContent) != expectedTarget))
            {
                bPassed = FALSE;
            }
         }
         else if (pMatch)
         {
             bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            g_uModeA_AccumulatedTimeUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;                  
         }
      }
   }
   else
   {
      if (bCase)
      {
         // Length-limited, case-insensitive test.
         if (bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = LenCaseFindUtf8(
                      pContent, pPattern, lenContent, lenPattern, &pLast);

         if (expectedTarget == g_noMatch)
         {
            if ((size_t) (pFirst - pContent) != expectedFirst ||
                (size_t) (pLast - pContent) != expectedLast)
            {
                bPassed = FALSE;
            }
         }
         else if (pFirst && len)
         {
             bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            timeStart = HIGH_RES_CLOCK_NOW;
            pFirst= (uint8_t *) LenCaseFindAscii(
                      (char *) pContent, (char *) pPattern, lenContent, 
                      lenPattern, (char **) &pLast);

            if (expectedTarget == g_noMatch)
            {
               if ((size_t) (pFirst - pContent) != expectedFirst ||
                   (size_t) (pLast - pContent) != expectedLast)
               {
                   bPassed = FALSE;
               }
            }
            else if (pFirst && len)
            {
                bPassed = FALSE;
            }

            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenAscii += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = pContent;
         pMatch = WildLenCaseFindUtf8(&pFirst, pPattern, lenContent, 
                      lenPattern, &pLast, &pTarget);

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
         }

         if (expectedMatch != g_noMatch)
         {
             if (len &&
                 ((size_t) (pFirst - pContent) != expectedFirst ||
                  (size_t) (pLast - pContent) != expectedLast ||
                  (size_t) (pMatch - pContent) != expectedMatch ||
                  (size_t) (pTarget - pContent) != expectedTarget))
             {
                 bPassed = FALSE;
             }
         }
         else if (pMatch)
         {
             bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            g_uModeA_AccumulatedTimeLenUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;                  
         }
      }
      else
      {
         // Length-limited, case-sensitive test.
         if (bAccumulateFunctionTimes)
         {
            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = LenFindUtf8(
                      pContent, pPattern, lenContent, lenPattern, &pLast);

         if (expectedTarget == g_noMatch)
         {
            if ((size_t) (pFirst - pContent) != expectedFirst ||
                (size_t) (pLast - pContent) != expectedLast)
            {
                bPassed = FALSE;
            }
         }
         else if (pFirst && len)
         {
             bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
            pFirst= (uint8_t *) LenFindAscii(
                      (char *) pContent, (char *) pPattern, lenContent, 
                      lenPattern, (char **) &pLast);

            if (expectedTarget == g_noMatch)
            {
               if ((size_t) (pFirst - pContent) != expectedFirst ||
                   (size_t) (pLast - pContent) != expectedLast)
               {
                   bPassed = FALSE;
               }
            }
            else if (pFirst && len)
            {
                bPassed = FALSE;
            }

            timeFinish = HIGH_RES_CLOCK_NOW;
            g_uModeB_AccumulatedTimeLenAscii += 
                (DURATION(timeFinish - timeStart))COUNT;

            timeStart = HIGH_RES_CLOCK_NOW;
         }

         pFirst = pContent;
         pMatch = WildLenFindUtf8(&pFirst, pPattern, lenContent, lenPattern, 
                      &pLast, &pTarget);

         if (bAccumulateFunctionTimes)
         {
            timeFinish = HIGH_RES_CLOCK_NOW;
         }

         if (expectedMatch != g_noMatch)
         {
             if (len &&
                 ((size_t) (pFirst - pContent) != expectedFirst ||
                  (size_t) (pLast - pContent) != expectedLast ||
                  (size_t) (pMatch - pContent) != expectedMatch ||
                  (size_t) (pTarget - pContent) != expectedTarget))
             {
                 bPassed = FALSE;
             }
         }
         else if (pMatch)
         {
             bPassed = FALSE;
         }

         if (bAccumulateFunctionTimes)
         {
            g_uModeA_AccumulatedTimeLenUtf8 += 
                (DURATION(timeFinish - timeStart))COUNT;
            g_iModeA_CallsUtf8vAscii++;                  
         }
      }
   }

   return bPassed;
}

// Given content and a set of tokens, tests the following routines:
//
//   TokenFindUtf8()
//   TokenLenFindUtf8()
//
int testfindtoken(uint8_t *pContent, uint8_t *pTokenSet, 
                   int lenContent, int lenTokenSet, size_t expectedToken)
{
   uint8_t *pToken;
   int    bPassed = TRUE;

   if (!lenContent && !lenTokenSet)
   {
      pToken = TokenFindUtf8(pContent, pTokenSet);

      if (expectedToken == g_noMatch)
      {
         if (pToken)
         {
            bPassed = FALSE;
         }
      }
      else if ((size_t) (pToken - pContent) != expectedToken)
      {
         bPassed = FALSE;
      }
   }
   else
   {
      pToken = TokenLenFindUtf8(pContent, pTokenSet, lenContent, lenTokenSet);

      if (expectedToken == g_noMatch)
      {
         if (pToken)
         {
            bPassed = FALSE;
         }
      }
      else if ((size_t) (pToken - pContent) != expectedToken)
      {
         bPassed = FALSE;
      }
   }

   return bPassed;
}

// Tests that validate UTF-8 content and that convert 8-bit ASCII text to 
// equivalent UTF-8 content involve these functions:
//    ValidateUtf8()
//    LenValidateUtf8()
//    Convert8BitAsciiToUtf8()
//    LenConvert8BitAsciiToUtf8()
//    CompareUtf8()
//
int testset_validateandconvert(void)
{
   int iCountExtendedAscii = 0;
   const int iExpectedCountExtendedAscii = 0;  // Because it's not valid UTF-8.
   uint8_t zExtendedAscii[8] =                 // Actual array element count.
               { 0xBE, 0xF7, 0xBD, 0xAC, 0x3D, 0x7E, 0xD8, 0x00 };
   uint8_t *zConvertedExtendedAscii;

   int iCountAscii = 0;
   const int iExpectedCountAscii = 10;
   uint8_t zAscii[1 + iExpectedCountAscii] = "No problem";
   uint8_t *zConvertedAscii;

   int iCountCherokee = 0;
   const int iExpectedCountCherokee = 25;
   uint8_t zCherokee[1 + (4 * iExpectedCountCherokee)] = "ᎤᏁᏝᏅᎯ ᎤᏓᏁᏗ ᎬᏩᏂᏐᎢ ᏂᎦᏓ ᎠᏂᎷᎩ";
   uint8_t *zConvertedCherokee;

   int iCountGreek = 0;
   const int iExpectedCountGreek = 40;
   uint8_t zGreek[1 + (4 * iExpectedCountGreek)] = 
               "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία";
   uint8_t *zConvertedGreek;

   int iCountSanskrit = 0;
   const int iExpectedCountSanskrit = 35;
   uint8_t zSanskrit[1 + (4 * iExpectedCountSanskrit)] = 
               "गते गते पारगते पारसंगते बोधि स्वाहा";
   uint8_t *zConvertedSanskrit;

   // Test involving multiple=byte code points that contain bit sequences 
   // identical to thouse found in certain single-byte code points.
   int iCountTricky = 7;
   const int iExpectedCountTricky = 5;
   uint8_t zTricky[1 + (4 * iExpectedCountTricky)] = "ḪؿꜪἪꜿ";   
   uint8_t *zConvertedTricky;

   int bAllPassed = TRUE;

   // Case with extended ASCII text.
   bAllPassed &= testvalidate(/* pContent = */ zExtendedAscii, 
                              /* iCount = */ &iCountExtendedAscii, 
                              /* bExpectedResult = */ FALSE);
   
   // Cases with valid UTF-8 content.
   bAllPassed &= testvalidate(zAscii, &iCountAscii, /* bExpectedResult = */ TRUE);
   bAllPassed &= testvalidate(zCherokee, &iCountCherokee, /* bExpectedResult = */ TRUE);
   bAllPassed &= testvalidate(zGreek, &iCountGreek, /* bExpectedResult = */ TRUE);
   bAllPassed &= testvalidate(zSanskrit, &iCountSanskrit, /* bExpectedResult = */ TRUE);
   bAllPassed &= testvalidate(zTricky, &iCountTricky, /* bExpectedResult = */ TRUE);

   if (bAllPassed)
   {
      bAllPassed &= (iCountExtendedAscii == iExpectedCountExtendedAscii);
      bAllPassed &= (iCountAscii == iExpectedCountAscii);
      bAllPassed &= (iCountCherokee == iExpectedCountCherokee);
      bAllPassed &= (iCountGreek == iExpectedCountGreek);
      bAllPassed &= (iCountSanskrit == iExpectedCountSanskrit);
      bAllPassed &= (iCountTricky == iExpectedCountTricky);

      if (bAllPassed)
      {
         // Calls to testconvert() allocate the blocks returned by reference.
         bAllPassed &= testconvert(zExtendedAscii, &zConvertedExtendedAscii);
         bAllPassed &= testconvert(zAscii, &zConvertedAscii);
         bAllPassed &= testconvert(zCherokee, &zConvertedCherokee);
         bAllPassed &= testconvert(zGreek, &zConvertedGreek);
         bAllPassed &= testconvert(zSanskrit, &zConvertedSanskrit);
         bAllPassed &= testconvert(zTricky, &zConvertedTricky);

         if (bAllPassed)
         {
            bAllPassed &= testcompare(zConvertedExtendedAscii,
                            (uint8_t *) "¾÷½¬=~Ø", /* lenContent = */ 0,
                            /* bCase = */ TRUE, 
                            /* bExpectedResult = */ TRUE);
            bAllPassed &= testcompare(zConvertedAscii, zAscii, 0, TRUE,
                                      /* bExpectedResult = */ TRUE);
            bAllPassed &= testcompare(zConvertedCherokee, zCherokee, 0, TRUE,
                                      /* bExpectedResult = */ FALSE);
            bAllPassed &= testcompare(zConvertedGreek, zGreek, 0, TRUE, 
                                      /* bExpectedResult = */ FALSE);
            bAllPassed &= testcompare(zConvertedSanskrit, zSanskrit, 0, TRUE,
                                      /* bExpectedResult = */ FALSE);
            bAllPassed &= testcompare(zConvertedTricky, zTricky, 0 ,TRUE,
                                      /* bExpectedResult = */ FALSE);

            if (bAllPassed)
            {
               printf("Passed UTF-8 validation and conversion tests for null-terminated content\n");
            }
            else
            {
               printf("Failed converted 8-bit ASCII validation for null-terminated content\n");
            }
         }
         else
         {
            printf("Failed 8-bit ASCII conversion for null-terminated content\n");
         }
      }
      else
      {
         printf("Failed code point counting for null-terminated content\n");
      }
   }
   else
   {
      printf("Failed null-terminated UTF-8 validation tests\n");
   }

   if (zConvertedExtendedAscii && zConvertedExtendedAscii != zExtendedAscii)
   {
      free((char *) zConvertedExtendedAscii);
   }

   if (zConvertedAscii && zConvertedAscii != zAscii)
   {
       free((char *) zConvertedAscii);
   }

   if (zConvertedCherokee && zConvertedCherokee != zCherokee)
   {
       free((char *) zConvertedCherokee);
   }

   if (zConvertedGreek && zConvertedGreek != zGreek)
   {
       free((char *) zConvertedGreek);
   }

   if (zConvertedSanskrit && zConvertedSanskrit != zSanskrit)
   {
       free((char *) zConvertedSanskrit);
   }

   if (zConvertedTricky && zConvertedTricky != zTricky)
   {
       free((char *) zConvertedTricky);
   }

   return bAllPassed;
}

// Tests that case fold UTF-8 content, check as to whether content comprises 
// entirely ASCII text, and copy and duplicate it, involve these functions:
//    ToFoldedUtf8()
//    Is7BitUtf8()
//    IsLen7BitUtf8()
//    CopyUtf8()
//    LenCopyUtf8()
//    DuplicateUtf8()
//    LenDuplicateUtf8()
//    IndexUtf8()
//    CompareUtf8()
//    LenCompareUtf8()
//
// The first set of tests involves 7-bit ASCII strings and includes code for 
// performance comparison against C standard library functions.  Performance 
// comparisons include *CopyUtf8() v. str*cpy() (Mode A) and *DuplicateUtf8() 
// v. str*dup() (Mode B).
//
int testset_foldcopyandduplicate_ascii(void)
{
   // 7-bit ASCII tests.
   const int lenAsciiA = 13;
   uint8_t szAsciiA[1 + lenAsciiA] = "Hello, World!";
   const int lenAsciiB = 75;
   uint8_t szAsciiB[1 + lenAsciiB] =
          "missiSSippi, doWnsTream oF the biG muDDy: too thin 2 plow, 2 thIck to drInk";
   const int lenAsciiC = 67;
   uint8_t szAsciiC[1 + lenAsciiC] =
          "Wasn't that a dainty dish / To set before the king?";
   const int lenAsciiD = 38;
   uint8_t szAsciiD[1 + lenAsciiD] = "DROP PROCEDURE IF EXISTS destruct_key;";
   const int lenAsciiE = 80;
   uint8_t szAsciiE[1 + lenAsciiE] = 
         "RM PUPPIES\r\nMR PUPPIES\r\nMR NOT PUPPIES\r\nMR 2PUPPIES CMPN\r\nLIB MR PUPPIES";
   const int lenAsciiF = 29;
   uint8_t szAsciiF[1 + lenAsciiF] = "use std::convert::Infallible;";
   const int lenAsciiG = 9;
   uint8_t szAsciiG[1 + lenAsciiG] = "[] == ![]";
   const int lenAsciiH = 24;
   uint8_t szAsciiH[1 + lenAsciiH] = 
         "weer goin duhSIVILEYEzum";  // From "ygUDuh" by e.e. cummings (1944)
   const int lenAsciiI = 40;
   uint8_t szAsciiI[1 + lenAsciiI] = "yEt mORe MiXeD UpPeR anD lOwEr cAsE";
   const int lenAsciiJ = 10;
   uint8_t szAsciiJ[1 + lenAsciiJ] = "No problem";
   const int lenAsciiK = 15;
   uint8_t szAsciiK[1 + lenAsciiK] = "These go to 11.";
   int bAllPassed = TRUE;
   int nReps;

   if (g_bAccumulateFunctionTimes)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   // Verify that the 7-bit ASCII strings check out.
   bAllPassed &= testisascii(szAsciiA, lenAsciiA, TRUE);
   bAllPassed &= testisascii(szAsciiB, lenAsciiB, TRUE);
   bAllPassed &= testisascii(szAsciiC, lenAsciiC, TRUE);
   bAllPassed &= testisascii(szAsciiD, lenAsciiD, TRUE);
   bAllPassed &= testisascii(szAsciiE, lenAsciiE, TRUE);
   bAllPassed &= testisascii(szAsciiF, lenAsciiF, TRUE);
   bAllPassed &= testisascii(szAsciiG, lenAsciiG, TRUE);
   bAllPassed &= testisascii(szAsciiH, lenAsciiH, TRUE);
   bAllPassed &= testisascii(szAsciiI, lenAsciiI, TRUE);
   bAllPassed &= testisascii(szAsciiJ, lenAsciiJ, TRUE);
   bAllPassed &= testisascii(szAsciiK, lenAsciiK, TRUE);

   if (bAllPassed)
   {
      // Verify indexing for the 7-bit ASCII strings.
      bAllPassed &= testindex(szAsciiA, 7, (uint32_t) 'W');
      bAllPassed &= testindex(szAsciiB, 15, (uint32_t) 'W');
      bAllPassed &= testindex(szAsciiC, 0, (uint32_t) 'W');
      bAllPassed &= testindex(szAsciiD, 33, (uint32_t) '_');
      bAllPassed &= testindex(szAsciiE, 1, (uint32_t) 'M');
      bAllPassed &= testindex(szAsciiF, 18, (uint32_t) 'I');
      bAllPassed &= testindex(szAsciiG, 2, (uint32_t) ' ');
      bAllPassed &= testindex(szAsciiH, 0, (uint32_t) 'w');
      bAllPassed &= testindex(szAsciiI, 34, (uint32_t) 'E');
      bAllPassed &= testindex(szAsciiJ, 8, (uint32_t) 'e');
      bAllPassed &= testindex(szAsciiK, 13, (uint32_t) '1');

      if (bAllPassed)
      {
         // Verify case folding for short strings and single ASCII code points.
         bAllPassed &= testtofolded((uint8_t *) "r", (uint8_t *) "r");
         bAllPassed &= testtofolded((uint8_t *) "R", (uint8_t *) "r");
         bAllPassed &= testtofolded((uint8_t *) "aaa", (uint8_t *) "aaa");
         bAllPassed &= testtofolded((uint8_t *) "aAa", (uint8_t *) "aaa");
         bAllPassed &= testtofolded((uint8_t *) "AAA", (uint8_t *) "aaa");
         bAllPassed &= testtofolded((uint8_t *) "aaA", (uint8_t *) "aaa");
         bAllPassed &= testtofolded((uint8_t *) "Mississippi", (uint8_t *) "mississippi");
         bAllPassed &= testtofolded((uint8_t *) "ippississiM", (uint8_t *) "ippississim");
         bAllPassed &= testtofolded((uint8_t *) "IPPISSISSIM", (uint8_t *) "ippississim");
         bAllPassed &= testtofolded((uint8_t *) "_ _", (uint8_t *) "_ _");
         bAllPassed &= testtofolded((uint8_t *) szAsciiA, (uint8_t *) "hello, world!");
         bAllPassed &= testtofolded((uint8_t *) szAsciiF, (uint8_t *) "use std::convert::infallible;");

         if (bAllPassed)
         {
            // Verify copy / duplicate functionality for the 7-bit ASCII 
            // strings.
            while (nReps--)
            {
               bAllPassed &= testcopyandduplicate(szAsciiA, lenAsciiA);
               bAllPassed &= testcopyandduplicate(szAsciiB, lenAsciiB);
               bAllPassed &= testcopyandduplicate(szAsciiC, lenAsciiC);
               bAllPassed &= testcopyandduplicate(szAsciiD, lenAsciiD);
               bAllPassed &= testcopyandduplicate(szAsciiE, lenAsciiE);
               bAllPassed &= testcopyandduplicate(szAsciiF, lenAsciiF);
               bAllPassed &= testcopyandduplicate(szAsciiG, lenAsciiG);
               bAllPassed &= testcopyandduplicate(szAsciiH, lenAsciiH);
               bAllPassed &= testcopyandduplicate(szAsciiI, lenAsciiI);
               bAllPassed &= testcopyandduplicate(szAsciiJ, lenAsciiJ);
               bAllPassed &= testcopyandduplicate(szAsciiK, lenAsciiK);
            }

            if (bAllPassed)
            {
               printf("Passed fold, copy, and duplicate tests with ASCII strings\n");
            }
            else
            {
               printf("Failed copy and duplicate tests with ASCII strings\n");
            }
         }
         else
         {
            printf("Failed ASCII case folding (lowercasing) tests\n");
         }
      }
      else
      {
         printf("Failed ASCII indexing tests\n");
      }
   }
   else
   {
      printf("Failed recognition of ASCII as ASCII\n");
   }

   return bAllPassed;
}


// Internationalized tests for case-insensitive code point conversion and for 
// UTF-8 content replication functions.
//
int testset_foldcopyandduplicate_utf8(void)
{
   // A few unicameral or lowercase tests.
   const int lenCherokee = 25;
   uint8_t zCherokee[1 + (4 * lenCherokee)] = "ᎤᏁᏝᏅᎯ ᎤᏓᏁᏗ ᎬᏩᏂᏐᎢ ᏂᎦᏓ ᎠᏂᎷᎩ";
   const int lenGreek = 40;
   uint8_t zGreek[1 + (4 * lenGreek)] = 
               "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία";
   const int lenSanskrit = 35;
   uint8_t zSanskrit[1 + (4 * lenSanskrit)] = 
               "गते गते पारगते पारसंगते बोधि स्वाहा";
   const int lenJapanese = 11;
   uint8_t zJapanese[1 + (4 * lenJapanese)] = 
               "古池や蛙飛びこむ水の音";
   const int lenAramaic = 153;
   uint8_t zAramaic[1 + (4 * lenAramaic)] = 
            "הָכָא יִתְמַצֵּא מִלּוּתָא דְּסִיּוּן יַעֲקֹב מִן אַרִימַתְיָא. דִּי הוּא גִּבּוֹר וְטָהוֹר בְּרוּחָא יִמְצָא גְּלִיּוֹן קָדִישׁ בְּטוּרָא דְּאֲרָרְגָּה.";
             // Based on _Monty Python and the Holy Grail_, script by Graham 
             // Chapman, John Cleese, and Eric Idle (1975)

   // Tests with symbols / emoji.
   const int lenSymbolsA = 11;
                                               // The "Lenny Face", creator unknown
   uint8_t zSymbolsA[1 + (4 * lenSymbolsA)] = "( ͡° ͜ʖ ͡°)";
   const int lenSymbolsB = 25;
   uint8_t zSymbolsB[1 + (4 * lenSymbolsB)] = "ʚïɞ✧🦢🌿𓍊⋇𓋼𓍊.✧🍄✧.𓍊𓋼⋇𓍊🌿🦢✧ʚïɞ";
   const int lenEmoji = 1;
   uint8_t zEmoji[1 + (4 * lenEmoji)] = "🙃";

   // Test involving multiple=byte code points that contain bit sequences 
   // identical to thouse found in certain single-byte code points.
   const int lenTricky = 5;
   uint8_t zTricky[1 + (4 * lenTricky)] = "ḪؿꜪἪꜿ";

   // Test with mixed 1-, 2-, 3-, and 4-byte code points.
   const int lenMixedCodePoints = 32;
   uint8_t zMixedCodePoints[1 + (4 * lenMixedCodePoints)] = 
            "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝";

   // Tests with mixed case.
   const int lenMixedCaseGreek = 65;
   uint8_t zMixedCaseGreek[1 + (4 * lenMixedCaseGreek)] = 
            "Ἡμετέρα φύσις αἱ καθ' ἕξιν πράξεις ἐστί. Ἀριστεία οὖν ἕξις (ἐστί)";
   const int lenMixedCaseRussian = 168;
   uint8_t zMixedCaseRussian[1 + (4 * lenMixedCaseRussian)] = 
            "Хотят из мужика сделать образованного человека. Да ведь прежде всего нужно сделать его хорошим и зажиточным хозяином, а тогда уж он сам узнает всё, что ему нужно знать.";
   const int lenMixedCaseCoptic = 114;
   uint8_t zMixedCaseCoptic[1 + (4 * lenMixedCaseCoptic)] = 
            "ϩⲱⲓⲧⲉ ⲧⲉ ⲡⲱϩⲙ ⲉⲧⲟⲩⲱⲙ ⲛϩⲏⲧⲥ ⲛⲧⲡⲛⲓⲧⲉ ⲙⲡⲉⲥⲱⲧⲡ ⲛⲧⲉ ⲩ ϩⲉⲗⲉⲥ ⲛⲉⲣⲱⲙⲉ ⲧⲉ ⲡ ⲉⲡⲁⲓⲛⲟⲥ ⲛⲛⲣⲱⲙⲉ ⲉⲩⲉ ⲧⲱⲗⲉ ⲛⲧⲉ ϩⲏⲧ ⲉϣⲱⲡⲉ ⲁⲩ ⲉⲣⲁⲧⲥ.";
   const int lenMixedCaseVithkuqi = 133;
   uint8_t zMixedCaseVithkuqi[1 + (4 * lenMixedCaseVithkuqi)] = 
            " 𐕰𐖗𐖵𐖥 𐖥𐖬𐖻 𐖛𐖻 𐖧𐖟 𐖻 𐖨𐖥𐖟𐖩, 𐖥𐖮𐖠𐖵𐖻 𐖳𐖣𐖻𐖬𐖵𐖻𐖱𐖷𐖗𐖱𐖻 𐖻𐖫𐖻𐖱𐖥 𐖥𐖵 𐕰𐖱𐖵𐖻 𐖱𐖟𐖵𐖻𐖱𐖥𐖗 𐖧𐖮𐖵𐖟, 𐖐 𐖻𐖠𐖵𐖻 𐖷𐖱𐖜𐖻𐖱𐖥 𐖥𐖵, 𐖳𐖥 𐖨𐖷𐖬𐖵𐖱𐖻 𐖻𐖬𐖟𐖵𐖻 𐖜𐖻 𐖨𐖥𐖟𐖩 𐖗𐖳𐖣𐖵𐖷 𐖞 𐖜𐖟 𐖻 𐖜𐖟";
   const int lenMixedCaseGlagolitic = 80;
   uint8_t zMixedCaseGlagolitic[1 + (4 * lenMixedCaseGlagolitic)] = 
            "Ⰰⰸⱏ 1ⱁⰽⰻ 3ⰾⱑⱅⰵⱃⱏ 4ⱍⱜ 5ⰵⱄⱜ 6 6 4ⱁ 4ⱐ 8 6ⰾ 8 4 8 2 4 8ⱃ 8 9 6 4ⱐ 9 6 4 2 4 7 6 9 2";
   const int lenMixedCaseAdlam = 108;
   uint8_t zMixedCaseAdlam[1 + (4 * lenMixedCaseAdlam)] = 
            "𞤋𞤧𞥆𞤢 𞤢 𞤭𞤣𞥆𞤭 𞤢 𞤪𞤫𞤲𞤣𞤭 𞤢 𞤪𞤫𞤲𞤣𞤭𞤪𞤢𞥄𞤣𞤭𞤥𞤢, 𞤢 𞤬𞤵𞤤𞥆𞤢𞥄𞤳𞤮 𞤀𞤤𞥆𞤢𞥄𞤸 𞤴𞤭𞤥𞤣𞤫 𞤢𞤪𞤭𞤶𞤢𞤲𞤣𞤫. 𞤑𞤢𞤪𞤢𞤤𞤳𞤢𞤤 𞤽𞤢𞤤𞤢 𞤪𞤫𞤲𞤣𞤭 𞤤𞤫𞤴𞤣𞤭 𞤳𞤮 𞤢𞤣𞤢𞤥𞤢𞥄𞤶𞤮.";

   int bAllPassed = TRUE;

   // Verify that what's not 7-bit ASCII is recognized as such.
   bAllPassed &= testisascii(zCherokee, lenCherokee, FALSE);
   bAllPassed &= testisascii(zGreek, lenGreek, FALSE);
   bAllPassed &= testisascii(zSanskrit, lenSanskrit, FALSE);
   bAllPassed &= testisascii(zJapanese, lenJapanese, FALSE);
   bAllPassed &= testisascii(zAramaic, lenAramaic, FALSE);
   bAllPassed &= testisascii(zSymbolsA, lenSymbolsA, FALSE);
   bAllPassed &= testisascii(zSymbolsB, lenSymbolsB, FALSE);
   bAllPassed &= testisascii(zEmoji, lenEmoji, FALSE);
   bAllPassed &= testisascii(zTricky, lenTricky, FALSE);
   bAllPassed &= testisascii(zMixedCodePoints, lenMixedCodePoints, FALSE);
   bAllPassed &= testisascii(zMixedCaseGreek, lenMixedCaseGreek, FALSE);
   bAllPassed &= testisascii(zMixedCaseRussian, lenMixedCaseRussian, FALSE);
   bAllPassed &= testisascii(zMixedCaseCoptic, lenMixedCaseCoptic, FALSE);
   bAllPassed &= testisascii(zMixedCaseVithkuqi, lenMixedCaseVithkuqi, FALSE);
   bAllPassed &= testisascii(zMixedCaseGlagolitic, lenMixedCaseGlagolitic, FALSE);
   bAllPassed &= testisascii(zMixedCaseAdlam, lenMixedCaseAdlam, FALSE);

   if (bAllPassed)
   {
      // Verify indexing for the internationalized strings.
      bAllPassed &= testindex(zCherokee, 14, (uint32_t) 'Ꮠ');
      bAllPassed &= testindex(zGreek, 39, (uint32_t) 'α');
      bAllPassed &= testindex(zSanskrit, 4, (uint32_t) 'ग');
      bAllPassed &= testindex(zJapanese, 0, (uint32_t) '古');
      bAllPassed &= testindex(zAramaic, 62, (uint32_t) '.');
      bAllPassed &= testindex(zSymbolsA, 6, (uint32_t) 'ʖ');
      bAllPassed &= testindex(zSymbolsB, 20, (uint32_t) '🦢');
      bAllPassed &= testindex(zEmoji, 0, (uint32_t) '🙃');
      bAllPassed &= testindex(zTricky, 4, (uint32_t) 'ꜿ');
      bAllPassed &= testindex(zMixedCodePoints, 0, (uint32_t) '𓅓');
      bAllPassed &= testindex(zMixedCaseGreek, 41, (uint32_t) 'Ἀ');
      bAllPassed &= testindex(zMixedCaseRussian, 48, (uint32_t) 'Д');
      bAllPassed &= testindex(zMixedCaseCoptic, 95, (uint32_t) 'ϩ');
      bAllPassed &= testindex(zMixedCaseVithkuqi, 41, (uint32_t) '𐖱');
      bAllPassed &= testindex(zMixedCaseGlagolitic, 21, (uint32_t) '5');
      bAllPassed &= testindex(zMixedCaseAdlam, 20, (uint32_t) '𞤢');

      if (bAllPassed)
      {
         // Verify case folding for some code points from the above strings.
         bAllPassed &= testtofolded((uint8_t *) "Φ", (uint8_t *) "φ");
         bAllPassed &= testtofolded((uint8_t *) "Ж", (uint8_t *) "ж");
         bAllPassed &= testtofolded((uint8_t *) "Ⱎ", (uint8_t *) "ⱎ");
         bAllPassed &= testtofolded((uint8_t *) "ⱎ", (uint8_t *) "ⱎ");
         bAllPassed &= testtofolded((uint8_t *) "𞤒", (uint8_t *) "𞤴");
         bAllPassed &= testtofolded((uint8_t *) "𞤴", (uint8_t *) "𞤴");
         bAllPassed &= testtofolded((uint8_t *) "Ծ", (uint8_t *) "ծ");
         bAllPassed &= testtofolded((uint8_t *) "ծ", (uint8_t *) "ծ");
         bAllPassed &= testtofolded((uint8_t *) "αこ", (uint8_t *) "αこ");
         bAllPassed &= testtofolded((uint8_t *) "🦢🦢🦢🦢", (uint8_t *) "🦢🦢🦢🦢");
         bAllPassed &= testtofolded((uint8_t *) "𓅓𓅓", (uint8_t *) "𓅓𓅓");
         bAllPassed &= testtofolded((uint8_t *) "Ἀα", (uint8_t *) "ἀα"); 
         bAllPassed &= testtofolded((uint8_t *) "αἈ", (uint8_t *) "αἀ"); 
         bAllPassed &= testtofolded((uint8_t *) "ἀα", (uint8_t *) "ἀα"); 
         bAllPassed &= testtofolded((uint8_t *) "Дϩ𐖱", (uint8_t *) "дϩ𐖱"); 
         bAllPassed &= testtofolded((uint8_t *) "дϩ𐖱", (uint8_t *) "дϩ𐖱"); 
         bAllPassed &= testtofolded((uint8_t *) "𞤀 𞤢", (uint8_t *) "𞤢 𞤢"); 

         if (bAllPassed)
         {
            // Verify copy / duplicate functionality for the 
            // internationalized content.
            bAllPassed &= testcopyandduplicate(zCherokee, lenCherokee);
            bAllPassed &= testcopyandduplicate(zGreek, lenGreek);
            bAllPassed &= testcopyandduplicate(zSanskrit, lenSanskrit);
            bAllPassed &= testcopyandduplicate(zJapanese, lenJapanese);
            bAllPassed &= testcopyandduplicate(zAramaic, lenAramaic);
            bAllPassed &= testcopyandduplicate(zSymbolsA, lenSymbolsA);
            bAllPassed &= testcopyandduplicate(zSymbolsB, lenSymbolsB);
            bAllPassed &= testcopyandduplicate(zEmoji, lenEmoji);
            bAllPassed &= testcopyandduplicate(zTricky, lenTricky);
            bAllPassed &= testcopyandduplicate(zMixedCodePoints, 
                                               lenMixedCodePoints);
            bAllPassed &= testcopyandduplicate(zMixedCaseGreek, 
                                               lenMixedCaseGreek);
            bAllPassed &= testcopyandduplicate(zMixedCaseRussian, 
                                               lenMixedCaseRussian);
            bAllPassed &= testcopyandduplicate(zMixedCaseCoptic, 
                                               lenMixedCaseCoptic);
            bAllPassed &= testcopyandduplicate(zMixedCaseVithkuqi, 
                                               lenMixedCaseVithkuqi);
            bAllPassed &= testcopyandduplicate(zMixedCaseGlagolitic, 
                                               lenMixedCaseGlagolitic);
            bAllPassed &= testcopyandduplicate(zMixedCaseAdlam, 
                                               lenMixedCaseAdlam);

            if (bAllPassed)
            {
               printf("Passed fold, copy, and duplicate tests with UTF-8 content\n");
            }
            else
            {
               printf("Failed copy and duplicate tests with UTF-8 content\n");
            }
         }
         else
         {
            printf("Failed UTF-8 case folding tests\n");
         }
      }
      else
      {
         printf("Failed UTF-8 indexing tests\n");
      }
   }
    else
    {
        printf("Failed recognition of non-ASCII as non-ASCII\n");
    }

   return bAllPassed;
}

//
// Tests that concatenate, separate, tokenize, and slice UTF-8 content 
// involve these functions:
//    ConcatenateUtf8()
//    LenConcatenateUtf8()
//    SeparateUtf8()
//    LenSeparateUtf8()
//    SliceUtf8()
//    LenSliceUtf8()
//
// This first test set involves 7-bit ASCII strings and includes code for 
// performance comparison of SeparateUtf8() vs. SeparateAscii() (Mode A) and 
// *ConcatenateUtf8() vs. str*cat() (Mode B).
//
int testset_separateconcatenateandslice_ascii(void)
{
   // 7-bit ASCII tests.
   const int lenAsciiA = 45;
   uint8_t szAsciiA[1 + lenAsciiA] = 
             "what,do,we,do,with,a,comma-separated,list?";
   const int lenAsciiB = 46;
   uint8_t szAsciiB[1 + lenAsciiB] =
             "Back on Times Square, Dreaming of Times Square";
   const int lenAsciiC = 18;
   uint8_t szAsciiC[1 + lenAsciiC] =
             ";;Separate;this.;;";
   const int lenAsciiD = 55;
   uint8_t szAsciiD[1 + lenAsciiD] =
             "it's'as'easy'to'learn'as'your'Alif'Daali'Laam'Miim'Ba!";
   const int lenAsciiE = 1;
   uint8_t szAsciiE[1 + lenAsciiE] =
             "6";
   const int lenAsciiF = 1645;
   uint8_t szAsciiF[1 + lenAsciiF] = 
                      "1 Hydrogen | 2 Helium | 3 Lithium | 4 Beryllium | 5 Boron | 6\
Carbon | 7 Nitrogen | 8 Oxygen | 9 Fluorine | 10 Neon | 11 Sodium | 12 Magnesium | 13\
Aluminium | 14 Silicon | 15 Phosphorus | 16 Sulfur | 17 Chlorine | 18 Argon | 19 Pota\
ssium | 20 Calcium | 21 Scandium | 22 Titanium | 23 Vanadium | 24 Chromium | 25 Manga\
nese | 26 Iron | 27 Cobalt | 28 Nickel | 29 Copper | 30 Zinc | 31 Gallium | 32 German\
ium | 33 Arsenic | 34 Selenium | 35 Bromine | 36 Krypton | 37 Rubidium | 38 Strontium\
 | 39 Yttrium | 40 Zirconium | 41 Niobium | 42 Molybdenum | 43 Technetium | 44 Ruthen\
 ium | 45 Rhodium | 46 Palladium | 47 Silver | 48 Cadmium | 49 Indium | 50 Tin | 51 A\
 ntimony | 52 Tellurium | 53 Iodine | 54 Xenon | 55 Cesium | 56 Barium | 57 Lanthanum\
 | 58 Cerium | 59 Praseodymium | 60 Neodymium | 61 Promethium | 62 Samarium | 63 Euro\
 pium | 64 Gadolinium | 65 Terbium | 66 Dysprosium | 67 Holmium | 68 Erbium | 69 Thul\
 ium | 70 Ytterbium | 71 Lutetium | 72 Hafnium | 73 Tantalum | 74 Tungsten | 75 Rheni\
 um | 76 Osmium | 77 Iridium | 78 Platinum | 79 Gold | 80 Mercury | 81 Thallium | 82 \
 Lead | 83 Bismuth | 84 Polonium | 85 Astatine | 86 Radon | 87 Francium | 88 Radium |\
 89 Actinium | 90 Thorium | 91 Protactinium | 92 Uranium | 93 Neptunium | 94 Plutoniu\
 m | 95 Americium | 96 Curium | 97 Berkelium | 98 Californium | 99 Einsteinium | 100 \
 Fermium | 101 Mendelevium | 102 Nobelium | 103 Lawrencium | 104 Rutherfordium | 105 \
 Dubnium | 106 Seaborgium | 107 Bohrium | 108 Hassium | 109 Meitnerium | 110 Darmstad\
 tium | 111 Roentgenium | 112 Copernicium | 113 Nihonium | 114 Flerovium | 115 Moscov\
 ium | 116 Livermorium | 117 Tennessine | 118 Oganesso";
   const int lenAsciiG = 6;
   uint8_t szAsciiG[1 + lenAsciiG] =
             "BR5-49";             // It won't break down on ya.  Guaranteed.

   const int iRelyNull = 0;  // Rely on null string terminators.
   int bAllPassed = TRUE;
   int  nReps;

   if (g_bAccumulateFunctionTimes)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // what,do,we,do,with,a,comma-separated,list?
      bAllPassed &= testseparateconcatenateandslice(szAsciiA, 
           /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ 21, /* iLast = */ 41, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "comma-separated list");
      bAllPassed &= testseparateconcatenateandslice(szAsciiA, 
           /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ 21, /* iLast = */ 41, 
           /* lenContent = */ 42, 
           /* pExpectedSlice = */ (uint8_t *) "comma-separated list");

      // Back on Times Square, Dreaming of Times Square
      bAllPassed &= testseparateconcatenateandslice(szAsciiB, 
           /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ 0, /* iLast = */ 4, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "Back");
      bAllPassed &= testseparateconcatenateandslice(szAsciiB, 
           /* pDelimiter = */ (uint8_t *) " ", /* iFirst = */ 0, /* iLast = */ 4, 
           /* lenContent = */ 46, /* pExpectedSlice = */ (uint8_t *) "Back");
      bAllPassed &= testseparateconcatenateandslice(szAsciiB, 
           /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ -6, /* iLast = */ 0, 
           /* lenContent = */ 46, /* pExpectedSlice = */ (uint8_t *) "Square");
      bAllPassed &= testseparateconcatenateandslice(szAsciiB, 
           /* pDelimiter = */ (uint8_t *) " ", /* iFirst = */ -6, /* iLast = */ 0, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "Square");

      // ;;Separate;this.;;
      bAllPassed &= testseparateconcatenateandslice(szAsciiC, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ 11, /* iLast = */ 15, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "this");
      bAllPassed &= testseparateconcatenateandslice(szAsciiC, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ -6, /* iLast = */ -2, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "this");
      bAllPassed &= testseparateconcatenateandslice(szAsciiC, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ -6, /* iLast = */ -2, 
           /* lenContent = */ 19, /* pExpectedSlice = */ (uint8_t *) "this");
      bAllPassed &= testseparateconcatenateandslice(szAsciiC, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ 11, /* iLast = */ 15, 
           /* lenContent = */ 18, /* pExpectedSlice = */ (uint8_t *) "this");
      bAllPassed &= testseparateconcatenateandslice(szAsciiC, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ -6, /* iLast = */ -2, 
           /* lenContent = */ 17, /* pExpectedSlice = */ (uint8_t *) "this");

      // it's'as'easy'to'learn'as'your'Alif'Daali'Laam'Miim'Ba!
      bAllPassed &= testseparateconcatenateandslice(szAsciiD, 
           /* pDelimiter = */ (uint8_t *) "\'", /* iFirst = */ 3, /* iLast = */ 4, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "s");
      bAllPassed &= testseparateconcatenateandslice(szAsciiD, 
           /* pDelimiter = */ (uint8_t *) "\'", /* iFirst = */ -51, /* iLast = */ -50, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "s");
      bAllPassed &= testseparateconcatenateandslice(szAsciiD, 
           /* pDelimiter = */ (uint8_t *) "\'", /* iFirst = */ 3, /* iLast = */ 4, 
           /* lenContent = */ 54, /* pExpectedSlice = */ (uint8_t *) "s");
      bAllPassed &= testseparateconcatenateandslice(szAsciiD, 
           /* pDelimiter = */ (uint8_t *) "\'", /* iFirst = */ -51, /* iLast = */ -50, 
           /* lenContent = */ 54, /* pExpectedSlice = */ (uint8_t *) "s");
      bAllPassed &= testseparateconcatenateandslice(szAsciiD, 
           /* pDelimiter = */ (uint8_t *) "\'", /* iFirst = */ 3, /* iLast = */ 7, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "s as");
      bAllPassed &= testseparateconcatenateandslice(szAsciiD, 
           /* pDelimiter = */ (uint8_t *) "\'", /* iFirst = */ -51, /* iLast = */ -47, 
           /* lenContent = */ 54, /* pExpectedSlice = */ (uint8_t *) "s as");

      // 6
      bAllPassed &= testseparateconcatenateandslice(szAsciiE, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ 0, /* iLast = */ 1, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "6");
      bAllPassed &= testseparateconcatenateandslice(szAsciiE, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ -1, /* iLast = */ 0, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "6");
      bAllPassed &= testseparateconcatenateandslice(szAsciiE, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ 0, /* iLast = */ 1, 
           /* lenContent = */ 1, /* pExpectedSlice = */ (uint8_t *) "6");
      bAllPassed &= testseparateconcatenateandslice(szAsciiE, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ -1, /* iLast = */ 0, 
           /* lenContent = */ 1, /* pExpectedSlice = */ (uint8_t *) "6");
        
// 1 Hydrogen | 2 Helium | 3 Lithium | 4 Beryllium | 5 Boron | 6 Carbon | 7 Nitrogen | 8 Oxygen | 9 Fluorine | 10 Neon | 11 Sodium | 12 Magnesium | 13 Aluminium | 14 Silicon | 15 Phosphorus | 16 Sulfur | 17 Chlorine | 18 Argon | 19 Potassium | 20 Calcium | 21 Scandium | 22 Titanium | 23 Vanadium | 24 Chromium | 25 Manganese | 26 Iron | 27 Cobalt | 28 Nickel | 29 Copper | 30 Zinc | 31 Gallium | 32 Germanium | 33 Arsenic | 34 Selenium | 35 Bromine | 36 Krypton | 37 Rubidium | 38 Strontium | 39 Yttrium | 40 Zirconium | 41 Niobium | 42 Molybdenum | 43 Technetium | 44 Ruthenium | 45 Rhodium | 46 Palladium | 47 Silver | 48 Cadmium | 49 Indium | 50 Tin | 51 Antimony | 52 Tellurium | 53 Iodine | 54 Xenon | 55 Cesium | 56 Barium | 57 Lanthanum | 58 Cerium | 59 Praseodymium | 60 Neodymium | 61 Promethium | 62 Samarium | 63 Europium | 64 Gadolinium | 65 Terbium | 66 Dysprosium | 67 Holmium | 68 Erbium | 69 Thulium | 70 Ytterbium | 71 Lutetium | 72 Hafnium | 73 Tantalum | 74 Tungsten | 75 Rhenium | 76 Osmium | 77 Iridium | 78 Platinum | 79 Gold | 80 Mercury | 81 Thallium | 82 Lead | 83 Bismuth | 84 Polonium | 85 Astatine | 86 Radon | 87 Francium | 88 Radium | 89 Actinium | 90 Thorium | 91 Protactinium | 92 Uranium | 93 Neptunium | 94 Plutonium | 95 Americium | 96 Curium | 97 Berkelium | 98 Californium | 99 Einsteinium | 100 Fermium | 101 Mendelevium | 102 Nobelium | 103 Lawrencium | 104 Rutherfordium | 105 Dubnium | 106 Seaborgium | 107 Bohrium | 108 Hassium | 109 Meitnerium | 110 Darmstadtium | 111 Roentgenium | 112 Copernicium | 113 Nihonium | 114 Flerovium | 115 Moscovium | 116 Livermorium | 117 Tennessine | 118 Oganesso
      bAllPassed &= testseparateconcatenateandslice(szAsciiF, 
           /* pDelimiter = */ (uint8_t *) "|", /* iFirst = */ 1084, /* iLast = */ 1090, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "Curium");
      bAllPassed &= testseparateconcatenateandslice(szAsciiF, 
           /* pDelimiter = */ (uint8_t *) "|", /* iFirst = */ -327, /* iLast = */ -321, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "Curium");
      bAllPassed &= testseparateconcatenateandslice(szAsciiF, 
           /* pDelimiter = */ (uint8_t *) "|", /* iFirst = */ 1084, /* iLast = */ 1090, 
           /* lenContent = */ 1636, /* pExpectedSlice = */ (uint8_t *) "Curium");
      bAllPassed &= testseparateconcatenateandslice(szAsciiF, 
           /* pDelimiter = */ (uint8_t *) "|", /* iFirst = */ -327, /* iLast = */ -321, 
           /* lenContent = */ 1636, /* pExpectedSlice = */ (uint8_t *) "Curium");

      // BR5-49
      bAllPassed &= testseparateconcatenateandslice(szAsciiG, 
           /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ 0, /* iLast = */ 6, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "BR5-49");
      bAllPassed &= testseparateconcatenateandslice(szAsciiG, 
           /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ -6, /* iLast = */ 0, 
           /* lenContent = */ iRelyNull, 
           /* pExpectedSlice = */ (uint8_t *) "BR5-49");
      bAllPassed &= testseparateconcatenateandslice(szAsciiG, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ 0, /* iLast = */ 6, 
           /* lenContent = */ 6, /* pExpectedSlice = */ (uint8_t *) "BR5-49");
      bAllPassed &= testseparateconcatenateandslice(szAsciiG, 
           /* pDelimiter = */ (uint8_t *) ";", /* iFirst = */ -6, /* iLast = */ 0, 
           /* lenContent = */ 6, /* pExpectedSlice = */ (uint8_t *) "BR5-49");
   }

   if (bAllPassed)
   {
      printf("Passed separate, concatenate, and slice tests with ASCII strings\n");
   }
   else
   {
      printf("Failed separate, concatenate, and slice tests with ASCII strings\n");
   }

   return bAllPassed;
}

// Internationalized tests for content separation, concatenation, and slicing.
//
int testset_separateconcatenateandslice_utf8(void)
{
   const int lenThai = 340;
   uint8_t zThai[1 + (4 * lenThai)] = 
        "ในทางพุทธศาสนา ผู้ตื่นรู้ , คือผู้ที่หลุดพ้นจากวัฏสงสารและกิเลสทั้งปวง ทำให้จิตใจดำรงอยู่ใ\
       นความบริสุทธิ์และอิสระสูงสุด การปรับเปลี่ยนจักรวาล , ของผู้ตื่นรู้ไม่ได้หมายถึงการเป็นพระเจ้\
       าผู้สร้าง , แต่หมายถึงการที่จิตที่มีอภิญญาและปัญญาญาณสามารถเห็นความจริงแท้ของสรรพ\
       สิ่ง และสามารถ แปรสภาพ ความจริง , หรือโครงสร้างจิตวิญญาณเพื่อช่วยสรรพสัตว์ได้";  // sep: ,
   const int lenSumerian = 23;
   uint8_t zSumerian[1 + (4 * lenSumerian)] = "من يضحك أخيرا يضحك كثير";  // sep: 'ث'
   const int lenPhoenician = 21;
   uint8_t zPhoenician[1 + (4 * lenPhoenician)] = "𐤖 𐤟 𐤚 𐤟 𐤛 𐤟 𐤗 𐤟 𐤘 𐤟 𐤙";  // sep: 𐤟
   const int lenLatinWithRunes = 42;
   uint8_t zLatinWithRunes[1 + (4 * lenLatinWithRunes)] = 
        "what᛫do᛫we᛫do᛫with᛫a᛫runic-separated᛫list?";  // sep: ᛫
   const int lenArmenianWithStars = 26;
   uint8_t zArmenianWithStars[1 + (4 * lenArmenianWithStars)] = "Կաթ ✶ հաց ✶ պանիր ✶ ձու";  // sep: ✶
   const int lenOsageWithCoptic = 19;
   uint8_t zOsageWithCoptic[1 + (4 * lenOsageWithCoptic)] = "𐓘𐒰𐓆𐒻𐓘𐒰𐓆𐒻⳿𐒼𐒰𐓄𐒷⳿𐓄𐒰𐓇𐒻";  // sep: ⳿
   const int lenElementsA = 26;
   uint8_t zElementsA[1 + (4 * lenElementsA)] = "𞥞Γῆ🜃흙𞥞Ἀήρ🜁공기𞥞Πῦρ🜂불𞥞Ὕδωρ🜄물𞥞";  // sep: 𞥞
   const int lenElementsB = 896;
   uint8_t zElementsB[1 + (4 * lenElementsB)] = 
                             "Водород჻Гелий჻Литий჻჻Бериллий჻Бор჻Углерод჻Азот჻\
Кислород჻Фтор჻Неон჻Натрий჻Магний჻Алюминий჻Кремний჻Фосфор჻Сера჻Хлор჻Аргон჻Калий჻\
Кальций჻Скандий჻Титан჻Ванадий჻Хром჻Марганец჻Железо჻Кобальт჻Никель჻Медь჻Цинк჻\
Галлий჻Германий჻Мышьяк჻Селен჻Бром჻Криптон჻Рубидий჻Стронций჻Иттрий჻Цирконий჻Ниобий჻\
Молибден჻Технеций჻Рутений჻Родий჻Палладий჻Серебро჻Кадмий჻Индий჻Олово჻Сурьма჻Теллур჻\
Йод჻Ксенон჻Цезий჻Барий჻Лантан჻Церий჻Празеодим჻Неодим჻Прометий჻Самарий჻Европий჻\
Гадолиний჻Тербий჻Диспрозий჻Гольмий჻Эрбий჻Тулий჻Иттербий჻Лютеций჻Гафний჻Тантал჻\
Вольфрам჻Рений჻Осмий჻Иридий჻Платина჻Золото჻Ртуть჻Таллий჻Свинец჻Висмут჻Полоний჻\
Астат჻Радон჻Франций჻Радий჻Актиний჻Торий჻Протактиний჻Уран჻Нептуний჻Плутоний჻\
Америций჻Кюрий჻Берклий჻Калифорний჻Эйнштейний჻Фермий჻Менделеевий჻Нобелий჻Лоуренсий჻\
Резерфордий჻Дубний჻Сиборгий჻Борий჻Хассий჻Мейтнерий჻Дармштадтий჻Рентгений჻\
Коперниций჻Нихоний჻Флеровий჻Московий჻Ливерморий჻Теннессин჻Оганесон";  // sep: ჻
   const int iRelyNull = 0;  // Rely on null string terminators.
   int bAllPassed = TRUE;

// ในทางพุทธศาสนา ผู้ตื่นรู้ , คือผู้ที่หลุดพ้นจากวัฏสงสารและกิเลสทั้งปวง ทำให้จิตใจดำรงอยู่ในความบริสุทธิ์และอิสระสูงสุด การปรับเปลี่ยนจักรวาล , ของผู้ตื่นรู้ไม่ได้หมายถึงการเป็นพระเจ้าผู้สร้าง , แต่หมายถึงการที่จิตที่มีอภิญญาและปัญญาญาณสามารถเห็นความจริงแท้ของสรรพสิ่ง และสามารถ แปรสภาพ ความจริง , หรือโครงสร้างจิตวิญญาณเพื่อช่วยสรรพสัตว์ได้
   bAllPassed &= testseparateconcatenateandslice(zThai, 
        /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ 23, /* iLast = */ 34, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "รู้, คือผู้ที่");
   bAllPassed &= testseparateconcatenateandslice(zThai, 
        /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ -318, /* iLast = */ -306, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "รู้, คือผู้ที่");
   bAllPassed &= testseparateconcatenateandslice(zThai, 
        /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ 23, /* iLast = */ 34, 
        /* lenContent = */ 340, /* pExpectedSlice = */ (uint8_t *) "รู้, คือผู้ที่");
   bAllPassed &= testseparateconcatenateandslice(zThai, 
        /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ -318, /* iLast = */ -306, 
        /* lenContent = */ 340, /* pExpectedSlice = */ (uint8_t *) "รู้, คือผู้ที่");

   // من يضحك أخيرا يضحك كثير
   bAllPassed &= testseparateconcatenateandslice(zSumerian, 
        /* pDelimiter = */ (uint8_t *) "ث", /* iFirst = */ 21, /* iLast = */ 23, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "ير");
   bAllPassed &= testseparateconcatenateandslice(zSumerian, 
        /* pDelimiter = */ (uint8_t *) "ث", /* iFirst = */ -2, /* iLast = */ 0, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "ير");
   bAllPassed &= testseparateconcatenateandslice(zSumerian, 
        /* pDelimiter = */ (uint8_t *) "ث", /* iFirst = */ 21, /* iLast = */ 23, 
        /* lenContent = */ 23, /* pExpectedSlice = */ (uint8_t *) "ير");
   bAllPassed &= testseparateconcatenateandslice(zSumerian, 
        /* pDelimiter = */ (uint8_t *) "ث", /* iFirst = */ -2, /* iLast = */ 0, 
        /* lenContent = */ 23, /* pExpectedSlice = */ (uint8_t *) "ير");

   // 𐤖 𐤟 𐤚 𐤟 𐤛 𐤟 𐤗 𐤟 𐤘 𐤟 𐤙
   bAllPassed &= testseparateconcatenateandslice(zPhoenician, 
        /* pDelimiter = */ (uint8_t *) "𐤟", /* iFirst = */ 2, /* iLast = */ 3, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "𐤚");
   bAllPassed &= testseparateconcatenateandslice(zPhoenician, 
        /* pDelimiter = */ (uint8_t *) "𐤟", /* iFirst = */ -9, /* iLast = */ -8, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "𐤚");
   bAllPassed &= testseparateconcatenateandslice(zPhoenician, 
        /* pDelimiter = */ (uint8_t *) "𐤟", /* iFirst = */ 2, /* iLast = */ 3, 
        /* lenContent = */ 21, /* pExpectedSlice = */ (uint8_t *) "𐤚");
   bAllPassed &= testseparateconcatenateandslice(zPhoenician, 
        /* pDelimiter = */ (uint8_t *) "𐤟", /* iFirst = */ -9, /* iLast = */ -8, 
        /* lenContent = */ 21, /* pExpectedSlice = */ (uint8_t *) "𐤚");

   // what᛫do᛫we᛫do᛫with᛫a᛫runic-separated᛫list?
   bAllPassed &= testseparateconcatenateandslice(zLatinWithRunes, 
        /* pDelimiter = */ (uint8_t *) "᛫", /* iFirst = */ 27, /* iLast = */ 41, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "runic-separated list");
   bAllPassed &= testseparateconcatenateandslice(zLatinWithRunes, 
        /* pDelimiter = */ (uint8_t *) "᛫", /* iFirst = */ 27, /* iLast = */ 41, 
        /* lenContent = */ 42, 
        /* pExpectedSlice = */ (uint8_t *) "runic-separated list");

   // Կաթ ✶ հաց ✶ պանիր ✶ ձու
   bAllPassed &= testseparateconcatenateandslice(zArmenianWithStars, 
        /* pDelimiter = */ (uint8_t *) "✶", /* iFirst = */ 0, /* iLast = */ 3, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "Կաթ");
   bAllPassed &= testseparateconcatenateandslice(zArmenianWithStars, 
        /* pDelimiter = */ (uint8_t *) "✶", /* iFirst = */ -17, /* iLast = */ -14, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "Կաթ");
   bAllPassed &= testseparateconcatenateandslice(zArmenianWithStars, 
        /* pDelimiter = */ (uint8_t *) "✶", /* iFirst = */ 0, /* iLast = */ 3, 
        /* lenContent = */ 23, /* pExpectedSlice = */ (uint8_t *) "Կաթ");
   bAllPassed &= testseparateconcatenateandslice(zArmenianWithStars, 
        /* pDelimiter = */ (uint8_t *) ",", /* iFirst = */ -17, /* iLast = */ -14, 
        /* lenContent = */ 23, /* pExpectedSlice = */ (uint8_t *) "Կաթ");

   // 𐓘𐒰𐓆𐒻𐓘𐒰𐓆𐒻⳿𐒼𐒰𐓄𐒷⳿𐓄𐒰𐓇𐒻
   bAllPassed &= testseparateconcatenateandslice(zOsageWithCoptic, 
        /* pDelimiter = */ (uint8_t *) "⳿", /* iFirst = */ 9, /* iLast = */ 13, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "𐒼𐒰𐓄𐒷");
   bAllPassed &= testseparateconcatenateandslice(zOsageWithCoptic, 
        /* pDelimiter = */ (uint8_t *) "⳿", /* iFirst = */ -9, /* iLast = */ -5, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "𐒼𐒰𐓄𐒷");
   bAllPassed &= testseparateconcatenateandslice(zOsageWithCoptic, 
        /* pDelimiter = */ (uint8_t *) "⳿", /* iFirst = */ 9, /* iLast = */ 13, 
        /* lenContent = */ 18, /* pExpectedSlice = */ (uint8_t *) "𐒼𐒰𐓄𐒷");
   bAllPassed &= testseparateconcatenateandslice(zOsageWithCoptic, 
        /* pDelimiter = */ (uint8_t *) "⳿", /* iFirst = */ -9, /* iLast = */ -5, 
        /* lenContent = */ 18, /* pExpectedSlice = */ (uint8_t *) "𐒼𐒰𐓄𐒷");

   // 𞥞Γῆ🜃흙𞥞Ἀήρ🜁공기𞥞Πῦρ🜂불𞥞Ὕδωρ🜄물𞥞
   bAllPassed &= testseparateconcatenateandslice(zElementsA, 
        /* pDelimiter = */ (uint8_t *) "𞥞", /* iFirst = */ 22, /* iLast = */ 23, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "🜄");
   bAllPassed &= testseparateconcatenateandslice(zElementsA, 
        /* pDelimiter = */ (uint8_t *) "𞥞", /* iFirst = */ -2, /* iLast = */ -1, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "🜄");
   bAllPassed &= testseparateconcatenateandslice(zElementsA, 
        /* pDelimiter = */ (uint8_t *) "𞥞", /* iFirst = */ 22, /* iLast = */ 23, 
        /* lenContent = */ 26, /* pExpectedSlice = */ (uint8_t *) "🜄");
   bAllPassed &= testseparateconcatenateandslice(zElementsA, 
        /* pDelimiter = */ (uint8_t *) "𞥞", /* iFirst = */ -2, /* iLast = */ -1, 
        /* lenContent = */ 26, /* pExpectedSlice = */ (uint8_t *) "🜄");

//  Водород჻Гелий჻Литий჻჻Бериллий჻Бор჻Углерод჻Азот჻Кислород჻Фтор჻Неон჻Натрий჻Магний჻Алюминий჻Кремний჻Фосфор჻Сера჻Хлор჻Аргон჻Калий჻Кальций჻Скандий჻Титан჻Ванадий჻Хром჻Марганец჻Железо჻Кобальт჻Никель჻Медь჻Цинк჻Галлий჻Германий჻Мышьяк჻Селен჻Бром჻Криптон჻Рубидий჻Стронций჻Иттрий჻Цирконий჻Ниобий჻Молибден჻Технеций჻Рутений჻Родий჻Палладий჻Серебро჻Кадмий჻Индий჻Олово჻Сурьма჻Теллур჻Йод჻Ксенон჻Цезий჻Барий჻Лантан჻Церий჻Празеодим჻Неодим჻Прометий჻Самарий჻Европий჻Гадолиний჻Тербий჻Диспрозий჻Гольмий჻Эрбий჻Тулий჻Иттербий჻Лютеций჻Гафний჻Тантал჻Вольфрам჻Рений჻Осмий჻Иридий჻Платина჻Золото჻Ртуть჻Таллий჻Свинец჻Висмут჻Полоний჻Астат჻Радон჻Франций჻Радий჻Актиний჻Торий჻Протактиний჻Уран჻Нептуний჻Плутоний჻Америций჻Кюрий჻Берклий჻Калифорний჻Эйнштейний჻Фермий჻Менделеевий჻Нобелий჻Лоуренсий჻Резерфордий჻Дубний჻Сиборгий჻Борий჻Хассий჻Мейтнерий჻Дармштадтий჻Рентгений჻Коперниций჻Нихоний჻Флеровий჻Московий჻Ливерморий჻Теннессин჻Оганесон
   bAllPassed &= testseparateconcatenateandslice(zElementsB, 
        /* pDelimiter = */ (uint8_t *) "჻", /* iFirst = */ 684, /* iLast = */ 689, 
        /* lenContent = */ iRelyNull, 
        /* pExpectedSlice = */ (uint8_t *) "Кюрий");
   bAllPassed &= testseparateconcatenateandslice(zElementsB, 
        /* pDelimiter = */ (uint8_t *) "჻", /* iFirst = */ 8, /* iLast = */ 13, 
        /* lenContent = */ 896, /* pExpectedSlice = */ (uint8_t *) "Гелий");
   bAllPassed &= testseparateconcatenateandslice(zElementsB, 
        /* pDelimiter = */ (uint8_t *) "჻", /* iFirst = */ -212, /* iLast = */ -207, 
        /* lenContent = */ 896, /* pExpectedSlice = */ (uint8_t *) "Кюрий");

   if (bAllPassed)
   {
      printf("Passed separate, concatenate, and slice tests with UTF-8 content\n");
   }
   else
   {
      printf("Failed separate, concatenate, and slice tests with UTF-8 content\n");
   }

   return bAllPassed;
}

// Whole-content comparison tests involve these functions:
//    CompareUtf8()
//    CaseCompareUtf8()
//    LenCompareUtf8()
//    LenCaseCompareUtf8()
//    SizeCompareUtf8()
//    SizeCaseCompareUtf8()
//    SizeOfLenUtf8()
//    CodePointCountUtf8()
//
// This first set of whole-content comparison tests involves 7-bit ASCII 
// string pairs.
//
int testset_compare_ascii(void)
{
   int  nReps;
   int bCase;
   int  iRelyNull = 0;       // Rely on null string terminators.
   int bAllPassed = TRUE;

   if (g_bAccumulateFunctionTimes)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

    while (nReps--)
    {
      bCase = FALSE;  // For case-sensitive ASCII tests.

      bAllPassed &= testcompare(
         (uint8_t *) "Hi", (uint8_t *) "Hi", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Hi", (uint8_t *) "Hi*", iRelyNull, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "Oh, the monkeys have no tails in zamboanga", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);

      // Length-limited case-sensitive ASCII tests.
      bAllPassed &= testcompare(
         (uint8_t *) "Hi", (uint8_t *) "Hi", (int) strlen("Hi"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Hi", (uint8_t *) "Hi*", (int) strlen("Hi*"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Hi", (uint8_t *) "Hi*", (int) strlen("Hi"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (int) strlen("Oh, the monkeys have no tails in Zamboanga"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (int) strlen("Oh, the monkeys have no tails"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "Oh, the monkeys have no tails in zamboanga", 
         (int) strlen("Oh, the monkeys have no tails in zamboanga"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         (int) strlen("they were bitten off by whales"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         (int) strlen("they were oh, the monkeys have no tails, off. by. whales."), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         (int) strlen("oh, the monkeys have no tails"), 
         bCase, /* bExpectedResult = */ FALSE);

      // Case with last character mismatch.
      bAllPassed &= testcompare(
         (uint8_t *) "abc", (uint8_t *) "abd", iRelyNull, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abc", (uint8_t *) "abd", (int) strlen("abd"), 
         bCase, /* bExpectedResult = */ FALSE);

      // Cases with repeating character sequences.
      bAllPassed &= testcompare(
         (uint8_t *) "abcccd", (uint8_t *) "abcccd", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abcccd", (uint8_t *) "abcccd", (int) strlen("abcccd"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "mississipisippi", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "mississipisippi", 
         (int) strlen("mississipisippi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyfffff", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyfffff", 
         (int) strlen("xxxxzzzzzzzzyfffff"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf", 
         (int) strlen("xxxxzzzzzzzzyf"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzy.fffff", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzy.fffff", 
         (int) strlen("xxxxzzy.fffff"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf", 
         (int) strlen("xxxxzzzzzzzzyf"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "xyxyxyzyxyz", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "xyxyxyzyxyz", 
         (int) strlen("xyxyxyzyxyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississippi", (uint8_t *) "mississippi", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississippi", (uint8_t *) "mississippi", 
         (int) strlen("mississippi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         (int) strlen("xyxyxyxyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "m ississippi", (uint8_t *) "m ississippi", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "m ississippi", (uint8_t *) "m ississippi", 
         1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "ababac", (uint8_t *) "ababac?", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "ababac", (uint8_t *) "ababac?", 
         1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "dababac", (uint8_t *) "ababac", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "dababac", (uint8_t *) "ababac", 
         1, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "aaazz", (uint8_t *) "aaazz", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aaazz", (uint8_t *) "aaazz", 
         2, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "1212", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "1212", 
         3, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         4, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b12", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b12", 
         (int) strlen("xyxyxyxyz") + 2, bCase, /* bExpectedResult = */ TRUE);

      // A mix of testcases
      bAllPassed &= testcompare(
         (uint8_t *) "n", (uint8_t *) "n", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "n", (uint8_t *) "n", (int) strlen("n"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aabab", (uint8_t *) "aabab", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aabab", (uint8_t *) "aabab", 
         (int) strlen("aabab") - 1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "ar", (uint8_t *) "ar", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "ar", (uint8_t *) "ar", iRelyNull, 
         (int) strlen("ar") + 1, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aar", (uint8_t *) "aaar", iRelyNull, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "aar", (uint8_t *) "aaar", 3, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XYXYXYZYXYz", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XYXYXYZYXYz", 
         3, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "missisSIPpi", (uint8_t *) "missisSIPpi", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "missisSIPpi", (uint8_t *) "missisSIPpi", 
         3, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississipPI", (uint8_t *) "mississipPI", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississipPI", (uint8_t *) "mississipPI", 
         3, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         (int) strlen("xyxyxyxyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsissippi", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsissippi", 
         (int) strlen("miSsissippi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsisSippi", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsisSippi", 
         (int) strlen("miSsissippi"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         (int) strlen("miSsissippi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         (int) strlen("abAbac"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         (int) strlen("bLaH"), bCase, /* bExpectedResult = */ FALSE);
      
      bCase = TRUE;  // For case-insensitive tests.

      bAllPassed &= testcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsisSippi", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsisSippi", 
         (int) strlen("miSsisSippi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         (int) strlen("abAbac"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         (int) strlen("abAbac") + 1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         4, bCase, /* bExpectedResult = */ TRUE);
      
      bAllPassed &= testcompare(
         (uint8_t *) "aAazz", (uint8_t *) "aAazz", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aAazz", (uint8_t *) "aAazz", 
         (int) strlen("aAazz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "A12b12", (uint8_t *) "A12b123", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "A12b12", (uint8_t *) "A12b123", 
         (int) strlen("A12b123"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12B12", (uint8_t *) "a12B12", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12B12", (uint8_t *) "a12B12", 
         (int) strlen("a12B12"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "oWn", (uint8_t *) "oWn", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "oWn", (uint8_t *) "oWn", 
         (int) strlen("oWn"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "oWn", (uint8_t *) "oWn", 
         1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLah", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLah", 
         (int) strlen("bLah"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         (int) strlen("bLaH"), bCase, /* bExpectedResult = */ TRUE);

        // Longish string scenarios.
      bAllPassed &= testcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (uint8_t *)  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 100, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajaxalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", iRelyNull, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *)  "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajaxalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 99, 
          bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaggggagaaaaaaaab", iRelyNull, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *)  "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaggggagaaaaaaaab", 127, 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", iRelyNull, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 127, 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "aaabbaabbaab", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "aaabbaabbaab", 
         (int) strlen("aaabbaabbaab"), bCase, /* bExpectedResult = */ TRUE);         
      bAllPassed &= testcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare((uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare((uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (int) strlen("aaaaaaaaaaaaaaaaa"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare((uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare((uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (int) strlen("aaaaaaaaaaaaaaaaa"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare((uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare((uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (int) strlen("aaaaaaaaaaaaaaaaa"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc", 
         (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia", 
         (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghia"), 
         bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghikjlmn", 
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefkhi", 
         (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghiab"), 
         bCase, /* bExpectedResult = */ FALSE);

      // For length-limited testcases, the bevahavior of the standard library 
	  // and the FastUtf8 functions may have a small difference.  For example, 
      // typical strncmp() documentation suggests that the function compares 
	  // characters one by one and stops if it finds a mismatch, reaches the 
	  // end of a string (null character), or has compared n characters.  
	  // Testing with the Microsoft implementation shows that reaching the end 
	  // of a string doesn't stop the comparison.  The tests in this little 
	  // section leave out performance comparison against calls to standard 
	  // library functions, whose results might differ from what's expected 
	  // here for the FastUtf8 functions.
      if (g_bAccumulateFunctionTimes)
      {
         g_bAccumulateFunctionTimes = FALSE;

         bAllPassed &= testcompare(
            (uint8_t *) "a12b12", (uint8_t *) "a12b", 5, 
             /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
         bAllPassed &= testcompare(
            (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
            (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia", 
            (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghiab"), 
            /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
         bAllPassed &= testcompare(
            (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
            (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia", 
            (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghiAb"), 
            /* bCase= */ TRUE, /* bExpectedResult = */ TRUE);

         g_bAccumulateFunctionTimes = TRUE;
      }
   }

    if (bAllPassed)
    {
        printf("Passed whole content comparison tests\n");
    }
    else
    {
        printf("Failed whole content comparison tests\n");
    }

   return bAllPassed;
}

// A set of whole-content comparison tests involving empty vs. ascii strings.
//
int testset_compare_empty(void)
{
   int bCase = FALSE;       // Start out with case-sensitive tests.
   int  iRelyNull = 0;       // Rely on null string terminators.
   int bAllPassed = TRUE;
   int  nReps;

   if (g_bAccumulateFunctionTimes)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // Two simple empty input cases illustrate the main behavioral 
      // difference between CompareUtf8() and LenCompareUtf8().  Length-
      // limited comparison stops at the first null in either string.
      // Regarding the special casing around g_bAccumulateFunctionTimes, see 
      // the commment toward the end of the above function.
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "abd", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "abd", 
         (int) strlen("abd"), bCase, /* bExpectedResult = */ TRUE);

      // Empty input cases with repeating character sequences.
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "abcccd", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "abcccd", (int) strlen("abcccd"), bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "mississipisippi", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "mississipisippi", 3, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyfffff", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyfffff", 300, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 1, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzy.fffff", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzy.fffff", 1, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 2, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyzyxyz", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyzyxyz", 2, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "mississippi", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "mississippi", 
         (int) strlen("mississippi"), bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 1, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "m ississippi", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "m ississippi", 2, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "ababac*", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "ababac*", (int) strlen("ababac*"), bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "ababac*", (int) strlen("ababac"), bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "ababac", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "ababac", (int) strlen("ababac"), bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "aaazz", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "aaazz", 5, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "1212", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "1212", 4, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "a12b", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "a12b", 3, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "a12b12", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "a12b12", 2, bCase, 
         /* bExpectedResult = */ TRUE);

      // A mix of empty input testcases.
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "n", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "n", 1, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "aabab", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "aabab", 1, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "ar", iRelyNull, bCase, 
         /* bExpectedResult = */  FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "ar", 1, bCase, 
         /* bExpectedResult = */  TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "aaar", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "aaar", 4, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "XYXYXYZYXYz", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "XYXYXYZYXYz", 
         (int) strlen("XYXYXYZYXYz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "missisSIPpi", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "missisSIPpi", 
         (int) strlen("missisSIPpi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "mississipPI", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "mississipPI", 
         (int) strlen("missisSIPpi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
         (int) strlen("xyxyxyxyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "miSsissippi", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "miSsisSippi", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "miSsisSippi", 
         2, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "abAbac", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "abAbac", 
         (int) strlen("abAbac"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "aAazz", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "aAazz", 
         (int) strlen("aAazz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "A12b123", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "A12b123", 
         (int) strlen("a12B12"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "a12B12", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "a12B12", 
         (int) strlen("a12B12"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "oWn", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "oWn", 
         (int) strlen("oWn"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "bLah", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "bLah", 
         (int) strlen("bLah"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "bLaH", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);

      // Both strings empty.
      bAllPassed &= testcompare(
         (uint8_t *) "", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "", (uint8_t *) "", 1, bCase, 
         /* bExpectedResult = */ TRUE);

      // Another simple scenario.
      bAllPassed &= testcompare(
         (uint8_t *) "abc", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "abc", (uint8_t *) "", 3, bCase, 
         /* bExpectedResult = */ TRUE);

      // More empty input cases with repeating character sequences.
      bAllPassed &= testcompare(
         (uint8_t *) "abcccd", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "abcccd", (uint8_t *) "", 
         (int) strlen("abcccd"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "", 
         (int) strlen("mississipisippi"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         (int) strlen("xxxxzzzzzzzzyf"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         2, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         3, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &=g_bAccumulateFunctionTimes ? TRUE :  testcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         4, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "", 
         5, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "mississippi", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "m ississippi", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "ababac", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "ababac", (uint8_t *) "", 
         (int) strlen("ababac"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "dababac", (uint8_t *) "", 
         iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "dababac", (uint8_t *) "", 
         (int) strlen("ababac"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "aaazz", (uint8_t *) "", 
       iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "aaazz", (uint8_t *) "", 
         (int) strlen("aaazz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "", 
         iRelyNull, g_bAccumulateFunctionTimes ? TRUE : bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "", 
         (int) strlen("a12b12"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= g_bAccumulateFunctionTimes ? TRUE : testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "", 
         1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12b12", (uint8_t *) "", 
         0, bCase, /* bExpectedResult = */ FALSE);

      // Another mix of empty input testcases.
      bAllPassed &= testcompare(
         (uint8_t *) "aAazz", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "A12b12", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "a12B12", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "oWn", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "bLah", (uint8_t *) "", iRelyNull, bCase, 
         /* bExpectedResult = */ FALSE);
   }

   if (bAllPassed)
   {
      printf("Passed whole content comparison tests with empty input\n");
   }
   else
   {
      printf("Failed whole content comparison tests with empty input\n");
   }

   return bAllPassed;
}

// Correctness tests for case-sensitive and case-insensitive UTF-8-enabled 
// routines for whole content comparison.
//
int testset_compare_utf8(void)
{
   int len = 0;             // Rely on null string terminators.
   int bAllPassed = TRUE;

   do
   {
      // Tests with Asian code points.
      bAllPassed &= testcompare(
         (uint8_t *) "三十輻，共一轂，當其無，有車之用。埏埴以為器，當其無，有器之用。鑿戶牖以為室，當其無，有室之用。故有之以為利，無之以為用", 
         (uint8_t *) "三十根车辐尽管彼此不相连接, 却可以形成车毂, 而有车。一扇门窗, 也是利用了“有”与“无”的转换而起到了保暖和通风的作用。", 
          /* lenContent = */ !len ? len : 52, 
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "三十輻，共一轂，當其無，有車之用。埏埴以為器，當其無，有器之用。鑿戶牖以為室，當其無，有室之用。故有之以為利，無之以為用", 
         (uint8_t *) "三十輻，共一轂，當其無，有車之用。埏埴以為器，當其無，有器之用。鑿戶牖以為室，當其無，有室之用。故有之以為利，無之以為用", 
          !len ? len : 52, TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "古池や蛙飛びこむ水の音", 
         (uint8_t *) "古池や蛙飛びこむ水の音", 
          !len ? len : CodePointCountUtf8((uint8_t *) "古池や蛙飛びこむ水の音"), TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "露の世は露の世ながらさりながら", 
         (uint8_t *) "露の世は露の世ながらさりながら", 
          !len ? len : CodePointCountUtf8((uint8_t *) "露の世は露の世ながらさりながら"), FALSE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "古池や蛙飛びこむ水の音", 
         (uint8_t *) "閑かさや岩にしみ入る蝉の声", 
          !len ? len : CodePointCountUtf8((uint8_t *) "古池や蛙飛びこむ水の音"), TRUE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "ᠪᠢ ᠬᠠᠭᠠᠨ ᠴᠤ ᠶᠠᠪᠤᠳᠠᠭ ᠦᠭᠡᠢ", 
         (uint8_t *) "ᠪᠢ ᠬᠠᠭᠠᠨ ᠴᠤ ᠶᠠᠪᠤᠳᠠᠭ ᠦᠭᠡᠢ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "사뿐히 즈려 밟고 가시옵소서"), FALSE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "사뿐히 즈려 밟고 가시옵소서", 
         (uint8_t *) "사뿐히 즈려 밟고 가시옵소서", 
          !len ? len : CodePointCountUtf8((uint8_t *) "사뿐히 즈려 밟고 가시옵소서"), TRUE, TRUE);
          
      // Positive and negative mixed-case comparisons.
      bAllPassed &= testcompare(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "đàn chim trắng​​ ôi nhớ về đây, các con đi đâu, ôi đàn chim trắng bay về mau mau, ôi đàn chim trắng bay về mau mau.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "사뿐히 즈려 밟고 가시옵소서"), 
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "Đàn Chim Trắng​​ Ôi Nhớ Về Đây, Các Con Đi Đâu, Ôi Đàn Chim Trắng Bay Về Mau Mau, Ôi Đàn Chim Trắng Bay Về Mau Mau.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau."), 
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "đàn chim trắng​​ ôi nhớ về đây, các con đi đâu, ôi đàn chim trắng bay về mau mau, ôi đàn chim trắng bay về mau mau.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau."),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "đàn chim trắng​​ ôi nhớ về đây, các con đi đâu, ôi đàn chim trắng bay về mau mau, ôi đàn chim trắng bay về mau mau.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "đàn chim trắng​​ ôi nhớ về đây, các con đi đâu, ôi đàn chim trắng bay ve mau mau, ôi đàn chim trắng bay về mau mau.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau."),
          TRUE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία", 
         (uint8_t *) "ΤΕΘΝΆΚΗΝ Δ' ὈΛΊΓΩ 'ΠΙΔΕΎΗΣ ΦΑΊΝΟΜ' ἈΛΑΊΑ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία"),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία", 
         (uint8_t *) "ΤΕΘΝΆΚΗΝ Δ' ὈΛΊΓΩ 'ΠΙΔΕΎΗΣ ΦΑΊΝΟΜ' ἈΛΑΊΑ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          FALSE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "рельсы, рельсы. шпалы, шпалы. едет поезд запоздалый. из последнего окошка вдруг посыпались горошки. вышли куры поклевали. вышли гуси пощипали.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "рельсы, рельсы. шпалы, шпалы. едет поезд запоздалый. из последнего окошка вдруг посыпались горошки. вышли куры поклевали. вышли гуси пощипали.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "рельсы, рельсы. шпалы, шпалы. едет поезд запоздалый. и3 последнего окошка вдруг посыпались горошки. вышли куры поклевали. вышли гуси пощипали.", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. И3 последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          TRUE, FALSE);

      // Tests with emoji.
      bAllPassed &= testcompare(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍🥳👦🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍🥳👦🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          TRUE, FALSE);

      // Tests with circled Latin code points.
      bAllPassed &= testcompare(
         (uint8_t *) "ⒶⒷⒸⒶ", 
         (uint8_t *) "ⓐⓑⓒⓩ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ⒶⒷⒸ"),
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "ⒶⒷⒸⒶ", 
         (uint8_t *) "ⓐⓑⓒⓩ", 
          !len ? 3 : CodePointCountUtf8((uint8_t *) "ⒶⒷⒸ"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "ⒶⒷⒸⒶ", 
         (uint8_t *) "ⓐⓑⓒⓩ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ⓐⓑⓒⓩ"),
          TRUE, FALSE);

      // Mixed-case Armenian.
      bAllPassed &= testcompare(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "ԵՍ ԻՄ ԱՆՈՒՇ ՀԱՅԱՍՏԱՆԻ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "ԵՍ ԻՄ ԱՆՈՒՇ ՀԱՅԱՍՏԱՆԻ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          FALSE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "Ես իմ անուշ Հաjաստանի", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          FALSE, FALSE);

      // A bit of Georgian.
      bAllPassed &= testcompare(
         (uint8_t *) "ვმოგზაურობ", 
         (uint8_t *) "არსად არ ვმოგზაურობ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "არსად არ ვმოგზაურობ"),
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "ვმოგზაურობ", 
         (uint8_t *) "არსად არ ვმოგზაურობ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "არსად არ ვმოგზაურობ"),
          /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);

      // Tests with Middle Eastern unicameral scripts.
      bAllPassed &= testcompare(
         (uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها", 
         (uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها"),
          /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها", 
         (uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எமலு்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡"),
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எமலு்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡"),
          /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥", 
         (uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥"),
          /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥", 
         (uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥"),
          /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "মই একেবাৰে ভ্ৰমণ নকৰো", 
         (uint8_t *) "মই ক'তো ভ্ৰমণ নকৰো", 
          !len ? len : 6,
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "মই একেবাৰে ভ্ৰমণ নকৰো", 
         (uint8_t *) "মই ক'তো ভ্ৰমণ নকৰো", 
          !len ? len : 6,
          /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);    
      bAllPassed &= testcompare(
         (uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟", 
         (uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟", 
           !len ? len : CodePointCountUtf8((uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟"),
          /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟", 
         (uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟"),
          /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "מי שנקי מחטא, שיהיה הראשון שישליך אבן", 
         (uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה☥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה☥"),
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "מי שנקי מחטא, שיהיה הראשון שישליך אבן", 
         (uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה☥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה☥"),
          /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);

      // A snippet from the Rök Runestone inscription.
      bAllPassed &= testcompare(
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ"),
          /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ"),
          /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);

      // Tests that include 4-byte code point sequences.
      bAllPassed &= testcompare(
         (uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈", 
         (uint8_t *) "𑢹𑣗 𑢩𑣗 𑣋𑣉𑣜𑣋𑣗𑣉𑣜𑣋𑣉𑣜", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈"),
          /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈", 
         (uint8_t *) "𑢹𑣗 𑢩𑣗 𑣋𑣉𑣜𑣋𑣗𑣉𑣜𑣋𑣉𑣜", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈"),
          TRUE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
         (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
         (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "Obɛri Ɔkaimɛ", 
         (uint8_t *) "OBƐRI ƆKAIMƐ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Obɛri Ɔkaimɛ"),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "Obɛri Ɔkaimɛ", 
         (uint8_t *) "OBƐRI ƆKAIMƐ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Obɛri Ɔkaimɛ"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐲𐑄𐐲𐑉𐑆", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐲𐑄𐐲𐑉𐑆", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
          TRUE, TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨r𐐼 𐐲𐑄𐐲𐑉𐑆", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉d 𐐊𐑄𐐲𐑉𐑆"),
          TRUE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤", 
         (uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "𞤀𞤁𞤂𞤃", 
         (uint8_t *) "𞤢𞤣𞤤𞤥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𞤢𞤣𞤤𞤥"),
          TRUE, TRUE);

      // Tests with mixed 1-, 2-, 3-, and 4-byte code points.
      bAllPassed &= testcompare(
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝", 
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝"),
          /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testcompare(
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝", 
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝"),
          TRUE, TRUE);          
      bAllPassed &= testcompare(
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝", 
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝"),
          FALSE, FALSE);
      bAllPassed &= testcompare(
         (uint8_t *) "⚡a⨄𓅓ß𐑄^⚜️ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀⌬ꙮ🍓🢇ʤʡ🦄௵ ☪☮🕉✡☤☯✝", 
         (uint8_t *) "⚡a⨄𓅓ß𐑄^⚜️ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽ੴ𓇀⌬ꙮ🍓🢇ʤʡ🦄௵ ☪☮🕉✡☤☯✝", 
          !len ? len : CodePointCountUtf8((uint8_t *) "⚡a⨄𓅓ß𐑄^⚜️ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀⌬ꙮ🍓🢇ʤʡ🦄௵ ☪☮🕉✡☤☯✝"),
          TRUE, FALSE);          
   } while (!len++);

   return bAllPassed;
}

//
// Partial-content comparison tests involve these functions:
//    FindUtf8()
//    LenFindUtf8()
//    CaseFindUtf8()
//    LenCaseFindUtf8()
//    LenSizeOfUtf8()
//    CodePointCountUtf8()
//
// This first set of partial-content comparison tests involves 7-bit ASCII 
// string pairs.
//
int testset_find_ascii(void)
{
   const int iNotFound = -1;       // A mismatch gives us a negative offset.
   const int iFoundAtFront = 0;    // The strings begin with a match.
   const int iRelyNull = 0;        // Rely on null string terminators.

   int  nReps;
   int bCase;
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

    while (nReps--)
    {
      bCase = FALSE;  // Case-sensitive ASCII tests.

      bAllPassed &= testfind(
         (uint8_t *) "Hi", (uint8_t *) "Hi", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "Hi", (uint8_t *) "Hi*", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "no tails in Zamboanga", 
         iRelyNull, iRelyNull, bCase, 21);
      bAllPassed &= testfind(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "Oh, the monkeys have no tails in zamboanga", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         iRelyNull, iRelyNull, bCase, iNotFound);

      // Length-limited case-sensitive ASCII tests.
      bAllPassed &= testfind(
         (uint8_t *) "Hi", (uint8_t *) "Hi", 
         (int) strlen("Hi"), (int) strlen("Hi"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "Hi", (uint8_t *) "Hi*", 
         (int) strlen("Hi*"), (int) strlen("Hi"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "Hi*", (uint8_t *) "i*", 
         (int) strlen("Hi*"), (int) strlen("i*"), bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "no tails in Zamboanga", 
         (int) strlen("Oh, the monkeys have no tails in Zamboanga"), 
         (int) strlen("no tails in Zamboanga"), bCase, 21);
      bAllPassed &= testfind(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "no tails in Zamboanga", 
         (int) strlen("Oh, the monkeys have no tails"), 
         (int) strlen("no tails in Zamboanga"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Oh, the monkeys have no tails in Zamboanga", 
         (uint8_t *) "no tails in zamboanga", 
         (int) strlen("Oh, the monkeys have no tails in zamboanga"), 
         (int) strlen("no tails in zamboanga"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         (int) strlen("they were bitten off by whales"), 
         (int) strlen("oh, the monkeys have no tails"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         (int) strlen("they were oh, the monkeys have no tails, off. by. whales."), 
         (int) strlen("they were oh, the monkeys have no tails, off. by. whales."), 
         bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "they were bitten off by whales", 
         (uint8_t *) "oh, the monkeys have no tails", 
         (int) strlen("oh, the monkeys have no tails"), 
         (int) strlen("they were bitten off by whales"), bCase, iNotFound);

      // Case with last character mismatch.
      bAllPassed &= testfind(
         (uint8_t *) "abc", (uint8_t *) "abd", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abc", (uint8_t *) "abd", 
         (int) strlen("abc"), (int) strlen("abd"), bCase, iNotFound);

      // Cases with repeating character sequences.
      bAllPassed &= testfind(
         (uint8_t *) "abcccd", (uint8_t *) "abcccd", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abcccd", (uint8_t *) "abcccd", 
         (int) strlen("abcccd"), (int) strlen("abcccd"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "mississipisippi", (uint8_t *) "sip", 
         iRelyNull, iRelyNull, bCase, 6);
      bAllPassed &= testfind(
         (uint8_t *) "mississipisippi", (uint8_t *) "sip", 
         (int) strlen("mississipisippi"), (int) strlen("sip"), bCase, 6);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "zyfffff", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "zyfffff", 
         (int) strlen("xxxxzzzzzzzzyfffff"), (int) strlen("zyfffff"), bCase, 
         iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "zzy", 
         iRelyNull, iRelyNull, bCase, 10);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "zzy", 
         (int) strlen("xxxxzzzzzzzzyf"), (int) strlen("zzy"), bCase, 10);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "zy.f", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "zy.f", 
         (int) strlen("xxxxzzy.fffff"), (int) strlen("zy.f"), bCase, 
         iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xz", 
         iRelyNull, iRelyNull, bCase, 3);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xz", 
         (int) strlen("xxxxzzzzzzzzyf"), (int) strlen("xz"), bCase, 3);
      bAllPassed &= testfind(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "xyz", 
         iRelyNull, iRelyNull, bCase, 4);
      bAllPassed &= testfind(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "xyz", 
         (int) strlen("xyxyxyzyxyz"), (int) strlen("xyz"), bCase, 4);
      bAllPassed &= testfind(
         (uint8_t *) "mississippi", (uint8_t *) "pi", 
         iRelyNull, iRelyNull, bCase, 9);
      bAllPassed &= testfind(
         (uint8_t *) "mississippi", (uint8_t *) "pi", 
         (int) strlen("mississippi"), (int) strlen("pi"), bCase, 9);
      bAllPassed &= testfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "yx", 
         iRelyNull, iRelyNull, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "yx", 
         (int) strlen("xyxyxyxyz"), (int) strlen("yx"), bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "m ississippi", (uint8_t *) "iss", 
         iRelyNull, iRelyNull, bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "m ississippi", (uint8_t *) "iss", 
         1, 1, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ababac", (uint8_t *) "ab?", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ababac", (uint8_t *) "ab?", 
         3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ababac", (uint8_t *) "ab?", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ababac", (uint8_t *) "ab?", 
         2, 2, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "dababac", (uint8_t *) "ababac", 
         iRelyNull, iRelyNull, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "dababac", (uint8_t *) "ababac", 
         3, 2, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "dababac", (uint8_t *) "ababac", 
         2, 2, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "aaazz", (uint8_t *) "aaazz", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaazz", (uint8_t *) "aaazz", 
         2, 2, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "1212", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "1212", 
         3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         4, 4, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         5, 5, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "a12b12", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12b12", (uint8_t *) "a12b12", 
         (int) strlen("xyxyxyxyz") + 2, (int) strlen("xyxyxyxyz") + 2, bCase, 
         iFoundAtFront);

      // A mix of testcases.
      bAllPassed &= testfind(
         (uint8_t *) "n", (uint8_t *) "n", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "n", (uint8_t *) "n", 
         (int) strlen("n"), (int) strlen("n"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aabab", (uint8_t *) "ab", 
         iRelyNull, iRelyNull, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "aabab", (uint8_t *) "ab", 
         (int) strlen("aabab") - 1, (int) strlen("aabab") + 1, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "ar", (uint8_t *) "ar", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "ar", (uint8_t *) "ar", 
         (int) strlen("ar") + 1, (int) strlen("ar") - 1, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aar", (uint8_t *) "aaar", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "aar", (uint8_t *) "aaar", 
         3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XYz", 
         iRelyNull, iRelyNull, bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XYz", 
         3, 3, bCase, iNotFound);

      bAllPassed &= testfind(
         (uint8_t *) "missisSIPpi", (uint8_t *) "SIP", 
         iRelyNull, iRelyNull, bCase, 6);
      bAllPassed &= testfind(
         (uint8_t *) "missisSIPpi", (uint8_t *) "SIP", 
         3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "mississipPI", (uint8_t *) "mississipPI", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "mississipPI", (uint8_t *) "mississipPI", 
         3, 3, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         (int) strlen("xyxyxyxyz"), (int) strlen("xyxyxyxyz"), bCase, 
         iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         (int) strlen("miSsissippi") + 1, iRelyNull, bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         iRelyNull, (int) strlen("miSsissippi"), bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         (int) strlen("miSsissippi"), iRelyNull, bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         iRelyNull, (int) strlen("miSsissippi") + 1, bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abA", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abA", 
         (int) strlen("miSsissippi"), iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abAba", 
         (int) strlen("abAbac"), iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "ababa", 
         iRelyNull, (int) strlen("abAbac"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abAb", (uint8_t *) "abAba", 
         4, 5, bCase, iNotFound);

      bAllPassed &= testfind(
         (uint8_t *) "bLah", (uint8_t *) "LaH", 
         iRelyNull, (int) strlen("bLah"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "bLah", (uint8_t *) "LaH", 
         (int) strlen("bLaH"), iRelyNull, bCase, iNotFound);

      bCase = TRUE;  // The rest of the tests are case-insensitive.

      // A mix of case-insensitive testcases.
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         iRelyNull, iRelyNull, bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         (int) strlen("miSsisSippi"), iRelyNull, bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         iRelyNull, (int) strlen("miSsisSippi"), bCase, 8);
      bAllPassed &= testfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "ppi", 
         1 + (int) strlen("miSsisSippi"), 
         1 + (int) strlen("miSsisSippi"), bCase, 8);

      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "Abac", 
         iRelyNull, iRelyNull, bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abac", 
         iRelyNull, iRelyNull, bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "Abac", 
         (int) strlen("abAbac"), (int) strlen("Abac"), bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abac", 
         (int) strlen("abAbac"), (int) strlen("Abac"), bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abac", 
         (int) strlen("abAbac"), iRelyNull, bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abac", 
         iRelyNull, (int) strlen("Abac"), bCase, 2);
      bAllPassed &= testfind(
         (uint8_t *) "abAbac", (uint8_t *) "abac", 
         (int) strlen("abAbac") + 1, (int) strlen("abAbac") + 1, bCase, 2);

      bAllPassed &= testfind(
         (uint8_t *) "bLah", (uint8_t *) "LaH", 
         iRelyNull, iRelyNull, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "bLah", (uint8_t *) "LaH", 4, 3, bCase, 1);
      bAllPassed &= testfind(
         (uint8_t *) "LaH", (uint8_t *) "bLah", 3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "LaH", (uint8_t *) "bLah", 3, 3, bCase, iNotFound);

      bAllPassed &= testfind(
         (uint8_t *) "aAazz", (uint8_t *) "zz", 
         iRelyNull, iRelyNull, bCase, 3);
      bAllPassed &= testfind(
         (uint8_t *) "aAazz", (uint8_t *) "ZZ", 
         iRelyNull, iRelyNull, bCase, 3);
      bAllPassed &= testfind(
         (uint8_t *) "aAazz", (uint8_t *) "zz", 
         (int) strlen("aAazz"), (int) strlen("zz"), bCase, 3);
      bAllPassed &= testfind(
         (uint8_t *) "aAazz", (uint8_t *) "ZZ", 
         (int) strlen("aAazz"), (int) strlen("ZZ"), bCase, 3);

      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "b123", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "b123", 
         (int) strlen("A12b12"), (int) strlen("A12b123"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "a12B12", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "a12B12", 
         (int) strlen("a12B12"), iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "a12b12", 
         iRelyNull, (int) strlen("a12B12"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "a12b12", 
         (int) strlen("a12B12"), (int) strlen("a12b12"), bCase, iFoundAtFront);

      bAllPassed &= testfind(
         (uint8_t *) "oWn", (uint8_t *) "oWn", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "oWn", (uint8_t *) "oWn", 
         (int) strlen("oWn"), (int) strlen("oWn"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "own", (uint8_t *) "oWn", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "own", (uint8_t *) "oWn", 
         (int) strlen("own"), (int) strlen("oWn"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "own", (uint8_t *) "oWn", 1, 1, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "own", (uint8_t *) "oWn", 1, 2, bCase, iNotFound);         
      bAllPassed &= testfind(
         (uint8_t *) "blah", (uint8_t *) "bLah", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "blah", (uint8_t *) "bLah", 
         (int) strlen("blah"), (int) strlen("bLah"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         (int) strlen("bLah"), (int) strlen("bLaH"), bCase, iFoundAtFront);

        // Longish string scenarios.
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAb", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAb", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAb"), 
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaAb", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         100, 100, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaAb", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajaxalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaAb", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajaxalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         99, 99, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaggggagaaaaaaaab", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaggggagaaaaaaaab", 
         127, 127, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "ggg", iRelyNull, iRelyNull, bCase, 96);
      bAllPassed &= testfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "ggg", 127, 3, bCase, 96);
      bAllPassed &= testfind(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "baabb", 
         iRelyNull, iRelyNull, bCase, 4);
      bAllPassed &= testfind(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "baabb", 
         (int) strlen("aaabbaabbaab"), (int) strlen("baabb"), bCase, 4);         
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 
         bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "AAAAAAAA", iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "AAAAAAAA", 
         (int) strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
         (int) strlen("AAAAAAAA"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", (int) strlen("aaaaaaaaaaaaaaaaa"), 
         (int) strlen("aaaaaaaaaaaaaaaaa"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         iRelyNull, iRelyNull, bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "AAAAA", 
         (int) strlen("aaaaaaaaaaaaaaaaa"), 
         (int) strlen("AAAAA"), bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", (int) strlen("aaaaaaaaaaaaaaaa"), 
         (int) strlen("aaaaaaaaaaaaaaaaa"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc", 
         (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn"), 
         iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia", 
         iRelyNull, 
         (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghiab"), 
         bCase, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefgii", iRelyNull, 
         (int) strlen("abcabcdabcdeabcdefabcdefgabcdefghabcdefghiab"), 
         bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcdefgabcdefghabcdefghiA", iRelyNull, 
         (int) strlen("abcdefgabcdefghabcdefghiAb"), 
         bCase, 18);
   }

   if (bAllPassed)
   {
       printf("Passed partial ASCII content comparison tests\n");
   }
   else
   {
       printf("Failed partial ASCII content comparison tests\n");
   }

   return bAllPassed;
}

// A set of partial-content comparison tests involving empty vs. ascii 
// strings.
//
int testset_find_empty(void)
{
   const int iNotFound = -1;       // A mismatch gives us a negative offset.
   const int iRelyNull = 0;        // Rely on null string terminators.

   int bCase = FALSE;             // Start out with case-sensitive tests.
   int bAllPassed = TRUE;
   int  nReps;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // A simple empty input case.
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "abd", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "abd", 
         iRelyNull, (int) strlen("abd"), bCase, iNotFound);

      // Empty input cases with repeating character sequences
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "abcccd", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "abcccd", 
         iRelyNull, (int) strlen("abcccd"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "mississipisippi", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "mississipisippi", 
         3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyfffff", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyfffff", 
         300, 300, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 
         1, 1, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzy.fffff", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzy.fffff", 
         1, 1, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 
         iRelyNull, 2, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyzyxyz", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyzyxyz", 
         iRelyNull, 2, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "mississippi", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "mississippi", 
         iRelyNull, (int) strlen("mississippi"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, 1, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "m ississippi", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "m ississippi", 
         iRelyNull, 2, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ababac*", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ababac*", 
         iRelyNull, (int) strlen("ababac*"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ababac*", 
         iRelyNull, (int) strlen("ababac"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ababac", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ababac", 
         iRelyNull, (int) strlen("ababac"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aaazz", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aaazz", 5, 5, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "1212", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "1212", 4, 4, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "a12b", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "a12b", 3, 3, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "a12b12", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "a12b12", 2, 2, bCase, iNotFound);

      // A mix of empty input testcases
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "n", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "n", 1, 1, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aabab", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aabab", 1, 1, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ar", 
         iRelyNull, iRelyNull, bCase,  iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "ar", 1, 1, bCase,  iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aaar", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aaar", 
         1, 4, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "XYXYXYZYXYz", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "XYXYXYZYXYz", 
         iRelyNull, (int) strlen("XYXYXYZYXYz"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "missisSIPpi", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "missisSIPpi", 
         iRelyNull, (int) strlen("missisSIPpi"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "mississipPI", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "mississipPI", 
         iRelyNull, (int) strlen("missisSIPpi"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
         iRelyNull, (int) strlen("xyxyxyxyz"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "miSsissippi", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "miSsisSippi", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "miSsisSippi", 
         iRelyNull, 2, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "abAbac", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "abAbac", 
         iRelyNull, (int) strlen("abAbac"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aAazz", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "aAazz", 
         (int) strlen("aAazz"), iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "A12b123", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "A12b123", 
         iRelyNull, (int) strlen("a12B12"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "a12B12", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "a12B12", 
         iRelyNull, (int) strlen("a12B12"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "oWn", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "oWn", 
         iRelyNull, (int) strlen("oWn"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "bLah", 
         iRelyNull, iRelyNull, bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "bLah", 
         iRelyNull, (int) strlen("bLah"), bCase, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "bLaH", 
         iRelyNull, iRelyNull, bCase, iNotFound);

      // Both strings empty
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "", iRelyNull, iRelyNull, bCase, 
         /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "", (uint8_t *) "", 1, 1, bCase, 
         /* iExpectedOffset = */ 0);

      // Another simple case
      bAllPassed &= testfind(
         (uint8_t *) "abc", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "abc", (uint8_t *) "", 3, 
         iRelyNull, bCase, /* iExpectedOffset = */ 0);

      // More empty input cases with repeating character sequences.
      bAllPassed &= testfind(
         (uint8_t *) "abcccd", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "abcccd", (uint8_t *) "", 
         iRelyNull, (int) strlen("abcccd"), bCase, 
         /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "mississipisippi", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "mississipisippi", (uint8_t *) "", 
         iRelyNull, (int) strlen("mississipisippi"), bCase, 
         /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         iRelyNull, (int) strlen("xxxxzzzzzzzzyf"), bCase, 
         /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 1, 1, bCase, 
         /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         2, 2, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         3, 3, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
         4, 4, bCase, /* iExpectedOffset = */ 0);

        // Another mix of empty input testcases
      bAllPassed &= testfind(
         (uint8_t *) "aAazz", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "A12b12", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
      bAllPassed &= testfind(
         (uint8_t *) "a12B12", (uint8_t *) "", 
         iRelyNull, iRelyNull, bCase, /* iExpectedOffset = */ 0);
   }

   if (bAllPassed)
   {
       printf("Passed partial content comparison tests with empty input\n");
   }
   else
   {
       printf("Failed partial content comparison tests with empty input\n");
   }

   return bAllPassed;
}

// Correctness tests for case-sensitive and case-insensitive UTF-8-enabled 
// routines for partial content comparison.
//
int testset_find_utf8(void)
{
   const int iNotFound = -1;       // A mismatch gives us a negative offset.
   const int iFoundAtFront = 0;    // The strings begin with a match.

   int len = 0;                    // Rely on null string terminators.
   int bAllPassed = TRUE;

   do
   {
      // Tests with Asian code points.
      bAllPassed &= testfind(
         (uint8_t *) "三十輻，共一轂，當其無，有車之用。埏埴以為器，當其無，有器之用。鑿戶牖以為室，當其無，有室之用。故有之以為利，無之以為用", 
         (uint8_t *) "三十根车辐尽管彼此不相连接, 却可以形成车毂, 而有车。一扇门窗, 也是利用了“有”与“无”的转换而起到了保暖和通风的作用。", 
          /* lenContent = */ !len ? len : 52, /* lenContent = */ !len ? len : 52,
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "三十輻，共一轂，當其無，有車之用。埏埴以為器，當其無，有器之用。鑿戶牖以為室，當其無，有室之用。故有之以為利，無之以為用", 
         (uint8_t *) "三十輻，共一轂，當其無，有車之用。埏埴以為器，當其無，有器之用。鑿戶牖以為室，當其無，有室之用。故有之以為利，無之以為用", 
          !len ? len : 52, !len ? len : 52, TRUE, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "古池や蛙飛びこむ水の音", 
         (uint8_t *) "古池や蛙飛びこむ水の音", 
          !len ? len : CodePointCountUtf8((uint8_t *) "古池や蛙飛びこむ水の音"), 
          !len ? len : CodePointCountUtf8((uint8_t *) "古池や蛙飛びこむ水の音"), 
          /* bCase = */ TRUE, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "露の世は露の世ながらさりながら", 
         (uint8_t *) "露の世は露の世ながらさりながら", 
          !len ? len : CodePointCountUtf8((uint8_t *) "露の世は露の世ながらさりながら"), 
          !len ? len : CodePointCountUtf8((uint8_t *) "露の世は露の世ながらさりながら"), 
          /* bCase = */ FALSE, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "古池や蛙飛びこむ水の音", 
         (uint8_t *) "閑かさや岩にしみ入る蝉の声", 
          !len ? len : CodePointCountUtf8((uint8_t *) "古池や蛙飛びこむ水の音"), 
          !len ? len : CodePointCountUtf8((uint8_t *) "閑かさや岩にしみ入る蝉の声"), 
          /* bCase = */ TRUE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ᠪᠢ ᠬᠠᠭᠠᠨ ᠴᠤ ᠶᠠᠪᠤᠳᠠᠭ ᠦᠭᠡᠢ", 
         (uint8_t *) "ᠦᠭᠡᠢ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ᠪᠢ ᠬᠠᠭᠠᠨ ᠴᠤ ᠶᠠᠪᠤᠳᠠᠭ ᠦᠭᠡᠢ"), 
          !len ? len : CodePointCountUtf8((uint8_t *) "ᠦᠭᠡᠢ"), 
          /* bCase = */ FALSE, /* iExpectedOffset = */ 20);
      bAllPassed &= testfind(
         (uint8_t *) "사뿐히 즈려 밟고 가시옵소서", 
         (uint8_t *) " 가시옵", 
          !len ? len : CodePointCountUtf8((uint8_t *) "사뿐히 즈려 밟고 가시옵소서"), 
          !len ? len : CodePointCountUtf8((uint8_t *) " 가시옵"), 
          /* bCase = */ TRUE, /* iExpectedOffset = */ 9);
          
      // Positive and negative mixed-case comparisons.
      bAllPassed &= testfind(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "mau mau", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau"), 
          !len ? len : CodePointCountUtf8((uint8_t *) "사뿐히 즈려 밟고 가시옵소서"), 
          /* bCase = */ FALSE, /* iExpectedOffset = */ len ? iNotFound : 73);
      bAllPassed &= testfind(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "Mau Mau", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau."), 
          !len ? len : CodePointCountUtf8((uint8_t *) "Mau Mau"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "Mau Mau", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau."), 
          !len ? len : CodePointCountUtf8((uint8_t *) "Mau Mau"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "Mau Mau", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Mau Mau"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 73);
      bAllPassed &= testfind(
         (uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau, Ôi đàn chim trắng bay về mau mau.", 
         (uint8_t *) "Mau Mau", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Đàn chim trắng​​ Ôi nhớ về đây, các con đi đâu, Ôi đàn chim trắng bay về mau mau"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Mau Mau"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 73);
      bAllPassed &= testfind(
         (uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία", 
         (uint8_t *) "Ω", 
          !len ? len : CodePointCountUtf8((uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Ω"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία", 
         (uint8_t *) "ΤΕΘΝΆΚΗΝ Δ' ὈΛΊΓΩ 'ΠΙΔΕΎΗΣ ΦΑΊΝΟΜ' ἈΛΑΊΑ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία"),
          !len ? len : CodePointCountUtf8((uint8_t *) "τεθνάκην δ' ὀλίγω 'πιδεύης φαίνομ' ἀλαία"),
          /* bCase = */ TRUE, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "окошка вдруг", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          !len ? len : CodePointCountUtf8((uint8_t *) "окошка вдруг"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 67);
      bAllPassed &= testfind(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "вышли куры поклевали", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          !len ? len : CodePointCountUtf8((uint8_t *) "вышли куры поклевали"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "вышли куры поклевали", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          !len ? len : CodePointCountUtf8((uint8_t *) "вышли куры поклевали"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 100);
      bAllPassed &= testfind(
         (uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. Из последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали.", 
         (uint8_t *) "едет поезд запоздалый", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Рельсы, рельсы. Шпалы, шпалы. Едет поезд запоздалый. И3 последнего окошка вдруг посыпались горошки. Вышли куры поклевали. Вышли гуси пощипали."),
          !len ? len : CodePointCountUtf8((uint8_t *) "едет поезд запоздалый"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 30);

      // Tests with emoji.
      bAllPassed &= testfind(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          !len ? len : CodePointCountUtf8((uint8_t *) "🥂✨"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 37);
      bAllPassed &= testfind(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👨‍👩 ‍👧‍🥳👦🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          !len ? len : CodePointCountUtf8((uint8_t *) "👨‍👩 ‍👧‍🥳👦🎉, 🥂✨"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨", 
         (uint8_t *) "👨‍👩 ‍👧‍🥳👦🎉, 🥂✨", 
          !len ? len : CodePointCountUtf8((uint8_t *) "👍🔥, 👏💯 🥰😍, 💕💖, 👩‍❤️‍💋‍👨, 👨‍👩 ‍👧‍👦🥳🎉, 🥂✨"),
          !len ? len : CodePointCountUtf8((uint8_t *) "👨‍👩 ‍👧‍🥳👦🎉, 🥂✨"),
          /* bCase = */ TRUE, iNotFound);

      // Tests with circled Latin code points.
      bAllPassed &= testfind(
         (uint8_t *) "ⒶⒷⒸⒶ", 
         (uint8_t *) "ⓐⓑⓒⓩ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ⒶⒷⒸ"),
           !len ? len : CodePointCountUtf8((uint8_t *) "ⒶⒷⒸ"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ⒶⒷⒸⒶ", 
         (uint8_t *) "ⓐⓑⓒⓩ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ⒶⒷⒸ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ⒶⒷⒸ"),
          /* bCase = */ TRUE, !len ? iNotFound : iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "ⒶⒷⒸⒶ", 
         (uint8_t *) "ⓐⓑⓒⓩ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ⓐⓑⓒⓩ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ⓐⓑⓒⓩ"),
          /* bCase = */ TRUE, iNotFound);

      // Mixed-case Armenian tests.
      bAllPassed &= testfind(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "ԵՍ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "ԵՍ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես"),
          /* bCase = */ TRUE, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          /* bCase = */ FALSE, iFoundAtFront);
      bAllPassed &= testfind(
         (uint8_t *) "Ես իմ անուշ Հայաստանի", 
         (uint8_t *) "Ես իմ անուշ  Հաjաստանի", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ Հայաստանի"),
          !len ? len : CodePointCountUtf8((uint8_t *) "Ես իմ անուշ  Հայաստանի"),
          /* bCase = */ FALSE, iNotFound);

      // A bit of Georgian.
      bAllPassed &= testfind(
         (uint8_t *) "ვმოგზაურობ", 
         (uint8_t *) "არსად არ ვმოგზაურობ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "არსად არ ვმოგზაურობ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ურო"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "ვმოგზაურობ", 
         (uint8_t *) "არსად არ ვმოგზაურობ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "არსად არ ვმოგზაურობ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ურო"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ iNotFound);

      // Tests with Middle Eastern unicameral scripts.
      bAllPassed &= testfind(
         (uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها", 
         (uint8_t *) " أدر كأسا", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها"),
          !len ? len : CodePointCountUtf8((uint8_t *) " أدر كأسا"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 18);
      bAllPassed &= testfind(
         (uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها", 
         (uint8_t *) " أدر كأسا", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ألا يا أيها الساقي أدر كأساً وناولها"),
          !len ? len : CodePointCountUtf8((uint8_t *) " أدر كأسا"),
         /* bCase = */  TRUE, /* iExpectedOffset = */ 18);
      bAllPassed &= testfind(
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
         (uint8_t *) "எலும்", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡"),
          !len ? len : CodePointCountUtf8((uint8_t *) "எலும்"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 9);
      bAllPassed &= testfind(
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
         (uint8_t *) "எலும்", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡"),
          !len ? len : CodePointCountUtf8((uint8_t *) "எலும்"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 9);
      bAllPassed &= testfind(
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
         (uint8_t *) "எமலு்", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡"),
          !len ? len : CodePointCountUtf8((uint8_t *) "எலும்"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡", 
         (uint8_t *) "எமலு்", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𒋳𒈠 𒀀𒉿𒈝 𒂊 எலும்𒋫𒄠 𒊭 𒀀𒉿𒅆𒅎 𒅖𒁉𒅕 𒌑𒇻𒈠 𒊺𒅕𒀀ྣ𒄠 𒈥𒍝𒄠 𒌑𒁀𒀠𒇷𒀉 𒁁𒂖 𒈥𒍢𒅎 𒄿𒈾𒀜𒁲𒅔 Ⅴ 𒅆𒅅𒈝 𒅗 súa 𒉡"),
          !len ? len : CodePointCountUtf8((uint8_t *) "எலும்"),
          /* bCase = */ TRUE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥", 
         (uint8_t *) "☥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥"),
          !len ? len : CodePointCountUtf8((uint8_t *) "☥"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 31);
      bAllPassed &= testfind(
         (uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥", 
         (uint8_t *) "☥", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏𓀐𓀑𓀒𓀓𓀔𓀕𓀖𓀗𓀘𓀙𓀚𓀛𓀜𓀝𓀞☥"),
          !len ? len : CodePointCountUtf8((uint8_t *) "☥"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 31);
      bAllPassed &= testfind(
         (uint8_t *) "মই একেবাৰে ভ্ৰমণ নকৰো", 
         (uint8_t *) "মই ক'তো ভ্ৰমণ নকৰো", 
          !len ? len : 6,
          !len ? len : 6,
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "মই একেবাৰে ভ্ৰমণ নকৰো", 
         (uint8_t *) "মই ক'তো ভ্ৰমণ নকৰো", 
          !len ? len : 6,
          !len ? len : 6,
          /* bCase = */ TRUE, /* iExpectedOffset = */ iNotFound);    
      bAllPassed &= testfind(
         (uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟", 
         (uint8_t *) "𐤆𐤍 𐤟 ", 
           !len ? len : CodePointCountUtf8((uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐤆𐤍 𐤟 "),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 147);
      bAllPassed &= testfind(
         (uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟", 
         (uint8_t *) "𐤆𐤍 𐤟 ", 
           !len ? len : CodePointCountUtf8((uint8_t *) "𐤀𐤓𐤍 𐤟 𐤆 𐤐𐤏𐤋 𐤟 [𐤐]𐤕𐤁𐤏𐤋 𐤟 𐤁𐤍 𐤀𐤇𐤓𐤌 𐤟 𐤌𐤋𐤊 𐤂𐤁𐤋 𐤟 𐤋𐤀𐤇𐤓𐤌 𐤟 𐤀𐤁𐤄 𐤟 𐤊 𐤔𐤕𐤄 𐤟 𐤁𐤏𐤋𐤌 𐤟 𐤅𐤀𐤋 𐤟 𐤌𐤋𐤊 𐤟 𐤁𐤌𐤋𐤊𐤌 𐤟 𐤅𐤎𐤊𐤍 𐤟 𐤁𐤎𐤊𐤍𐤌 𐤟 𐤅𐤕𐤌𐤀 𐤟 𐤌𐤇𐤍𐤕 𐤟 𐤏𐤋𐤉 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤉𐤂𐤋 𐤟 𐤀𐤓𐤍 𐤟 𐤆𐤍 𐤟 𐤕𐤇𐤕𐤎𐤐 𐤟 𐤇𐤈𐤓 𐤟 𐤌𐤔𐤐𐤈𐤄 𐤟 𐤕𐤄𐤕𐤐𐤊 𐤟 𐤊𐤎𐤀 𐤟 𐤌𐤋𐤊𐤄 𐤟 𐤅𐤍𐤇𐤕 𐤟 𐤕𐤁𐤓𐤇 𐤟 𐤏𐤋 𐤟 𐤂𐤁𐤋 𐤟 𐤅𐤄𐤀 𐤟 𐤉𐤌𐤇 𐤎𐤐𐤓 𐤆 𐤟"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐤆𐤍 𐤟 "),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 147);
      bAllPassed &= testfind(
         (uint8_t *) "מי שנקי מחטא, שיהיה הראשון שישליך אבן", 
         (uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה", 
          !len ? len : CodePointCountUtf8((uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה"),
          !len ? len : CodePointCountUtf8((uint8_t *) "מי שנקי מחטא, שיהיה הראשון שישליך אבן"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "מי שנקי מחטא, שיהיה הראשון שישליך אבן", 
         (uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה", 
          !len ? len : CodePointCountUtf8((uint8_t *) "מי מכם שאין בו חטא, שישליך את האבן הראשונה"),
          !len ? len : CodePointCountUtf8((uint8_t *) "מי שנקי מחטא, שיהיה הראשון שישליך אבן"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ iNotFound);

      // A snippet from the Rök Runestone inscription.
      bAllPassed &= testfind(
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
         (uint8_t *) "ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟ"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 36);
      bAllPassed &= testfind(
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
         (uint8_t *) "ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟ"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 36);

      // Tests that include 4-byte code point sequences.
      bAllPassed &= testfind(
         (uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈", 
         (uint8_t *) "𑣉𑣜𑣋𑣗𑣉", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𑣉𑣜𑣋𑣗𑣉"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈", 
         (uint8_t *) "𑣉𑣜𑣋𑣗𑣉", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𑣀𑣂𑣖 𑣁𑣕𑣂 𑣁𑣖𑣇𑣖𑣕𑣈𑣆𑣕𑣈"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𑣉𑣜𑣋𑣗𑣉"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
         (uint8_t *) "𐓆𐒰𐓟𐒷", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐓆𐒰𐓟𐒷"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 9);
      bAllPassed &= testfind(
         (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
         (uint8_t *) "𐓆𐒰𐓟𐒷", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒰͘ 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐓆𐒰𐓟𐒷"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 9);
      bAllPassed &= testfind(
         (uint8_t *) "Obɛri Ɔkaimɛ", 
         (uint8_t *) "RI", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Obɛri Ɔkaimɛ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "RI"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "Obɛri Ɔkaimɛ", 
         (uint8_t *) "RI", 
          !len ? len : CodePointCountUtf8((uint8_t *) "Obɛri Ɔkaimɛ"),
          !len ? len : CodePointCountUtf8((uint8_t *) "RI"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 3);
      bAllPassed &= testfind(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐲𐑄𐐲𐑉𐑆", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐊𐑄𐐲𐑉𐑆"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐲𐑄𐐲𐑉𐑆", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐊𐑄𐐲𐑉𐑆"),
          /* bCase = */ TRUE, /* iExpectedResult = */ 23);
      bAllPassed &= testfind(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐻𐐬𐐶𐐨r𐐼", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉d 𐐊𐑄𐐲𐑉𐑆"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𐐻𐐬𐐶𐐨r𐐼"),
          /* bCase = */ TRUE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤", 
         (uint8_t *) "𞤫 𞤂𞤫𞤻𞤮𞤤", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𞤫 𞤂𞤫𞤻𞤮𞤤"),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 14);
      bAllPassed &= testfind(
         (uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤", 
         (uint8_t *) "𞤫 𞤂𞤫𞤻𞤮𞤤", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
          !len ? len : CodePointCountUtf8((uint8_t *) "𞤫 𞤂𞤫𞤻𞤮𞤤"),
          /* bCase = */ TRUE, /* iExpectedOffset = */ 14);

      // Tests with mixed 1-, 2-, 3-, and 4-byte code points.
      bAllPassed &= testfind(
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝", 
         (uint8_t *) "௵𠁥 ", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝"),
          !len ? len : CodePointCountUtf8((uint8_t *) "௵𠁥 "),
          /* bCase = */ FALSE, /* iExpectedOffset = */ 24);
      bAllPassed &= testfind(
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝", 
         (uint8_t *) "☪☮🕉✡☤☯✝", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ𝄡`🤫🧏ᛋ♅🥑 ∽ੴ𓇀ꙮ🍓🢇ʤʡ🦄௵𠁥 ☪☮🕉✡☤☯✝"),
          !len ? len : CodePointCountUtf8((uint8_t *) "☪☮🕉✡☤☯✝"),
          /* bCase = */ TRUE, 27);          
      bAllPassed &= testfind(
         (uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝", 
         (uint8_t *) "ੴ𓇀⌬ꙮ🍓🢇", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ੴ𓇀⌬ꙮ🍓🢇"),
          /* bCase = */ FALSE, iNotFound);
      bAllPassed &= testfind(
         (uint8_t *) "⚡a⨄𓅓ß𐑄^⚜️ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀⌬ꙮ🍓🢇ʤʡ🦄௵ ☪☮🕉✡☤☯✝", 
         (uint8_t *) "ੴ𓇀⌬ꙮ🍓🢇", 
          !len ? len : CodePointCountUtf8((uint8_t *) "𓅓ß𐑄^ᛋ𐑉ƒ`🤫🧏ᛋ♅ ∽🚀𓇀ꙮ🍓🢇ʤʡ🦄௵🌚 ☪☮🕉✡☤☯✝"),
          !len ? len : CodePointCountUtf8((uint8_t *) "ੴ𓇀⌬ꙮ🍓🢇"),
          /* bCase = */ TRUE, iNotFound);          
   } while (!len++);

   return bAllPassed;
}

//
// Tests for matching wildcards involve these functions:
//    WildCompareUtf8()
//    WildLenCompareUtf8()
//    WildCaseCompareUtf8()
//    WildLenCaseCompareUtf8()
//
// This first set of wildcard comparison tests involves 7-bit ASCII strings.
//
int testset_wildcompare_wild(void)
{
   const int bCase = FALSE;  // Perform only case-sensitive tests.

   int  nReps;
   int  lenContent = 0;       // Rely on null string terminators.
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // Case with first wildcard after total match.
      bAllPassed &= testwildcompare(
         (uint8_t *) "Hi", (uint8_t *) "Hi*", 
         CodePointCountUtf8((uint8_t *) "Hi"), 
         CodePointCountUtf8((uint8_t *) "Hi*"), bCase, /* bExpectedResult = */ TRUE);
      
      // Case with mismatch after '*'
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "ab*d", 
         CodePointCountUtf8((uint8_t *) "abc"), 
         CodePointCountUtf8((uint8_t *) "ab*d"), bCase, /* bExpectedResult = */ FALSE);

      // Cases with repeating character sequences.
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcccd", (uint8_t *) "*ccd", 
         CodePointCountUtf8((uint8_t *) "abcccd"), 
         CodePointCountUtf8((uint8_t *) "*ccd"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "*issip*ss*", 
         CodePointCountUtf8((uint8_t *) "mississipisippi"), 
         CodePointCountUtf8((uint8_t *) "*issip*ss*"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxx*zzzzzzzzy*f", (uint8_t *) "xxxx*zzy*fffff", 
         CodePointCountUtf8((uint8_t *) "xxxx*zzzzzzzzy*f"), 
         CodePointCountUtf8((uint8_t *) "xxxx*zzy*fffff"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxx*zzzzzzzzy*f", (uint8_t *) "xxx*zzy*f", 
         CodePointCountUtf8((uint8_t *) "xxxx*zzzzzzzzy*f"), 
         CodePointCountUtf8((uint8_t *) "xxx*zzy*f"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxx*zzy*fffff", 
         CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), 
         CodePointCountUtf8((uint8_t *) "xxxx*zzy*fffff"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxx*zzy*f", 
         CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), 
         CodePointCountUtf8((uint8_t *) "xxxx*zzy*f"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "xy*z*xyz", 
         CodePointCountUtf8((uint8_t *) "xyxyxyzyxyz"), 
         CodePointCountUtf8((uint8_t *) "xy*z*xyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississippi", (uint8_t *) "*sip*", 
         CodePointCountUtf8((uint8_t *) "mississippi"), 
         CodePointCountUtf8((uint8_t *) "*sip*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xy*xyz", 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xy*xyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississippi", (uint8_t *) "mi*sip*", 
         CodePointCountUtf8((uint8_t *) "mississippi"), 
         CodePointCountUtf8((uint8_t *) "mi*sip*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ababac", (uint8_t *) "*abac*", 
         CodePointCountUtf8((uint8_t *) "ababac"), 
         CodePointCountUtf8((uint8_t *) "*abac*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ababac", (uint8_t *) "*abac*", 
         CodePointCountUtf8((uint8_t *) "ababac"), 
         CodePointCountUtf8((uint8_t *) "*abac*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aaazz", (uint8_t *) "a*zz*", 
         CodePointCountUtf8((uint8_t *) "aaazz"), 
         CodePointCountUtf8((uint8_t *) "a*zz*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "*12*23", 
         CodePointCountUtf8((uint8_t *) "a12b12"), 
         CodePointCountUtf8((uint8_t *) "*12*23"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         CodePointCountUtf8((uint8_t *) "a12b12"), 
         CodePointCountUtf8((uint8_t *) "a12b"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "*12*12*", 
         CodePointCountUtf8((uint8_t *) "a12b12"), 
         CodePointCountUtf8((uint8_t *) "*12*12*"), bCase, /* bExpectedResult = */ TRUE);

      if (!g_bComparePerformance)
      {
         // From DDJ reader Andy Belf: a case of repeating text matching 
         // the different kinds of wildcards in order of '*' and then '?'.
         bAllPassed &= testwildcompare(
           (uint8_t *) "caaab", (uint8_t *) "*a?b", 
             CodePointCountUtf8((uint8_t *) "caaab"), 
             CodePointCountUtf8((uint8_t *) "*a?b"), bCase, /* bExpectedResult = */ TRUE);
         // This similar case was found, probably independently, by Dogan 
         // Kurt.
         bAllPassed &= testwildcompare(
           (uint8_t *) "aaaaa", (uint8_t *) "*aa?", 
             CodePointCountUtf8((uint8_t *) "aaaaa"), 
             CodePointCountUtf8((uint8_t *) "*aa?"), bCase, /* bExpectedResult = */ TRUE);
      }

      // Additional cases where the '*' char appears in the tame string.
      bAllPassed &= testwildcompare(
         (uint8_t *) "*", (uint8_t *) "*", 
         CodePointCountUtf8((uint8_t *) "*"), 
         CodePointCountUtf8((uint8_t *) "*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a*abab", (uint8_t *) "a*b", 
         CodePointCountUtf8((uint8_t *) "a*abab"), 
         CodePointCountUtf8((uint8_t *) "a*b"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a*r", (uint8_t *) "a*", 
         CodePointCountUtf8((uint8_t *) "a*r"), 
         CodePointCountUtf8((uint8_t *) "a*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a*ar", (uint8_t *) "a*aar", 
         CodePointCountUtf8((uint8_t *) "a*ar"), 
         CodePointCountUtf8((uint8_t *) "a*aar"), bCase, /* bExpectedResult = */ FALSE);

      // More double wildcard scenarios.
      bAllPassed &= testwildcompare(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XY*Z*XYz", 
         CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), 
         CodePointCountUtf8((uint8_t *) "XY*Z*XYz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "missisSIPpi", (uint8_t *) "*SIP*", 
         CodePointCountUtf8((uint8_t *) "missisSIPpi"), 
         CodePointCountUtf8((uint8_t *) "*SIP*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississipPI", (uint8_t *) "*issip*PI", 
         CodePointCountUtf8((uint8_t *) "mississipPI"), 
         CodePointCountUtf8((uint8_t *) "*issip*PI"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xy*xyz", 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xy*xyz"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "mi*sip*", 
         CodePointCountUtf8((uint8_t *) "miSsissippi"), 
         CodePointCountUtf8((uint8_t *) "mi*sip*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "mi*Sip*", 
         CodePointCountUtf8((uint8_t *) "miSsissippi"), 
         CodePointCountUtf8((uint8_t *) "mi*Sip*"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abAbac", (uint8_t *) "*Abac*", 
         CodePointCountUtf8((uint8_t *) "abAbac"), 
         CodePointCountUtf8((uint8_t *) "*Abac*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abAbac", (uint8_t *) "*Abac*", 
         CodePointCountUtf8((uint8_t *) "abAbac"), 
         CodePointCountUtf8((uint8_t *) "*Abac*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aAazz", (uint8_t *) "a*zz*", 
         5 + CodePointCountUtf8((uint8_t *) "aAazz"), 
         5 + CodePointCountUtf8((uint8_t *) "a*zz*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "A12b12", (uint8_t *) "*12*23", 
         5 + CodePointCountUtf8((uint8_t *) "A12b12"), 
         5 + CodePointCountUtf8((uint8_t *) "*12*23"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12B12", (uint8_t *) "*12*12*", 
         5 + CodePointCountUtf8((uint8_t *) "a12B12"), 
         5 + CodePointCountUtf8((uint8_t *) "*12*12*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "oWn", (uint8_t *) "*oWn*", 
         5 + CodePointCountUtf8((uint8_t *) "oWn"), 
         5 + CodePointCountUtf8((uint8_t *) "*oWn*"), bCase, /* bExpectedResult = */ TRUE);

      // Completely tame (no wildcards) cases.
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLah", 
         CodePointCountUtf8((uint8_t *) "bLah") - 1, 
         CodePointCountUtf8((uint8_t *) "bLah") - 1, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         CodePointCountUtf8((uint8_t *) "bLah"), 
         CodePointCountUtf8((uint8_t *) "bLaH"), bCase, /* bExpectedResult = */ bCase);

      // Simple mixed wildcard test suggested by Marlin Deckert.
      bAllPassed &= testwildcompare(
         (uint8_t *) "a", (uint8_t *) "*?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ab", (uint8_t *) "*?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "*?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);

      // More mixed wildcard tests including coverage for FALSE positives.
      bAllPassed &= testwildcompare(
         (uint8_t *) "a", (uint8_t *) "??", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ab", (uint8_t *) "?*?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ab", (uint8_t *) "*?*?*", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "?**?*?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "?**?*&?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "?b*??", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "?a*??", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "?**?c?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "?**?d?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcde", (uint8_t *) "?*b*?*d*?",
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);

      // Single-character-match cases.
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bL?h", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLaaa", (uint8_t *) "bLa?", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLa?",
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLaH", (uint8_t *) "?Lah", 
         CodePointCountUtf8((uint8_t *) "bLaH"), 
         CodePointCountUtf8((uint8_t *) "?Lah"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLaH", (uint8_t *) "?LaH",
         CodePointCountUtf8((uint8_t *) "bLaH"), 
         CodePointCountUtf8((uint8_t *) "?LaH"), bCase, /* bExpectedResult = */ TRUE);

      // Many-wildcard scenarios.
      bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (uint8_t *) "a*a*a*a*a*a*aa*aaa*a*a*b", 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "a*a*a*a*a*a*aa*aaa*a*a*b"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *)  "*a*b*ba*ca*a*aa*aaa*fa*ga*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*a*aa*aaa*fa*ga*b*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "*a*b*ba*ca*a*x*aaa*fa*ga*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*a*x*aaa*fa*ga*b*"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*gggg*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*gggg*b*"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*ggg*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*ggg*b*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "*aabbaa*a*",
         CodePointCountUtf8((uint8_t *) "aaabbaabbaab"), 
         CodePointCountUtf8((uint8_t *) "*aabbaa*a*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*", 
         (uint8_t *) "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*",
         CodePointCountUtf8((uint8_t *) "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*"), 
         CodePointCountUtf8((uint8_t *) "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*",
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*", 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn", 
         (uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*", 
         CodePointCountUtf8((uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn"), 
         CodePointCountUtf8((uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*"), 
         bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn", 
         (uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*",
         CodePointCountUtf8((uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn"), 
         CodePointCountUtf8((uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*"), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc*abcd*abcd*abc*abcd", 
         (uint8_t *) "abc*abc*abc*abc*abc", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc*abcd*abcd*abc*abcd*abcd*abc*abcd*abc*abc*abcd", 
         (uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abcd",
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "********a********b********c********",
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "********a********b********c********", (uint8_t *) "abc", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "********a********b********b********", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "*abc*", (uint8_t *) "***a*b*c***", 
         lenContent, lenContent, bCase, /* bExpectedResult = */ TRUE);

      // Tests suggested by other DDJ readers
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "?", 
         CodePointCountUtf8((uint8_t *) ""), 
         CodePointCountUtf8((uint8_t *) "?"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "*?", 
         CodePointCountUtf8((uint8_t *) ""), 
         CodePointCountUtf8((uint8_t *) "*?"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "", 
         CodePointCountUtf8((uint8_t *) ""), 
         CodePointCountUtf8((uint8_t *) ""), bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a", (uint8_t *) "", 
         CodePointCountUtf8((uint8_t *) "a"), 
         CodePointCountUtf8((uint8_t *) ""), bCase, /* bExpectedResult = */ FALSE);
    }

    if (bAllPassed)
    {
        printf("Passed matching wildcards tests with ASCII strings\n");
    }
    else
    {
        printf("Failed matching wildcards tests with ASCII strings\n");
    }

   return bAllPassed;
}

// A set of tests with (almost) no '*' wildcards.
//
int testset_wildcompare_tame(void)
{
   const int bCase = FALSE;  // Perform only case-sensitive tests.
   const int iRelyNull = 0;   // Rely on null string terminators.

   int  nReps;
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

    while (nReps--)
    {
      // Case with last character mismatch.
       bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "abd", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);

       // Cases with repeating character sequences.
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcccd", (uint8_t *) "abcccd", 
           iRelyNull, iRelyNull,  bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "mississipisippi", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyfffff", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzy.fffff", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), 
           CodePointCountUtf8((uint8_t *) "xxxxzzy.fffff"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "xyxyxyzyxyz", 
           CodePointCountUtf8((uint8_t *) "xyxyxyzyxyz"), 
           CodePointCountUtf8((uint8_t *) "xyxyxyzyxyz"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "mississippi", (uint8_t *) "mississippi", 
           CodePointCountUtf8((uint8_t *) "mississippi"), 
           CodePointCountUtf8((uint8_t *) "mississippi"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
           CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
           CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "m ississippi", (uint8_t *) "m ississippi", 
           CodePointCountUtf8((uint8_t *) "m ississippi"), 
           CodePointCountUtf8((uint8_t *) "m ississippi"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "ababac", (uint8_t *) "ababac?", 
           CodePointCountUtf8((uint8_t *) "ababac"), 
           CodePointCountUtf8((uint8_t *) "ababac?"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "dababac", (uint8_t *) "ababac", 
           CodePointCountUtf8((uint8_t *) "dababac"), 
           CodePointCountUtf8((uint8_t *) "ababac"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aaazz", (uint8_t *) "aaazz", 
           CodePointCountUtf8((uint8_t *) "aaazz"), 
           CodePointCountUtf8((uint8_t *) "aaazz"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "1212", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
           iRelyNull, iRelyNull,  bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "a12b12", 
           iRelyNull, iRelyNull,  bCase, /* bExpectedResult = */ TRUE);

       // A mix of testcases
       bAllPassed &= testwildcompare(
         (uint8_t *) "n", (uint8_t *) "n", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aabab", (uint8_t *) "aabab", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "ar", (uint8_t *) "ar", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aar", (uint8_t *) "aaar", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XYXYXYZYXYz", 
           CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), 
           CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "missisSIPpi", (uint8_t *) "missisSIPpi", 
           CodePointCountUtf8((uint8_t *) "missisSIPpi"), 
           CodePointCountUtf8((uint8_t *) "missisSIPpi"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "mississipPI", (uint8_t *) "mississipPI", 
           CodePointCountUtf8((uint8_t *) "mississipPI"), 
           CodePointCountUtf8((uint8_t *) "mississipPI"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsissippi", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsisSippi", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
           CodePointCountUtf8((uint8_t *) "abAbac"), 
           CodePointCountUtf8((uint8_t *) "abAbac"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aAazz", (uint8_t *) "aAazz", 
           CodePointCountUtf8((uint8_t *) "aAazz"), 
           CodePointCountUtf8((uint8_t *) "aAazz"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "A12b12", (uint8_t *) "A12b123", 
           CodePointCountUtf8((uint8_t *) "A12b12"), 
           CodePointCountUtf8((uint8_t *) "A12b123"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "a12B12", (uint8_t *) "a12B12", 
          iRelyNull, iRelyNull,  bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "oWn", (uint8_t *) "oWn", 
           CodePointCountUtf8((uint8_t *) "oWn"), 
           CodePointCountUtf8((uint8_t *) "oWn"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLah", 
           CodePointCountUtf8((uint8_t *) "bLah"), 
           CodePointCountUtf8((uint8_t *) "bLah"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
           CodePointCountUtf8((uint8_t *) "bLah"), 
           CodePointCountUtf8((uint8_t *) "bLaH"), bCase, /* bExpectedResult = */ FALSE);

       // Single '?' cases.
       bAllPassed &= testwildcompare(
         (uint8_t *) "a", (uint8_t *) "a", 
           CodePointCountUtf8((uint8_t *) "a"), 
           CodePointCountUtf8((uint8_t *) "a"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "ab", (uint8_t *) "a?", 
           CodePointCountUtf8((uint8_t *) "ab"), 
           CodePointCountUtf8((uint8_t *) "a?"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "ab?", 
           CodePointCountUtf8((uint8_t *) "abc"), 
           CodePointCountUtf8((uint8_t *) "ab?"), bCase, /* bExpectedResult = */ TRUE);

       // Mixed '?' cases.
       bAllPassed &= testwildcompare(
         (uint8_t *) "a", (uint8_t *) "??", 
           CodePointCountUtf8((uint8_t *) "a"), 
           CodePointCountUtf8((uint8_t *) "??"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "ab", (uint8_t *) "??", 
           CodePointCountUtf8((uint8_t *) "ab"), 
           CodePointCountUtf8((uint8_t *) "??"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "???", 
           CodePointCountUtf8((uint8_t *) "abc"), 
           CodePointCountUtf8((uint8_t *) "???"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "????", 
           CodePointCountUtf8((uint8_t *) "abcd"), 
           CodePointCountUtf8((uint8_t *) "????"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "????", 
           CodePointCountUtf8((uint8_t *) "abc"), 
           CodePointCountUtf8((uint8_t *) "????"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "?b??", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "?a??", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "??c?", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcd", (uint8_t *) "??d?", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcde", (uint8_t *) "?b?d*?", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);

       // Longer string scenarios.
       bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajaxalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaggggagaaaaaaaab", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
          iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "aaabbaabbaab", 
           CodePointCountUtf8((uint8_t *) "aaabbaabbaab"), 
           CodePointCountUtf8((uint8_t *) "aaabbaabbaab"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
           CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 
           CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
           CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), 
           CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), bCase, /* bExpectedResult = */ TRUE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
           CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaa"), 
           CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), bCase, /* bExpectedResult = */ FALSE);
       bAllPassed &= testwildcompare(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc", 
           CodePointCountUtf8((uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn"), 
           CodePointCountUtf8((uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc"), bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcabcdabcdabcabcd", (uint8_t *) "abcabc?abcabcabc", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcabcdabcdabcabcdabcdabcabcdabcabcabcd", 
         (uint8_t *) "abcabc?abc?abcabc?abc?abc?bc?abc?bc?bcd", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "?abc?", (uint8_t *) "?abc?", 
           iRelyNull, iRelyNull, bCase, /* bExpectedResult = */ TRUE);
   }

   if (bAllPassed)
   {
        printf("Passed matching wildcards tests with tame ASCII strings\n");
   }
   else
   {
        printf("Failed matching wildcards tests with tame ASCII strings\n");
   }

   return bAllPassed;
}

// A set of matching wildcards tests with empty input.
//
int testset_wildcompare_empty(void)
{
   const int bCase = FALSE;  // Perform only case-sensitive tests.
   const int iRelyNull = 0;   // Rely on null string terminators.

   int  nReps;
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // A simple case.
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "abd", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "abd"), 
           bCase, /* bExpectedResult = */ FALSE);

      // Cases with repeating character sequences.
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "abcccd", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "abcccd"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "mississipisippi", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "mississipisippi"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyfffff", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyfffff"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 
           iRelyNull, 15, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "7", 
           iRelyNull, 2, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "6", 
           iRelyNull, 6, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "128", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "abd"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "mississippi", 
           iRelyNull, 42, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "",(uint8_t *)  "m ississippi", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "m ississippi"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "ababac*", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ababac*"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "ababac", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ababac"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "aaazz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "aaazz"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "1212", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "1212"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "a12b", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "a12b"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "a12b12", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "a12b12"), 
           bCase, /* bExpectedResult = */ FALSE);

      // A mix of testcases.
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "n", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "n"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "aabab", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "aabab"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "ar", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ar"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "aaar", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ar"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "XYXYXYZYXYz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "missisSIPpi", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "missisSIPpi"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "mississipPI", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "mississipPI"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "miSsissippi", 
           iRelyNull, 1 + CodePointCountUtf8((uint8_t *) "miSsissippi"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "miSsisSippi", 
           iRelyNull, 12, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "abAbac", 
           iRelyNull, 7, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "abAbac", 
           iRelyNull, 7, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "aAazz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "aAazz"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "A12b123", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "A12b123"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "",(uint8_t *)  "a12B12", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "a12B12"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "oWn", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "oWn"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "bLah", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "bLah"), 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "bLaH", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "bLaH"), 
           bCase, /* bExpectedResult = */ FALSE);

      // Both strings empty.
      bAllPassed &= testwildcompare(
         (uint8_t *) "", (uint8_t *) "", 
           iRelyNull, iRelyNull, 
           bCase, /* bExpectedResult = */ TRUE);

      // Another simple case
      bAllPassed &= testwildcompare(
         (uint8_t *) "abc", (uint8_t *) "", 
           iRelyNull, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);

      // Cases with repeating character sequences.
      bAllPassed &= testwildcompare(
         (uint8_t *) "abcccd", (uint8_t *) "", 
           iRelyNull, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississipisippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "mississipisippi"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xyxyxyzyxyz"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "mississippi"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "m ississippi", (uint8_t *)  "", 
           CodePointCountUtf8((uint8_t *) "m ississippi"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ababac", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "ababac"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "dababac",(uint8_t *)  "", 
           iRelyNull, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aaazz",(uint8_t *)  "", 
           6, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "", 
           8, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12",(uint8_t *)  "", 
           128, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12b12", (uint8_t *) "", 
           1, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);

      // A mix of testcases.
      bAllPassed &= testwildcompare(
         (uint8_t *) "n", (uint8_t *) "", 
           1 + CodePointCountUtf8((uint8_t *) "n"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aabab", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "aabab"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ar", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "ar"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aar", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "aar"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "XYXYXYZYXYz",(uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "missisSIPpi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "missisSIPpi"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississipPI", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "mississipPI"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "miSsissippi"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "miSsissippi"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abAbac", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "abAbac") - 3, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abAbac", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "abAbac") - 2, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "aAazz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "aAazz") - 1, iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "A12b12", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "A12b12"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "a12B12", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "a12B12"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "oWn", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "oWn"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah",(uint8_t *)  "", 
           CodePointCountUtf8((uint8_t *) "bLah"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "bLah"), iRelyNull, 
           bCase, /* bExpectedResult = */ FALSE);
   }

   if (bAllPassed)
   {
        printf("Passed matching wildcards tests with empty input\n");
   }
   else
   {
        printf("Failed matching wildcards tests with empty input\n");
   }

   return bAllPassed;
}

// Correctness tests for case-sensitive and case-insensitive UTF-8-enabled 
// routines for matching wildcards.
//
int testset_wildcompare_utf8(void)
{
   int len = 0;               // Rely on null string terminators.
   int bAllPassed = TRUE;

   do
   {
      // Simple correctness test with mixed content.
      bAllPassed &= testwildcompare(
         (uint8_t *) "🐂🚀♥🍀貔貅🦁★□√🚦€¥☯🐴😊🍓🐕🎺🧊☀☂🐉", (uint8_t *) "*☂🐉", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐂🚀♥🍀貔貅🦁★□√🚦€¥☯🐴😊🍓🐕🎺🧊☀☂🐉"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*☂🐉"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);

      // Case-sensitive scenarios.
      bAllPassed &= testwildcompare(
         (uint8_t *) "AbCD", (uint8_t *) "abc?", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "AbCD"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "abc?"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "AbC★", (uint8_t *) "abc?", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "AbC★"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "abc?"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "⚛⚖☁o", (uint8_t *) "⚛⚖☁O", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁o"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁O"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "⚛⚖☁o", (uint8_t *) "⚛⚖☁O", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁o"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁O"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐲𐑄𐐲𐑉𐑆",
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨r𐐼 𐐲𐑄𐐲𐑉𐑆", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨r𐐼 𐐲𐑄𐐲𐑉𐑆"),
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "𞤢 𞤣 𞤤 𞤥 𞤦 𞤧 𞤰 𞤨 𞤩 𞤪 𞤫", 
         (uint8_t *) "𞤀 𞤁 𞤂 𞤃 𞤄 𞤅 𞤎 𞤆 𞤇 𞤈 𞤉", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤸𞤮𞤤", 
         (uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);     
      bAllPassed &= testwildcompare(
         (uint8_t *) "mississippi", (uint8_t *) "*issip*PI",
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "mississippi"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*issip*PI"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "miSsissippi", (uint8_t *) "mi*Sip*", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "miSsissippi"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "mi*Sip*"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "bLah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "bLaH"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "🢇miSsissippi", (uint8_t *) "🢇miSsisSippi", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🢇miSsissippi"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🢇miSsisSippi"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abAßac", (uint8_t *) "abAßac", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "abAßac"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "abAßac"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "abAßaC", (uint8_t *) "abAßac",
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "abAßac"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "abAßac"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "bL௵🌚ah", (uint8_t *) "bL௵🌚aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "bL௵🌚ah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "bL௵🌚aH"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "b௵🌚Lah", (uint8_t *) "bL௵🌚aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "bL௵🌚ah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "bL௵🌚aH"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "b௵🌚Lah", (uint8_t *) "bL௵🌚aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "bL௵🌚ah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "bL௵🌚aH"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);

      // Tests with symbolic content.
      bAllPassed &= testwildcompare(
         (uint8_t *) "b௵🌚Lah", (uint8_t *) "b?🌚?aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "b௵🌚Lah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "b?🌚?aH"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "b௵🌚Lah", (uint8_t *) "b?🌚?aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "b௵🌚Lah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "b?🌚?aH"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "b௵🌚Lah", (uint8_t *) "b?🌚?ah", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "b௵🌚Lah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "b?🌚?ah"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "▲●🐎✗🤣🐶♫🌻ॐ", (uint8_t *) "▲●☂*", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "▲●🐎✗🤣🐶♫🌻ॐ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "▲●☂*"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "𓋍𓋔𓎍", (uint8_t *) "𓋍𓋔?", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𓋍𓋔𓎍"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𓋍𓋔?"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "𓋍𓋔𓎍", (uint8_t *) "𓋍?𓋔𓎍", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𓋍𓋔𓎍"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𓋍?𓋔𓎍"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "♅☌♇", (uint8_t *) "♅☌♇", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "♅☌♇"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "♅☌♇"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "⚛⚖☁", (uint8_t *) "⚛🍄☁", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛🍄☁"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "⚛⚖☁O", (uint8_t *) "⚛⚖☁0", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁O"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁0"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
     
      // Tests with internationalized content.
      bAllPassed &= testwildcompare(
         (uint8_t *) "गते गते पारगते पारसंगते बोधि स्वाहा", 
         (uint8_t *) "गते गते पारगते प????गते बोधि स्वाहा", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "गते गते पारगते पारसंगते बोधि स्वाहा"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "गते गते पारगते प????गते बोधि स्वाहा"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "गते गते पारगते पारसंगते बोधि स्वाहा", 
         (uint8_t *) "गते गते पारगते प????गते बोधि स्वाहा", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "गते गते पारगते पारसंगते बोधि स्वाहा"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "गते गते पारगते प????गते बोधि स्वाहा"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.", 
         (uint8_t *) "Мне нужно выучить * язык, чтобы лучше оценить *.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "Мне нужно выучить * язык, чтобы лучше оценить *."), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);          
      bAllPassed &= testwildcompare(
         (uint8_t *) "Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.", 
         (uint8_t *) "Мне нужно выучить * язык, чтобы лучше оценить *.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "Мне нужно выучить * язык, чтобы лучше оценить *."), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "אני צריך ללמוד אנגלית כדי להעריך את גינסברג", 
         (uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "אני צריך ללמוד אנגלית כדי להעריך את גינסברג"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "אני צריך ללמוד אנגלית כדי להעריך את גינסברג", 
         (uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "אני צריך ללמוד אנגלית כדי להעריך את גינסברג"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "??????????? શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "??????????? શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "??????????? શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "??????????? શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે."), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે."), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
   
      // These tests involve multiple-byte code points that contain bits 
      // identical to those found in the single-byte code points for '*' 
      // and '?'.
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿUἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿUἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿUἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿUἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿЖ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿЖ"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿЖ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿЖ"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "ЬḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ЬḪؿꜪἪꜿ"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "ЬḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ЬḪؿꜪἪꜿ"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ FALSE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "?ؿꜪ*ꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "?ؿꜪ*ꜿ"), 
         /* bCase = */ FALSE, /* bExpectedResult = */ TRUE);
      bAllPassed &= testwildcompare(
         (uint8_t *) "ḪؿꜪἪꜿ", 
         (uint8_t *) "?ؿꜪ*ꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "?ؿꜪ*ꜿ"), 
         /* bCase = */ TRUE, /* bExpectedResult = */ TRUE);
   } while (!len++);

   if (bAllPassed)
   {
      printf("Passed matching wildcards tests with UTF-8 content\n");
   }
   else
   {
      printf("Failed matching wildcards tests with UTF-8 content\n");
   }
   
   return bAllPassed;
}

//
// Tests for targeted wildcard search involve these functions:
//    WildFindUtf8()
//    WildLenFindUtf8()
//    WildCaseFindUtf8()
//    WildLenCaseFindUtf8()
//
// The salient aspect, for this first set of targeted search tests, involves  
// matching wildcards among 7-bit ASCII characters.  Debugging these tests is 
// relatively simple compared to those in testset_targetedsearch_global().
//
int testset_targetedsearch_latin(void)
{
   int bCase = FALSE;      // Perform case-sensitive tests on the first pass.

   int  nReps;
   int  iRelyNull = 0;      // Rely on null string terminators.
   int bAllPassed = TRUE;
   size_t expectedFirst, expectedLast, expectedMatch, expectedTarget;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // Case with first wildcard after total match.
      expectedFirst = 11;
      expectedLast = 12;
      expectedMatch = 13;
      expectedTarget = 13;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯Hi", (uint8_t *) "Hi*", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯Hi"), 
         CodePointCountUtf8((uint8_t *) "Hi*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      
      // Case with mismatch after '*'
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯abc", (uint8_t *) "ab*d", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯abc"), 
         CodePointCountUtf8((uint8_t *) "ab*d"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Cases with repeating character sequences.
      expectedFirst = 0;
      expectedLast = 16;
      expectedMatch = 0;
      expectedTarget = 14;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯abcccd", (uint8_t *) "*ccd", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯abcccd"), 
         CodePointCountUtf8((uint8_t *) "*ccd"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯mississipisippi", (uint8_t *) "*issip*ss*", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯mississipisippi"), 
         CodePointCountUtf8((uint8_t *) "*issip*ss*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯xxxx*zzzzzzzzy*f", (uint8_t *) "xxxx*zzy*fffff", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯xxxx*zzzzzzzzy*f"), 
         CodePointCountUtf8((uint8_t *) "xxxx*zzy*fffff"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 26;
      expectedMatch = 14;
      expectedTarget = 26;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯xxxx*zzzzzzzzy*f", (uint8_t *) "xxx*zzy*f", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯xxxx*zzzzzzzzy*f"), 
         CodePointCountUtf8((uint8_t *) "xxx*zzy*f"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 30;
      expectedMatch = 14;
      expectedTarget = 26;
      bAllPassed &= testwildfind(
          (uint8_t * )"¯\\(ツ)/¯xxxx*zzzzzzzzy*f♖l℻ Я", (uint8_t *) "xxx*zzy*f♖l",
          CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯xxxx*zzzzzzzzy*f"),
          CodePointCountUtf8((uint8_t *) "xxx*zzy*f"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯xxxxzzzzzzzzyf", (uint8_t *) "xxxx*zzy*fffff", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯xxxxzzzzzzzzyf"), 
         CodePointCountUtf8((uint8_t *) "xxxx*zzy*fffff"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 24;
      expectedMatch = 15;
      expectedTarget = 24;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯xxxxzzzzzzzzyf", (uint8_t *) "xxxx*zzy*f", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯xxxxzzzzzzzzyf"), 
         CodePointCountUtf8((uint8_t *) "xxxx*zzy*f"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 21;
      expectedMatch = 13;
      expectedTarget = 19;
      bAllPassed &= testwildfind(
         (uint8_t *) "¯\\(ツ)/¯xyxyxyzyxyz", (uint8_t *) "xy*z*xyz", 
         CodePointCountUtf8((uint8_t *) "¯\\(ツ)/¯xyxyxyzyxyz"), 
         CodePointCountUtf8((uint8_t *) "xy*z*xyz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 10;
      expectedMatch = 0;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "mississippi", (uint8_t *) "*sip*", 
         CodePointCountUtf8((uint8_t *) "mississippi"), 
         CodePointCountUtf8((uint8_t *) "*sip*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedMatch = 2;
      expectedLast = 8;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xy*xyz", 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xy*xyz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 10;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "mississippi", (uint8_t *) "mi*sip*", 
         CodePointCountUtf8((uint8_t *) "mississippi"), 
         CodePointCountUtf8((uint8_t *) "mi*sip*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Set of cases exercising all paths through CodePointBacktrackUtf8().
      expectedLast = 5;
      expectedMatch = 0;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "ababac", (uint8_t *) "*abac*", 
         CodePointCountUtf8((uint8_t *) "ababac"), 
         CodePointCountUtf8((uint8_t *) "*abac*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 10;
      expectedTarget = 10;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐐ababac🐐", (uint8_t *) "*abac*", 
         iRelyNull, iRelyNull,
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "😀ababac😀", (uint8_t *) "*abac*", 
         CodePointCountUtf8((uint8_t *) "😀ababac😀"), 
         CodePointCountUtf8((uint8_t *) "*abac*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 9;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "ꩳababacꩳ", (uint8_t *) "*abac*", 
         CodePointCountUtf8((uint8_t *) "ꩳababacꩳ"), 
         CodePointCountUtf8((uint8_t *) "*abac*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 8;
      expectedTarget = 8;
      bAllPassed &= testwildfind(
         (uint8_t *) "ߘababacߘ", (uint8_t *) "*abac*", 
          iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 9;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "♓ababac♓", (uint8_t *) "*abac*", 
         CodePointCountUtf8((uint8_t *) "♓ababac♓"), 
         CodePointCountUtf8((uint8_t *) "*abac*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 4;
      expectedMatch = 1;
      expectedTarget = 5;
      bAllPassed &= testwildfind(
         (uint8_t *) "aaazz", (uint8_t *) "a*zz*", 
         CodePointCountUtf8((uint8_t *) "aaazz"), 
         CodePointCountUtf8((uint8_t *) "a*zz*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12", (uint8_t *) "*12*23", 
         CodePointCountUtf8((uint8_t *) "a12b12"), 
         CodePointCountUtf8((uint8_t *) "*12*23"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 3;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12", (uint8_t *) "a12b", 
         CodePointCountUtf8((uint8_t *) "a12b12"), 
         CodePointCountUtf8((uint8_t *) "a12b"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 5;
      expectedMatch = 0;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12", (uint8_t *) "*12*12*", 
         CodePointCountUtf8((uint8_t *) "a12b12"), 
         CodePointCountUtf8((uint8_t *) "*12*12*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      if (!g_bComparePerformance)
      {
         // From DDJ reader Andy Belf: a case of repeating text matching 
         // the different kinds of wildcards in order of '*' and then '?'.
         expectedFirst = 0;
	     expectedLast = 4;
         expectedMatch = 0;
         expectedTarget = 2;
         bAllPassed &= testwildfind(
           (uint8_t *) "caaab", (uint8_t *) "*a?b", 
             CodePointCountUtf8((uint8_t *) "caaab"), 
             CodePointCountUtf8((uint8_t *) "*a?b"), 
             expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

         // This similar case was found, probably independently, by Dogan 
         // Kurt.
	     expectedLast = 2;
         expectedTarget = 0;
         bAllPassed &= testwildfind(
           (uint8_t *) "aaaaa", (uint8_t *) "*aa?", 
             CodePointCountUtf8((uint8_t *) "aaaaa"), 
             CodePointCountUtf8((uint8_t *) "*aa?"), 
             expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      }

      // Additional cases where the '*' char appears in the tame string.
      expectedFirst = 0;
      expectedLast = 3;
      expectedMatch = 1;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "a*abab", (uint8_t *) "a*b", 
         CodePointCountUtf8((uint8_t *) "a*abab"), 
         CodePointCountUtf8((uint8_t *) "a*b"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 4;
      expectedMatch = 1;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "ar*ar", (uint8_t *) "a*ar", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "a*ar", (uint8_t *) "a*aar", 
         CodePointCountUtf8((uint8_t *) "a*ar"), 
         CodePointCountUtf8((uint8_t *) "a*aar"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // More double wildcard scenarios.
      expectedFirst = 0;
      expectedLast = 10;
      expectedMatch = 2;
      expectedTarget = 8;
      bAllPassed &= testwildfind(
         (uint8_t *) "XYXYXYZYXYz", (uint8_t *) "XY*Z*XYz", 
         CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), 
         CodePointCountUtf8((uint8_t *) "XY*Z*XYz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 10;
      expectedMatch = 0;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "missisSIPpi", (uint8_t *) "*SIP*", 
         CodePointCountUtf8((uint8_t *) "missisSIPpi"), 
         CodePointCountUtf8((uint8_t *) "*SIP*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "mississipPI", (uint8_t *) "*issip*PI", 
         CodePointCountUtf8((uint8_t *) "mississipPI"), 
         CodePointCountUtf8((uint8_t *) "*issip*PI"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 8;
      expectedMatch = 2;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "xy*xyz", 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xy*xyz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 10;
      expectedMatch = 2;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "mi*sip*", 
         CodePointCountUtf8((uint8_t *) "miSsissippi"), 
         CodePointCountUtf8((uint8_t *) "mi*sip*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      if (!bCase)
      {
         expectedFirst = expectedLast = expectedTarget = 0;
         expectedMatch = g_noMatch;
      }

      bAllPassed &= testwildfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "mi*Sip*", 
         CodePointCountUtf8((uint8_t *) "miSsissippi"), 
         CodePointCountUtf8((uint8_t *) "mi*Sip*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 5;
      expectedMatch = 0;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "abAbac", (uint8_t *) "*Abac*", 
         CodePointCountUtf8((uint8_t *) "abAbac"), 
         CodePointCountUtf8((uint8_t *) "*Abac*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      if (!bCase)
      {
         expectedFirst = expectedLast = expectedTarget = 0;
         expectedMatch = g_noMatch;
      }

      bAllPassed &= testwildfind(
         (uint8_t *) "abAbaC", (uint8_t *) "*Abac*", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 4;
      expectedMatch = 1;
      expectedTarget = 5;
      bAllPassed &= testwildfind(
         (uint8_t *) "aAazz", (uint8_t *) "a*zz*", 
         5 + CodePointCountUtf8((uint8_t *) "aAazz"), 
         5 + CodePointCountUtf8((uint8_t *) "a*zz*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "A12b12", (uint8_t *) "*12*23", 
         5 + CodePointCountUtf8((uint8_t *) "A12b12"), 
         5 + CodePointCountUtf8((uint8_t *) "*12*23"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 5;
      expectedMatch = 0;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "a12B12", (uint8_t *) "*12*12*", 
         5 + CodePointCountUtf8((uint8_t *) "a12B12"), 
         5 + CodePointCountUtf8((uint8_t *) "*12*12*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 2;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "oWn", (uint8_t *) "*oWn*", 
         5 + CodePointCountUtf8((uint8_t *) "oWn"), 
         5 + CodePointCountUtf8((uint8_t *) "*oWn*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Completely tame (no wildcards) cases.
      expectedFirst = 0;
      expectedLast = 2;
      expectedMatch = g_noMatch;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "bLah", (uint8_t *) "bLah", 
         CodePointCountUtf8((uint8_t *) "bLah") - 1, 
         CodePointCountUtf8((uint8_t *) "bLah") - 1, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      if (bCase)
      {
          expectedLast = 3;
      }
      else
      {
          expectedFirst = expectedLast = expectedTarget = 0;
          expectedMatch = g_noMatch;
      }

      bAllPassed &= testwildfind(
         (uint8_t *) "bLah", (uint8_t *) "bLaH", 
         CodePointCountUtf8((uint8_t *) "bLah"), 
         CodePointCountUtf8((uint8_t *) "bLaH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Simple mixed matching wildcards test suggested by Marlin Deckert.
      expectedFirst = 0;
      expectedLast = 0;
      expectedMatch = 0;
      expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "a", (uint8_t *) "*?", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 1;
      expectedTarget = 1;
      bAllPassed &= testwildfind(
         (uint8_t *) "ab", (uint8_t *) "*?", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 2;
      expectedTarget = 2;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc", (uint8_t *) "*?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // More mixed wildcard tests including coverage for FALSE positives.
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "a", (uint8_t *) "??", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 1;
      expectedMatch = 1;
      expectedTarget = 1;
      bAllPassed &= testwildfind(
         (uint8_t *) "ab", (uint8_t *) "?*?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 1;
      expectedMatch = 0;
      expectedTarget = 2;
      bAllPassed &= testwildfind(
         (uint8_t *) "ab", (uint8_t *) "*?*?*", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 2;
      expectedMatch = 1;
      expectedTarget = 2;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc", (uint8_t *) "?**?*?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc", (uint8_t *) "?**?*&?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 3;
      expectedMatch = 2;
      expectedTarget = 2;
      bAllPassed &= testwildfind(
         (uint8_t *) "abcd", (uint8_t *) "?b*??", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abcd", (uint8_t *) "?a*??", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 3;
      expectedMatch = 1;
      expectedTarget = 1;
      bAllPassed &= testwildfind(
         (uint8_t *) "abcd", (uint8_t *) "?**?c?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abcd", (uint8_t *) "?**?d?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 4;
      expectedMatch = 1;
      expectedTarget = 4;
      bAllPassed &= testwildfind(
         (uint8_t *) "abcde", (uint8_t *) "?*b*?*d*?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Single-character cases for verification against FALSE positives.
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLah", (uint8_t *) "bL?h", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLaaa", (uint8_t *) "bLa?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLah", (uint8_t *) "bLa?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
 
      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLaH", (uint8_t *) "?Lah", 
         CodePointCountUtf8((uint8_t *) "🎶☕️bLaH"), 
         CodePointCountUtf8((uint8_t *) "?Lah"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLaH", (uint8_t *) "?LaH",
         CodePointCountUtf8((uint8_t *) "🎶☕️bLaH"), 
         CodePointCountUtf8((uint8_t *) "?LaH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLah", (uint8_t *) "*bL?h7*", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLaaa", (uint8_t *) "b*La7*?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLah", (uint8_t *) "b*L*a7?", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
 
      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLaH", (uint8_t *) "?La*7*h", 
         CodePointCountUtf8((uint8_t *) "🎶☕️bLaH"), 
         CodePointCountUtf8((uint8_t *) "?Lah"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLaH", (uint8_t *) "?La*7H*",
         CodePointCountUtf8((uint8_t *) "🎶☕️bLaH"), 
         CodePointCountUtf8((uint8_t *) "?LaH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Many-wildcard scenarios.
      expectedFirst = 0;
      expectedLast = 90;
      expectedMatch = 1;
      expectedTarget = 90;
      bAllPassed &= testwildfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (uint8_t *) "a*a*a*a*a*a*aa*aaa*a*a*b", 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "a*a*a*a*a*a*aa*aaa*a*a*b"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 109;
      expectedMatch = 0;
      expectedTarget = 110;
      bAllPassed &= testwildfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *)  "*a*b*ba*ca*a*aa*aaa*fa*ga*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*a*aa*aaa*fa*ga*b*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "*a*b*ba*ca*a*x*aaa*fa*ga*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*a*x*aaa*fa*ga*b*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*gggg*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*gggg*b*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 109;
      expectedMatch = 0;
      expectedTarget = 110;
      bAllPassed &= testwildfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*ggg*b*", 
         CodePointCountUtf8((uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab"), 
         CodePointCountUtf8((uint8_t *) "*a*b*ba*ca*aaaa*fa*ga*ggg*b*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 11;
      expectedMatch = 0;
      expectedTarget = 10;
      bAllPassed &= testwildfind(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "*aabbaa*a*",
         CodePointCountUtf8((uint8_t *) "aaabbaabbaab"), 
         CodePointCountUtf8((uint8_t *) "*aabbaa*a*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 1;
      expectedLast = 36;
      expectedMatch = 2;
      expectedTarget = 35;
      bAllPassed &= testwildfind(
         (uint8_t *) "ba*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*ba*b", 
         (uint8_t *) "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*",
         CodePointCountUtf8((uint8_t *) "ba*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*ba*b"), 
         CodePointCountUtf8((uint8_t *) "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 17;
      expectedMatch = 0;
      expectedTarget = 18;
      bAllPassed &= testwildfind(
         (uint8_t *) "baaaaaaaaaaaaaaaaa", 
         (uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*",
         CodePointCountUtf8((uint8_t *) "baaaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "baaaaaaaaaaaaaaaa", 
         (uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*", 
         CodePointCountUtf8((uint8_t *) "baaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn", 
         (uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*", 
         CodePointCountUtf8((uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn"), 
         CodePointCountUtf8((uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 112;
      expectedMatch = 3;
      expectedTarget = 102;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn", 
         (uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*",
         CodePointCountUtf8((uint8_t *) "abc*abcd*abcde*abcdef*abcdefg*abcdefgh*abcdefghi*a\
bcdefghij*abcdefghijk*abcdefghijkl*abcdefghijklm*abcdefghijklmn"), 
         CodePointCountUtf8((uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 20;
      expectedTarget = 18;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc*abcd*abcd*abc*abcd", 
         (uint8_t *) "abc*abc*abc*abc*abc", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 48;
      expectedTarget = 45;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc*abcd*abcd*abc*abcd*abcd*abc*abcd*abc*abc*abcd", 
         (uint8_t *) "abc*abc*abc*abc*abc*abc*abc*abc*abc*abc*abcd",
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 2;
      expectedMatch = 0;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc", (uint8_t *) "********a********b********c********",
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "********a********b********c********", (uint8_t *) "abc", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "abc", (uint8_t *) "********a********b********b********", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 4;
      expectedMatch = 0;
      expectedTarget = 4;
      bAllPassed &= testwildfind(
         (uint8_t *) "*abc*", (uint8_t *) "***a*b*c***", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Matching wildcards tests suggested by other DDJ readers
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "?", 
         CodePointCountUtf8((uint8_t *) ""), 
         CodePointCountUtf8((uint8_t *) "?"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 0;
      expectedMatch = 0;
      expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "*?", 
         CodePointCountUtf8((uint8_t *) ""), 
         CodePointCountUtf8((uint8_t *) "*?"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "", 
         CodePointCountUtf8((uint8_t *) ""), 
         CodePointCountUtf8((uint8_t *) ""), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = 0;
      expectedTarget = g_noMatch;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "a", (uint8_t *) "", 
         CodePointCountUtf8((uint8_t *) "a"), 
         CodePointCountUtf8((uint8_t *) ""), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bCase = TRUE;
   }

   if (bAllPassed)
   {
      printf("Passed targeted wildcard search tests with ASCII strings\n");
   }
   else
   {
      printf("Failed targeted wildcard search tests with ASCII strings\n");
   }

   return bAllPassed;
}

// A set of targeted search tests with (almost) no '*' wildcards.
// Some simple tests with single '*' wildcards are included.
//
int testset_targetedsearch_tame(void)
{
   const int bCase = FALSE;  // Perform only case-sensitive tests.
   const int iRelyNull = 0;   // Rely on null string terminators.
   size_t expectedFirst, expectedLast, expectedMatch, expectedTarget;

   int  nReps;
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // Case with matching ASCII characters but no wildcards.
      expectedFirst = 4;
      expectedLast = 6;
      expectedMatch = g_noMatch;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🥦abc", (uint8_t *) "abc", iRelyNull, iRelyNull,
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Case with last character mismatch.
      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "🥦abc", (uint8_t *) "abd", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Cases with repeating character sequences.
      expectedFirst = 4;
      expectedLast = 9;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🥦abcccd", (uint8_t *) "abcccd", iRelyNull, iRelyNull,  
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 12;
      expectedLast = 26;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🌟🤖🪐mississipisippi", (uint8_t *) "mississipisippi", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 24;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
          (uint8_t *) "💫🤪⚡xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyf",
          iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "💫🤪⚡xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzzzzzzzyff", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
 
      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "🥦xxxxzzzzzzzzyf", (uint8_t *) "xxxxzzy.fffff", 
         CodePointCountUtf8((uint8_t *) "🥦xxxxzzzzzzzzyf"), 
         CodePointCountUtf8((uint8_t *) "xxxxzzy.fffff"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 12;
      expectedLast = 22;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🌟🤖🪐xyxyxyzyxyz", (uint8_t *) "xyxyxyzyxyz", 
         CodePointCountUtf8((uint8_t *) "🌟🤖🪐xyxyxyzyxyz"), 
         CodePointCountUtf8((uint8_t *) "xyxyxyzyxyz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 21;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "💫🤪⚡mississippi", (uint8_t *) "mississippi", 
         CodePointCountUtf8((uint8_t *) "💫🤪⚡mississippi"), 
         CodePointCountUtf8((uint8_t *) "mississippi"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 19;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "💫🤪⚡xyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         CodePointCountUtf8((uint8_t *) "💫🤪⚡xyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 11;
      expectedLast = 22;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "💫🤪⚡m ississippi", (uint8_t *) "m ississippi", 
         CodePointCountUtf8((uint8_t *) "💫🤪⚡m ississippi"), 
         CodePointCountUtf8((uint8_t *) "m ississippi"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "✨ababac", (uint8_t *) "ababac?", 
         CodePointCountUtf8((uint8_t *) "✨ababac"), 
         CodePointCountUtf8((uint8_t *) "ababac?"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 4;
      expectedLast = 9;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "✨dababac", (uint8_t *) "ababac", 
         CodePointCountUtf8((uint8_t *) "✨dababac"), 
         CodePointCountUtf8((uint8_t *) "ababac"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 3;
      expectedLast = 7;
      bAllPassed &= testwildfind(
         (uint8_t *) "✨aaazz", (uint8_t *) "aaazz", 
         CodePointCountUtf8((uint8_t *) "✨aaazz"), 
         CodePointCountUtf8((uint8_t *) "aaazz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12", (uint8_t *) "1212", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "★🪐★a12b12", (uint8_t *) "*a12c", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // A few single-wildcard scenarios
      expectedFirst = 0;
      expectedLast = 16;
      expectedMatch = 0;
      expectedTarget = 13;
      bAllPassed &= testwildfind(
         (uint8_t *) "ℵ★🪐✷a12b12", (uint8_t *) "*a12b", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 3;
      expectedMatch = 6;
      bAllPassed &= testwildfind(
          (uint8_t *) "ℵ★🪐✷a12b12", (uint8_t *) "★*a12b", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 13;
      expectedLast = 18;
      expectedMatch = 17;
      expectedTarget = 17;
      bAllPassed &= testwildfind(
          (uint8_t *) "ℵ★🪐✷a12b12", (uint8_t *) "a12b*", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 3;
      expectedMatch = 4;
      expectedTarget = 4;
      bAllPassed &= testwildfind(
          (uint8_t *) "a12b", (uint8_t *) "a12b*", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // A mix of tame testcases
      expectedFirst = 0;
      expectedLast = 0;
      expectedMatch = g_noMatch;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "n", (uint8_t *) "n", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 4;
      bAllPassed &= testwildfind(
         (uint8_t *) "aabab", (uint8_t *) "aabab", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 1;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "ar🐁", (uint8_t *) "ar", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "aar", (uint8_t *) "aaar", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 6;
      expectedLast = 16;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "ΛΘΛmissisSIPpi", (uint8_t *) "missisSIPpi", 
         CodePointCountUtf8((uint8_t *) "ΛΘΛmissisSIPpi"), 
         CodePointCountUtf8((uint8_t *) "missisSIPpi"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "ѦѮѦmississipPI", (uint8_t *) "mississipPI", 
         CodePointCountUtf8((uint8_t *) "ѦѮѦmississipPI"), 
         CodePointCountUtf8((uint8_t *) "mississipPI"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 14;
      bAllPassed &= testwildfind(
         (uint8_t *) "ѦѮѦxyxyxyxyz", (uint8_t *) "xyxyxyxyz", 
         CodePointCountUtf8((uint8_t *) "ѦѮѦxyxyxyxyz"), 
         CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 17;
      expectedLast = 27;
      bAllPassed &= testwildfind(
         (uint8_t *) "ᛟᚠᛒᚢ ᛋ miSsissippi", (uint8_t *) "miSsissippi", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "miSsisSippi", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 5;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abAbac🦉", (uint8_t *) "abAbac", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "abAbac", (uint8_t *) "abAbac", 
         CodePointCountUtf8((uint8_t *) "abAbac"), 
         CodePointCountUtf8((uint8_t *) "abAbac"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 6;
      expectedLast = 10;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "Ꙅ⚡aAazz🐺", (uint8_t *) "aAazz", 
         CodePointCountUtf8((uint8_t *) "Ꙅ⚡aAazz🐺"), 
         CodePointCountUtf8((uint8_t *) "aAazz"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "≈≈A12b12", (uint8_t *) "A12b123", 
         CodePointCountUtf8((uint8_t *) "≈≈A12b12"), 
         CodePointCountUtf8((uint8_t *) "A12b123"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 4;
      expectedLast = 9;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "ՐՒa12B12", (uint8_t *) "a12B12", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "ՐՒoWn🐾", (uint8_t *) "oWn", 
         CodePointCountUtf8((uint8_t *) "ՐՒoWn🐾"), 
         CodePointCountUtf8((uint8_t *) "oWn"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 7;
      bAllPassed &= testwildfind(
         (uint8_t *) "ՐՒbLah🦆", (uint8_t *) "bLah", 
         CodePointCountUtf8((uint8_t *) "ՐՒbLah🦆"), 
         CodePointCountUtf8((uint8_t *) "bLah"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "ՐՒbLah🐇", (uint8_t *) "bLaH", 
         CodePointCountUtf8((uint8_t *) "ՐՒbLah🐇"), 
         CodePointCountUtf8((uint8_t *) "bLaH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Single ASCII character.
      expectedFirst = 0;
      expectedLast = 0;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "a", (uint8_t *) "a", 
         CodePointCountUtf8((uint8_t *) "a"), 
         CodePointCountUtf8((uint8_t *) "a"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Single '*' and single '?' cases.
      expectedFirst = 0;
      expectedLast = 12;
      expectedMatch = 0;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
          (uint8_t *) "ꏁ⅟☈‱✪⅖", (uint8_t *) "*☈?✪", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedLast = 15;
      expectedMatch = 3;
      expectedTarget = 9;
      bAllPassed &= testwildfind(
          (uint8_t *) "⅜ꏁ⅟☈‱✪⅖", (uint8_t *) "⅜*☈?✪", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 3;
      expectedLast = 16;
      expectedMatch = 6;
      expectedTarget = 13;
      bAllPassed &= testwildfind(
          (uint8_t *) "ℵ★🪐✷a12b12", (uint8_t *) "★*a12b", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 9;
      expectedMatch = 0;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
          (uint8_t *) "䷸★⅋b⟸", (uint8_t *) "*★?b",
          CodePointCountUtf8((uint8_t *) "䷸★⅋b⟸"),
          CodePointCountUtf8((uint8_t *) "*★?b"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Single '*' and multiple '?' cases.
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "䷸◡̈ab⟸", (uint8_t *) "*◡̈??b", 
         CodePointCountUtf8((uint8_t *) "䷸◡̈ab⟸"), 
         CodePointCountUtf8((uint8_t *) "*◡̈??b"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 29;
      expectedMatch = 0;
      expectedTarget = 12;
      bAllPassed &= testwildfind(
          (uint8_t *) "ᕒ⌘⚠䷸⯂ᔣ🌏🐰⯏b⟸", (uint8_t *) "*⯂???⯏b",
          iRelyNull, iRelyNull, 
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 3;
      expectedLast = 29;
      expectedMatch = 6;
      expectedTarget = 12;
      bAllPassed &= testwildfind(
          (uint8_t *) "ᕒ⌘⚠䷸⯂ᔣ🌏🐰⯏b⟸", (uint8_t *) "⌘*⯂???⯏b",
          CodePointCountUtf8((uint8_t *) "ᕒ⌘⚠䷸⯂ᔣ🌏🐰⯏b⟸"),
          CodePointCountUtf8((uint8_t *) "⌘*⯂???⯏b"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 2;
      expectedMatch = 0;
      expectedTarget = 0;
      bAllPassed &= testwildfind(
          (uint8_t *) "abc", (uint8_t *) "*???",
          CodePointCountUtf8((uint8_t *) "abc"),
          CodePointCountUtf8((uint8_t *) "*???"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 7;
      expectedMatch = 1;
      expectedTarget = 4;
      bAllPassed &= testwildfind(
          (uint8_t *) "abcdabcd", (uint8_t *) "a*a??d", iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Longer all-tame scenarios.
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "⚔️aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         (uint8_t *) "ヅaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", 
         iRelyNull, iRelyNull, expectedFirst, expectedLast, expectedMatch, 
         expectedTarget, bCase);

      expectedFirst = 8;
      expectedLast = 117;
      expectedMatch = g_noMatch;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "◡ᵕ̈abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         iRelyNull, iRelyNull, expectedFirst, expectedLast, expectedMatch, 
         expectedTarget, bCase);

      expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajaxalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         iRelyNull, iRelyNull, expectedFirst, expectedLast, expectedMatch, 
         expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 109;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         (uint8_t *) "abababababababababababababababababababaacacacacaca\
cacadaeafagahaiajakalaaaaaaaaaaaaaaaaaffafagaagggagaaaaaaaab", 
         iRelyNull, iRelyNull, expectedFirst, expectedLast, expectedMatch, 
         expectedTarget, bCase);

      expectedLast = 11;
      bAllPassed &= testwildfind(
         (uint8_t *) "aaabbaabbaab", (uint8_t *) "aaabbaabbaab", 
         CodePointCountUtf8((uint8_t *) "aaabbaabbaab"), 
         CodePointCountUtf8((uint8_t *) "aaabbaabbaab"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 8;
      expectedLast = 41;
      bAllPassed &= testwildfind(
         (uint8_t *) "◡ᵕ̈aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         CodePointCountUtf8((uint8_t *) "◡ᵕ̈aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 16;
      bAllPassed &= testwildfind(
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "aaaaaaaaaaaaaaaa", 
         (uint8_t *) "aaaaaaaaaaaaaaaaa", 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaa"), 
         CodePointCountUtf8((uint8_t *) "aaaaaaaaaaaaaaaaa"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      bAllPassed &= testwildfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc", 
         CodePointCountUtf8((uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn"), 
         CodePointCountUtf8((uint8_t *) "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 0;
      expectedLast = 101;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         (uint8_t *) "abcabcdabcdeabcdefabcdefgabcdefghabcdefghia\
bcdefghijabcdefghijkabcdefghijklabcdefghijklmabcdefghijklmn", 
         iRelyNull, iRelyNull, expectedFirst, expectedLast, expectedMatch, 
         expectedTarget, bCase);

      expectedFirst = expectedLast = expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "🪐🪐🪐abcabcdabcdabcabcd", (uint8_t *) "abcabcdabcabcabc", 
         iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      expectedFirst = 15;
      expectedLast = 30;
      expectedTarget = g_noMatch;
      bAllPassed &= testwildfind(
          (uint8_t *) "🪐🪐🪐abcabcabcdabcabcabcd", (uint8_t *) "abcabcdabcabcabc",
          iRelyNull, iRelyNull,
          expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
   }

   if (bAllPassed)
   {
        printf("Passed targeted wildcard search tests with tame ASCII strings\n");
   }
   else
   {
        printf("Failed targeted wildcard search tests with tame ASCII strings\n");
   }

   return bAllPassed;
}

// A set of targeted wildcard search tests with empty input.
//
int testset_targetedsearch_empty(void)
{
   const int bCase = FALSE;  // Perform only case-sensitive tests.
   const int iRelyNull = 0;   // Rely on null string terminators.
   size_t expectedFirst, expectedLast, expectedMatch, expectedTarget;

   int  nReps;
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      // Can choose as many repetitions as you might expect in production.
      nReps = g_iTestRepetitions;
   }
   else
   {
      nReps = 1;
   }

   while (nReps--)
   {
      // A simple case.
      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "abd", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "abd"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Cases with repeating character sequences.
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "abcccd", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "abcccd"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "mississipisippi", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "mississipisippi"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyfffff", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyfffff"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "xxxxzzzzzzzzyf", 
           iRelyNull, 15, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "7", 
           iRelyNull, 2, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "6", 
           iRelyNull, 6, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "128", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "abd"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "mississippi", 
           iRelyNull, 42, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "",(uint8_t *)  "m ississippi", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "m ississippi"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "ababac*", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ababac*"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "ababac", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ababac"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "aaazz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "aaazz"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "1212", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "1212"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "a12b12", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "a12b12"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // A mix of testcases.
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "n", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "n"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "aabab", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "aabab"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "ar", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ar"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "aaar", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "ar"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "XYXYXYZYXYz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "missisSIPpi", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "missisSIPpi"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "mississipPI", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "mississipPI"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "xyxyxyxyz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "miSsissippi", 
           iRelyNull, 1 + CodePointCountUtf8((uint8_t *) "miSsissippi"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "miSsisSippi", 
           iRelyNull, 12, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "abAbac", 
           iRelyNull, 7, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "abAbac", 
           iRelyNull, 7, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "aAazz", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "aAazz"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "A12b123", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "A12b123"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "",(uint8_t *)  "a12B12", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "a12B12"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "oWn", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "oWn"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "bLah", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "bLah"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "bLaH", 
           iRelyNull, CodePointCountUtf8((uint8_t *) "bLaH"), 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Both strings empty.
      expectedFirst = 0;
      expectedLast = 0;
      expectedMatch = 0;
      expectedTarget = 0;
      bAllPassed &= testwildfind(
         (uint8_t *) "", (uint8_t *) "", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Another simple case
      expectedFirst = expectedLast = 0;
      expectedTarget = g_noMatch;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "abc", (uint8_t *) "", iRelyNull, iRelyNull, 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // Cases with repeating character sequences.
      bAllPassed &= testwildfind(
         (uint8_t *) "abcccd", (uint8_t *) "", iRelyNull, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "mississipisippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "mississipisippi"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xxxxzzzzzzzzyf", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xxxxzzzzzzzzyf"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xyxyxyzyxyz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xyxyxyzyxyz"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "mississippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "mississippi"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "m ississippi", (uint8_t *)  "", 
           CodePointCountUtf8((uint8_t *) "m ississippi"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "ababac", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "ababac"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "dababac",(uint8_t *)  "", 
           iRelyNull, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "aaazz",(uint8_t *)  "", 
           6, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12", (uint8_t *) "", 
           8, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12",(uint8_t *)  "", 
           128, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "a12b12", (uint8_t *) "", 
           1, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);

      // A mix of testcases.
      bAllPassed &= testwildfind(
         (uint8_t *) "n", (uint8_t *) "", 
           1 + CodePointCountUtf8((uint8_t *) "n"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "aabab", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "aabab"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "ar", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "ar"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "aar", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "aar"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "XYXYXYZYXYz",(uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "XYXYXYZYXYz"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "missisSIPpi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "missisSIPpi"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "mississipPI", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "mississipPI"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "xyxyxyxyz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "xyxyxyxyz"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "miSsissippi"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "miSsissippi", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "miSsissippi"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "abAbac", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "abAbac") - 3, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "abAbac", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "abAbac") - 2, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "aAazz", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "aAazz") - 1, iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "A12b12", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "A12b12"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "a12B12", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "a12B12"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "oWn", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "oWn"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "bLah",(uint8_t *)  "", 
           CodePointCountUtf8((uint8_t *) "bLah"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
      bAllPassed &= testwildfind(
         (uint8_t *) "bLah", (uint8_t *) "", 
           CodePointCountUtf8((uint8_t *) "bLah"), iRelyNull, 
           expectedFirst, expectedLast, expectedMatch, expectedTarget, bCase);
   }

   if (bAllPassed)
   {
        printf("Passed targeted wildcard search tests with empty input\n");
   }
   else
   {
        printf("Failed targeted wildcard search tests with empty input\n");
   }

   return bAllPassed;
}

// Correctness tests for case-sensitive and case-insensitive UTF-8-enabled 
// routines for targeted wildcard search.
//
int testset_targetedsearch_global(void)
{
   int len = 0;               // Rely on null string terminators.
   int bAllPassed = TRUE;
   size_t expectedFirst, expectedLast, expectedMatch, expectedTarget;

   do
   {
      // Simple correctness test with mixed content.
      expectedFirst = 0;
      expectedLast = 76;
      expectedMatch = 0;
      expectedTarget = 73;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐂🚀♥🍀貔貅🦁★□√🚦€¥☯🐴😊🍓🐕🎺🧊☀☂🐉", (uint8_t *) "*☂🐉", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐂🚀♥🍀貔貅🦁★□√🚦€¥☯🐴😊🍓🐕🎺🧊☀☂🐉"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*☂🐉"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      // Tests with symbolic content.
      expectedLast = 6;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "≥AbCD", (uint8_t *) "*abc?", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "≥AbCD"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*abc?"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 3;
      expectedLast = 9;
      expectedMatch = 6;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "≥≫AbC★", (uint8_t *) "≫*abc?", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "≥≫AbC★"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "≫*abc?"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "≳⚛⚖☁o", (uint8_t *) "⚛⚖☁O", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "≳⚛⚖☁o"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚛⚖☁O"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 6;
      expectedLast = 23;
      expectedMatch = 10;
      expectedTarget = 14;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃⊃🌻🌻⚛⚖☁o", (uint8_t *) "🌻*⚛⚖☁O", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃⊃🌻🌻⚛⚖☁o"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🌻*⚛⚖☁O"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 106;
      expectedMatch = 0;
      expectedTarget = 10;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃🌻...𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "*🌻*𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐲𐑄𐐲𐑉𐑆",
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃🌻...𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*🌻*𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐲𐑄𐐲𐑉𐑆"),
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "≳🌻...𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆", 
         (uint8_t *) "*🌻*𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨r𐐼 𐐲𐑄𐐲𐑉𐑆", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "≳🌻...𐐀𐑌𐑊𐐪𐑉𐐽 𐐏𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨𐑉𐐼 𐐊𐑄𐐲𐑉𐑆"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*🌻*𐐨𐑌𐑊𐐪𐑉𐐽 𐐷𐐬𐑉 𐑅𐐬𐑊𐑆 𐐻𐐬𐐶𐐨r𐐼 𐐲𐑄𐐲𐑉𐑆"),
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 8;
      expectedLast = 58;
      expectedMatch = 12;
      expectedTarget = 12;
      bAllPassed &= testwildfind(
         (uint8_t *) "≫🌻 𞤢 𞤣 𞤤 𞤥 𞤦 𞤧 𞤰 𞤨 𞤩 𞤪 𞤫", 
         (uint8_t *) "𞤀* 𞤁 𞤂 𞤃 𞤄 𞤅 𞤎 𞤆 𞤇 𞤈 𞤉", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "≥𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤸𞤮𞤤", 
         (uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫* 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤤𞤳𞤵𞤤𞤫 𞤁𞤢𞤲𞤣𞤢𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞤀𞤂𞤖𞤵𞤤𞤫* 𞤁𞤀𞤲𞤣𞤀𞤴𞤯𞤫 𞤂𞤫𞤻𞤮𞤤 𞤃𞤵𞤤𞤵𞤺𞤮𞤤"),
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 21;
      expectedMatch = 0;
      expectedTarget = 20;
      bAllPassed &= testwildfind(
         (uint8_t *) "🌻ississip🌻🌻pi", (uint8_t *) "*issip*PI",
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🌻ississip🌻🌻pi"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*issip*PI"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 4;
      expectedLast = 22;
      expectedMatch = 6;
      expectedTarget = 13;
      bAllPassed &= testwildfind(
         (uint8_t *) "🌻miSsissip🌻🌻pi", (uint8_t *) "mi*Sip*", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🌻miSsissip🌻🌻pi"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "mi*Sip*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 13;
      expectedMatch = 0;
      expectedTarget = 13;
      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️bLah", (uint8_t *) "*bLa*H", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🎶☕️bLah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*bLaH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️🢇miSsissippi", (uint8_t *) "🐧⚓*🢇miSsisSip*pi", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🎶☕️🢇miSsissippi"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐧⚓*🢇miSsisSippi"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 4;
      expectedLast = 21;
      expectedMatch = 7;
      expectedTarget = 20;
      bAllPassed &= testwildfind(
          (uint8_t *) "🐧⚓🢇miSsissippi", (uint8_t *) "⚓*🢇miSsisSip*pi",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⚓*🢇miSsisSip*pi"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐧⚓*🢇miSsisSippi"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🎶☕️abAßac", (uint8_t *) "🐧⚓*abAß*c", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🎶☕️abAßac"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐧⚓*abAßac"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 13;
      expectedMatch = 7;
      expectedTarget = 7;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐧⚓abAßaC", (uint8_t *) "🐧⚓*abAß?c",
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐧⚓abAßac"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐧⚓*abAßac"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆bL௵🌚ah", (uint8_t *) "🐧⚓*bL௵🌚aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆bL௵🌚ah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐧⚓*bL௵🌚aH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆b௵🌚Lah", (uint8_t *) "*🐧⚓*bL௵🌚aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆bL௵🌚ah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*🐧⚓*bL௵🌚aH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆b௵🌚Lah", (uint8_t *) "🐧⚓*bL௵🌚aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆bL௵🌚ah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*🐧⚓*bL௵🌚aH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 16;
      expectedMatch = 0;
      expectedTarget = 6;
      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆b௵🌚Lah", (uint8_t *) "*b?🌚?aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆b௵🌚Lah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*b?🌚?aH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆b௵🌚Lah", (uint8_t *) "*b?🌚?aH", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆b௵🌚Lah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*b?🌚?aH"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 16;
      expectedMatch = 0;
      expectedTarget = 16;
      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆b௵🌚Lah", (uint8_t *) "*b?🌚?*h", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆b௵🌚Lah"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*b?🌚?*h"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "❆❆▲●🐎✗🤣🐶♫🌻ॐ", (uint8_t *) "*▲●☂*", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "❆❆▲●🐎✗🤣🐶♫🌻ॐ"),
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*▲●☂*"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 11;
      expectedMatch = 0;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃𓋍𓋔𓎍", (uint8_t *) "*𓋍𓋔?", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃𓋍𓋔𓎍"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*𓋍𓋔?"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃𓋍𓋔𓎍", (uint8_t *) "*𓋍?𓋔𓎍", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃𓋍𓋔𓎍"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*𓋍?𓋔𓎍"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 9;
      expectedMatch = 0;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃♅☌♇", (uint8_t *) "*♅☌♇", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃♅☌♇"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*♅☌♇"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedTarget = 9;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃⚛⚖☁", (uint8_t *) "*⚛*☁", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃⚛⚖☁"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*⚛*☁"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃⚛⚖☁O", (uint8_t *) "*⚛*0", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃⚛⚖☁O"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*⚛*0"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);
     
      // Tests with internationalized content.
      expectedFirst = 0;
      expectedLast = 95;
      expectedMatch = 0;
      expectedTarget = 3;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃गते गते पारगते पारसंगते बोधि स्वाहा", 
         (uint8_t *) "*गते गते पारगते प????गते बोधि स्वाहा", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃गते गते पारगते पारसंगते बोधि स्वाहा"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*गते गते पारगते प????गते बोधि स्वाहा"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃गते गते पारगते पारसंगते बोधि स्वाहा", 
         (uint8_t *) "*गते गते पारगते प????गते बोधि स्वाहा", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃गते गते पारगते पारसंगते बोधि स्वाहा"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*गते गते पारगते प????गते बोधि स्वाहा"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.", 
         (uint8_t *) "мне нужно выучить * язык, чтобы лучше оценить *.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "мне нужно выучить * язык, чтобы лучше оценить *."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 4;
      expectedLast = 113;
      expectedMatch = 37;
      expectedTarget = 98;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.", 
         (uint8_t *) "мне нужно выучить * язык, чтобы * ???????.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "мне нужно выучить * язык, чтобы * ???????."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedTarget = 113;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.", 
         (uint8_t *) "мне нужно выучить * язык, чтобы лучше оценить *.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "Мне нужно выучить * язык, чтобы лучше оценить *."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 25;
      expectedMatch = 0;
      expectedTarget = 25;
      bAllPassed &= testwildfind(
          (uint8_t *) "Able was I ere I saw Elba.",
          (uint8_t *) "*??? was I * I saw *.",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "Able was I ere I saw Elba."),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*??? was I * I saw *."),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 113;
      expectedMatch = 0;
      expectedTarget = 113;
      bAllPassed &= testwildfind(
         (uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.", 
         (uint8_t *) "*??? нужно выучить * язык, чтобы *.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*??? нужно выучить * язык, чтобы *."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
          (uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина.",
          (uint8_t *) "??? нужно выучить * язык, чтобы *.",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "🐍Мне нужно выучить русский язык, чтобы лучше оценить Пушкина."),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "??? нужно выучить * язык, чтобы *."),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 74;
      expectedMatch = 0;
      expectedTarget = 60;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍😍😍 abcde (F)ghiJnn (A)bc defg hijkl ab cdef ghijkl M Nopqrs tlhfd",
          (uint8_t *) "*????? * (A)bc defg hijkl ab cdef * M ?????? tlhfd",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍😍😍 abcde (F)ghiJnn (A)bc defg hijkl ab cdef ghijkl M Nopqrs tlhfd"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*????? * (A)bc defg hijkl ab cdef * M ?????? tlhfd"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ FALSE);

      expectedLast = 168;
      expectedTarget = 132;
      bAllPassed &= testwildfind(
         (uint8_t *) "😍😍😍 ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ", 
         (uint8_t *) "*????? * (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ * ᛞ ?????? ᚺᛟᛋᛚᛃ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍😍😍 ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*????? * (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ * ᛞ ?????? ᚺᛟᛋᛚᛃ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 1;
      expectedLast = 28;
      expectedMatch = 4;
      expectedTarget = 7;
      bAllPassed &= testwildfind(
          (uint8_t *) "B B a B rstln e slaA slaA Klf",
          (uint8_t *) " ? * ????? ? ???? ???? ???",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "B B a B rstln e slaA slaA Klf"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " ? * ????? ? ???? ???? ???"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ FALSE);

      expectedFirst = 4;
      expectedLast = 88;
      expectedMatch = 10;
      expectedTarget = 19;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻",
          (uint8_t *) " ? * ????? ? ???? ???? ???",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " ? * ????? ? ???? ???? ???"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ FALSE);

      expectedFirst = 13;
      expectedLast = 168;
      expectedMatch = 29;
      expectedTarget = 132;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍😍😍 ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ",
          (uint8_t *) "ᛋᚭᚷᚹᛗ * (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ * ᛞ ?????? ᚺᛟᛋᛚᛃ",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍😍😍 ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿᚭᛦ ᚺᛟᛋᛚᛃ"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ * (ᚦ)ᚭᛞ ᚺᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ * ᛞ ?????? ᚺᛟᛋᛚᛃ"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ FALSE);

      expectedFirst = 20;
      expectedLast = 88;
      expectedMatch = 24;
      expectedTarget = 36;
      bAllPassed &= testwildfind(
         (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻", 
         (uint8_t *) "𐓨*𐒷 ? ???? ???? ???", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐓨*𐒷 ? ???? ???? ???"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 20;
      expectedLast = 75;
      expectedTarget = expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻",
          (uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ TRUE);

      expectedFirst = 5;
      expectedLast = 88;
      expectedMatch = 24;
      expectedTarget = 36;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻",
          (uint8_t *) "😍 ? 😍 𐓨*𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 ???",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 ? 😍 𐓨*𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 ???"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ TRUE);

      expectedFirst = 20;
      expectedLast = 32;
      expectedMatch = 24;
      expectedTarget = 32;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻",
          (uint8_t *) "𐓨*𐒷",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐓨*𐒷"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 36;
      expectedMatch = 0;
      expectedTarget = 19;
      bAllPassed &= testwildfind(
          (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻",
          (uint8_t *) "* 𐓨𐓤𐒰𐓟𐒷",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "* 𐓨𐓤𐒰𐓟𐒷"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ TRUE);

      bAllPassed &= testwildfind(
          (uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻",
          (uint8_t *) "* 𐓨𐓤??𐒷",
          /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 😍 𐒷 😍 𐓨𐓤𐒰𐓟𐒷 𐒷 𐓆𐒰𐓟𐒷 𐓆𐒰𐓟𐒷 𐒼𐒰𐒻"),
          /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "* 𐓨𐓤𐒰𐓟𐒷"),
          expectedFirst, expectedLast, expectedMatch, expectedTarget,
          /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃אני צריך ללמוד אנגלית כדי להעריך את גינסברג", 
         (uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃אני צריך ללמוד אנגלית כדי להעריך את גינסברג"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃אני צריך ללמוד אנגלית כדי להעריך את גינסברג", 
         (uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃אני צריך ללמוד אנגלית כדי להעריך את גינסברג"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " אני צריך ללמוד אנגלית כדי להעריך את ???????"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = 0;
      expectedLast = 176;
      expectedMatch = 0;
      expectedTarget = 144;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "* શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "*??????????? શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*??????????? શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે * શીખવું પડશે."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 36;
      expectedMatch = 59;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) " શ્રેષ્ઠ * કરવા માટે મારે * શીખવું પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) " શ્રેષ્ઠ * કરવા માટે મારે * શીખવું પડશે."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે.", 
         (uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે.", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે અંગ્રેજી શીખવું પડશે."), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ગિન્સબર્ગની શ્રેષ્ઠ પ્રશંસા કરવા માટે મારે હિબ્રુ ભાષા શીખવી પડશે."), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);
   
      // These tests involve multiple-byte code points that contain bits 
      // identical to those found in the single-byte code points for '*' 
      // and '?'.
      expectedFirst = 3;
      expectedLast = 14;
      expectedMatch = 6;
      expectedTarget = 8;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "Ḫ*ꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "Ḫ*ꜪἪꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = expectedLast = expectedTarget = 0;
      expectedMatch = g_noMatch;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿUἪꜿ", 
         (uint8_t *) "Ḫ*ꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿUἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "Ḫ*ꜪἪꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿUἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿUἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "Ḫ*Ꜫ*Ж", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "Ḫ*Ꜫ*Ж"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "ḪؿꜪἪꜿЖ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ḪؿꜪἪꜿЖ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "ЬḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ЬḪؿꜪἪꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "ЬḪؿꜪἪꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "ЬḪؿꜪἪꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 0;
      expectedLast = 14;
      expectedMatch = 0;
      expectedTarget = 14;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "*?ؿꜪ*ꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*?ؿꜪ*ꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ FALSE);

      expectedFirst = 3;
      expectedMatch = 11;
      bAllPassed &= testwildfind(
         (uint8_t *) "⊃ḪؿꜪἪꜿ", 
         (uint8_t *) "Ḫ?Ꜫ*ꜿ", 
         /* lenTame = */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃ḪؿꜪἪꜿ"), 
         /* lenWild = */ !len ? len : CodePointCountUtf8((uint8_t *) "*?ؿꜪ*ꜿ"), 
         expectedFirst, expectedLast, expectedMatch, expectedTarget, 
         /* bCase = */ TRUE);
   } while (!len++);

   if (bAllPassed)
   {
      printf("Passed targeted wildcard search tests with UTF-8 content\n");
   }
   else
   {
      printf("Failed targeted wildcard search tests with UTF-8 content\n");
   }
   
   return bAllPassed;
}

// Tests for UTF-8 tokenset search functions.
//
int testset_findtoken(void)
{
   size_t expectedToken;
   int len = 0;               // Rely on null string terminators.
   int bAllPassed = TRUE;

   do
   {
      expectedToken = 4;
      bAllPassed &= testfindtoken(
         (uint8_t *) "what,do,we,do,with,a,comma-separated,list?", (uint8_t *) ";,",
         /* lenContent =  */ !len ? len : CodePointCountUtf8((uint8_t *) "what,do,we,do,with,a,comma-separated,list?"),
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) ";,"),
         expectedToken);

      expectedToken = 20;
      bAllPassed &= testfindtoken(
         (uint8_t *) "Back on Times Square, Dreaming of Times Square", (uint8_t *) ",",
         /* lenContent =  */ !len ? len : CodePointCountUtf8((uint8_t *) "Back on Times Square, Dreaming of Times Square"),
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) ","),
         expectedToken);

      expectedToken = 10;
      bAllPassed &= testfindtoken(
         (uint8_t *) ";;Separate;this.;;", (uint8_t* ) ";,",
         /* lenContent =  */ !len ? len : CodePointCountUtf8((uint8_t *) ";;Separate;this.;;"),
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) ";,"),
         expectedToken);

      expectedToken = g_noMatch;
      bAllPassed &= testfindtoken(
         (uint8_t *) "S", (uint8_t *) ";,",
         /* lenContent =  */ !len ? len : CodePointCountUtf8((uint8_t *) "S"),
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) ";,"),
         expectedToken);

      expectedToken = 8;
      bAllPassed &= testfindtoken(
         (uint8_t *) "⊃Ḫؿ,ꜪἪꜿ", (uint8_t *) ";,", 
         /* lenContent =  */ !len ? len : CodePointCountUtf8((uint8_t *) "⊃Ḫؿ,ꜪἪꜿ"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) ";,"), 
         expectedToken);

      expectedToken = 17;
      bAllPassed &= testfindtoken(
         (uint8_t *) "🎶☕️🢇miS;sissippi", (uint8_t *) ";,", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "🢇miSsissippi"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) ";,"), 
         expectedToken);

      expectedToken = 55;
      bAllPassed &= testfindtoken(
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺ😍ᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿ😍ᚭᛦ ᚺᛟᛋᛚᛃ", (uint8_t *) "😍", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺ😍ᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿ😍ᚭᛦ ᚺᛟᛋᛚᛃ"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍"), 
         expectedToken);

      expectedToken = 15;
      bAllPassed &= testfindtoken(
         (uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺ😍ᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿ😍ᚭᛦ ᚺᛟᛋᛚᛃ", (uint8_t *) "😍 ", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "ᛋᚭᚷᚹᛗ (ᛗ)ᛟᚷᛗᛖᚿᛃ (ᚦ)ᚭᛞ ᚺ😍ᛟᚭᛦ ᛃᚷᛟᛚᛞ ᚷᚭ ᛟᚭᛦᛃ ᚷᛟᛚᛞᛃᚿ ᛞ ᚷᛟᚭᚿ😍ᚭᛦ ᚺᛟᛋᛚᛃ"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "😍 "), 
         expectedToken);

      expectedToken = g_noMatch;
      bAllPassed &= testfindtoken(
         (uint8_t *) "", (uint8_t *) "a", 
         /* lenContent = */ 0, /* lenTokenSet = */ !len ? len : 1, 
         expectedToken);

      // Initial matching tokens are bypassed.
      expectedToken = g_noMatch;
      bAllPassed &= testfindtoken(
         (uint8_t *) "ab", (uint8_t *) "ab", 
         /* lenContent = */ !len ? len : 2, /* lenTokenSet = */ !len ? len : 2,
         expectedToken);

      expectedToken = 3;
      bAllPassed &= testfindtoken(
         (uint8_t *) "ab b", (uint8_t *) "ab", 
         /* lenContent = */ !len ? len : 4, /* lenTokenSet = */ !len ? len : 2,
         expectedToken);

      expectedToken = 36;
      bAllPassed &= testfindtoken(
         (uint8_t *) "من يضحك أخيرا يضحك كثير", (uint8_t *) "ث", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "من يضحك أخيرا يضحك كثير"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "ث"), 
         expectedToken);

      expectedToken = 5;
      bAllPassed &= testfindtoken(
         (uint8_t *) "𐤖 𐤟 𐤚 𐤟 𐤛 𐤟 𐤗 𐤟 𐤘 𐤟 𐤙", (uint8_t *) "𐤟", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐤖 𐤟 𐤚 𐤟 𐤛 𐤟 𐤗 𐤟 𐤘 𐤟 𐤙"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐤟"), 
         expectedToken);

      expectedToken = 4;
      bAllPassed &= testfindtoken(
         (uint8_t *) "what᛫do᛫we᛫do᛫with᛫a᛫runic-separated᛫list?", (uint8_t *) "?᛫", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "what᛫do᛫we᛫do᛫with᛫a᛫runic-separated᛫list?"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "᛫"), 
         expectedToken);

      expectedToken = 7;
      bAllPassed &= testfindtoken(
         (uint8_t *) "Կաթ ✶ հաց ✶ պանիր ✶ ձու", (uint8_t *) "✶", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "Կաթ ✶ հաց ✶ պանիր ✶ ձու"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "✶"), 
         expectedToken);

      expectedToken = 32;
      bAllPassed &= testfindtoken(
         (uint8_t *) "𐓘𐒰𐓆𐒻𐓘𐒰𐓆𐒻⳿𐒼𐒰𐓄𐒷⳿𐓄𐒰𐓇𐒻", (uint8_t *) "⳿", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "𐓘𐒰𐓆𐒻𐓘𐒰𐓆𐒻⳿𐒼𐒰𐓄𐒷⳿𐓄𐒰𐓇𐒻"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "⳿"), 
         expectedToken);

      expectedToken = 27;
      bAllPassed &= testfindtoken(
         (uint8_t *) "𞥞Γῆ🜃흙𞥞Ἀήρ🜁공기𞥞Πῦρ🜂불𞥞Ὕδωρ🜄물𞥞", (uint8_t *) "🜁", 
         /* lenContent = */ !len ? len : CodePointCountUtf8((uint8_t *) "𞥞Γῆ🜃흙𞥞Ἀήρ🜁공기𞥞Πῦρ🜂불𞥞Ὕδωρ🜄물𞥞"), 
         /* lenTokenSet = */ !len ? len : CodePointCountUtf8((uint8_t *) "🜁"), 
         expectedToken);
   } while (!len++);

   if (bAllPassed)
   {
      printf("Passed tokenset search tests\n");
   }
   else
   {
      printf("Failed tokenset search tests\n");
   }

   return bAllPassed;
}

//
// Tests for families of functions are combined, so that timing rollups for 
// performance comparisons can happen after all ASCII and UTF-8 testing has 
// been completed for each family as a whole.  The code above prints the 
// pass/fail results.  The code below code prints the timings, if any.
//
void ClearAccumulatedTimes(void)
{
   // Accumulated times are cleared before each family of tests is run.
   g_uModeA_AccumulatedTimeAscii = g_uModeA_AccumulatedTimeLenAscii = 
     g_uModeB_AccumulatedTimeAscii = g_uModeB_AccumulatedTimeLenAscii = 
       g_uModeA_AccumulatedTimeUtf8 = g_uModeB_AccumulatedTimeUtf8 = 
         g_uModeA_AccumulatedTimeLenUtf8 = g_uModeB_AccumulatedTimeLenUtf8 = 0;

   // Clear function call counts too.
   g_iModeA_CallsUtf8vAscii = g_iModeB_CallsUtf8vAscii = 
     g_iModeA_CallsLenUtf8vLenAscii = g_iModeB_CallsLenUtf8vLenAscii = 0;

   return;
}

// This set of tests validates UTF-8 content and converts 8-bit ASCII text to 
// equivalent UTF-8 content.
//
int validateandconvert_testswithrollup(void)
{
   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();

      // These tests don't invoke any third-party module for UTF-8 
      // validation or conversion.  For performance comparison, one or more 
      // such calls could be added.  Printing of rolled-up timing results 
      // can then be similar to what's done in the functions below.
   }

   return testset_validateandconvert();
}

// This is a family of tests for case-insensitive code point conversion, 
// useful for content matching, combined with a set of tests for UTF-8 
// content replication functions.  Performance comparisons include 
// *CopyUtf8() vs. str*cpy() (Mode A) and *DuplicateUtf8() vs. str*dup() 
// (Mode B) for tests that collectively pass an identical bunch of ASCII 
// strings to each of these functions.
//
int foldcopyandduplicate_testswithrollup(void)
{
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();
      g_bAccumulateFunctionTimes = TRUE;
   }

   // ASCII-only tests.
   bAllPassed &= testset_foldcopyandduplicate_ascii();

   if (g_bComparePerformance)
   {
      g_bAccumulateFunctionTimes = FALSE;
   }

   // UTF-8 content duplication tests.
   bAllPassed &= testset_foldcopyandduplicate_utf8();

   if (g_uModeA_AccumulatedTimeAscii)
   {
      // Function call counts and their timings and have been accumulated via 
      // file-scope data.
      const double fBase = 10.0;
      const double fExpNanoseconds = 9.0;
      const double fExpMilliseconds = 3.0;

      // Represent the timings in seconds, to millisecond precision.
      // Mode A findings are for *CopyUtf8() vs. str*cpy().
      // Mode B findings are for *DuplicateUtf8() vs. str*dup().
      double fTimeCumulativeAsciiCopy = 
            ((double) (g_uModeA_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenAsciiCopy = 
            ((double) (g_uModeA_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeAsciiDuplicate = 
            ((double) (g_uModeB_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenAsciiDuplicate = 
            ((double) (g_uModeB_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeUtf8Copy = 
            ((double) (g_uModeA_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8Copy = 
            ((double) (g_uModeA_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeUtf8Duplicate = 
            ((double) (g_uModeB_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8Duplicate = 
            ((double) (g_uModeB_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      // Can set up similar calculations for more performance comparisons.

      float fAsciiCopyTimeInSeconds = (float) (fTimeCumulativeAsciiCopy / 1000);
      float fAsciiLenCopyTimeInSeconds = (float) (fTimeCumulativeLenAsciiCopy / 1000);
      float fAsciiDuplicateTimeInSeconds = (float) (fTimeCumulativeAsciiDuplicate / 1000);
      float fAsciiLenDuplicateTimeInSeconds = (float) (fTimeCumulativeLenAsciiDuplicate / 1000);
      float fUtf8CopyTimeInSeconds = (float) (fTimeCumulativeUtf8Copy / 1000);
      float fUtf8LenCopyTimeInSeconds = (float) (fTimeCumulativeLenUtf8Copy / 1000);
      float fUtf8DuplicateTimeInSeconds = (float) (fTimeCumulativeUtf8Duplicate / 1000);
      float fUtf8LenDuplicateTimeInSeconds = (float) (fTimeCumulativeLenUtf8Duplicate / 1000);

      // Show the timing results.
      printf(
          "\nTiming results for CopyUtf8() v strcpy() (%d calls)\n", 
            g_iModeA_CallsUtf8vAscii);
      printf(
         "strcpy() - for ASCII strings: %.3f seconds\n",
              fAsciiCopyTimeInSeconds);
      printf(
          "CopyUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8CopyTimeInSeconds);

      printf(
          "\nTiming results for DuplicateUtf8() v strdup() (%d calls)\n", 
            g_iModeB_CallsUtf8vAscii);
      printf(
         "strdup() - for ASCII strings: %.3f seconds\n",
              fAsciiDuplicateTimeInSeconds);
      printf(
          "DuplicateUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8DuplicateTimeInSeconds);

      printf(
          "\nTiming results for LenCopyUtf8() v strncpy() (%d calls)\n", 
            g_iModeA_CallsLenUtf8vLenAscii);
      printf(
         "strncpy() - for ASCII strings: %.3f seconds\n",
              fAsciiLenCopyTimeInSeconds);         
      printf(
          "CopyUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8LenCopyTimeInSeconds);

      printf(
          "\nTiming results for LenDuplicateUtf8() v strdup() (%d calls)\n", 
            g_iModeB_CallsLenUtf8vLenAscii);
      printf(
         "strdup() - for ASCII strings: %.3f seconds\n",
              fAsciiLenDuplicateTimeInSeconds);         
      printf(
          "LenDuplicateUtf8() - for UTF-8-encoded content: %.3f seconds\n\n",
              fUtf8LenDuplicateTimeInSeconds);
   }

   return bAllPassed;
}

// This is a family of combined separation (tokenization), concatenation, and 
// slicing / substring tests.  Performance comparisons involve tests against 
// SeparateAscii(), which is implemented for those platforms that don't 
// provide such a function for ASCII strings.
//
// For performance comparison, Mode A accumulates findings for... 
//
//  SeparateUtf8() + TrimUtf8() 
//     vs. 
//        SeparateAscii() + TrimAscii()  [ASCII implementation]
//
// ...which is a pair typically called together as a separate=and-trim step in 
// many real-world scenarios.  Mode B accumulates findings for... 
//
//  *ConcatenateUtf8() vs. str*cat()
//
// ...where both modes involve tests that collectively pass an identical bunch 
// of ASCII strings to each of these functions.
//
int separateconcatenateandslice_testswithrollup(void)
{
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();
      g_bAccumulateFunctionTimes = TRUE;
   }

   // ASCII-only tests.
   bAllPassed &= testset_separateconcatenateandslice_ascii();

   if (g_bComparePerformance)
   {
      g_bAccumulateFunctionTimes = FALSE;
   }

   // UTF-8 whole content separation, concatenation, and slicing tests.
   bAllPassed &= testset_separateconcatenateandslice_utf8();

   if (g_uModeA_AccumulatedTimeAscii)
   {
      // Function call counts and their timings and have been accumulated via 
      // file-scope data.
      const double fBase = 10.0;
      const double fExpNanoseconds = 9.0;
      const double fExpMilliseconds = 3.0;

      // Represent the timings in seconds, to millisecond precision.
      // Mode A findings are for *CopyUtf8() vs. str*cpy().
      // Mode B findings are for *DuplicateUtf8() vs. str*dup().
      double fTimeCumulativeAsciiSeparate = 
            ((double) (g_uModeA_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeAsciiConcatenate = 
            ((double) (g_uModeB_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenAsciiConcatenate = 
            ((double) (g_uModeB_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeUtf8Separate = 
            ((double) (g_uModeA_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeUtf8Concatenate = 
            ((double) (g_uModeB_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8Concatenate = 
            ((double) (g_uModeB_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      // Can set up similar calculations for more performance comparisons.

      float fAsciiSeparateTimeInSeconds = (float) (fTimeCumulativeAsciiSeparate / 1000);
      float fAsciiConcatenateTimeInSeconds = (float) (fTimeCumulativeAsciiConcatenate / 1000);
      float fAsciiLenConcatenateTimeInSeconds = (float) (fTimeCumulativeLenAsciiConcatenate / 1000);
      float fUtf8SeparateTimeInSeconds = (float) (fTimeCumulativeUtf8Separate / 1000);
      float fUtf8ConcatenateTimeInSeconds = (float) (fTimeCumulativeUtf8Concatenate / 1000);
      float fUtf8LenConcatenateTimeInSeconds = (float) (fTimeCumulativeLenUtf8Concatenate / 1000);

      // Show the timing results.
      printf(
          "\nTiming results for SeparateUtf8() + TrimUtf8() v SeparateAscii() + TrimAscii() (%d calls)\n", 
            g_iModeA_CallsUtf8vAscii);
      printf(
         "SeparateAscii() + TrimAscii() - for ASCII strings: %.3f seconds\n",
              fAsciiSeparateTimeInSeconds);
      printf(
          "SeparateUtf8() + TrimUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8SeparateTimeInSeconds);

      printf(
          "\nTiming results for ConcatenateUtf8() v strcat() (%d calls)\n", 
            g_iModeB_CallsUtf8vAscii);
      printf(
         "strcat() - for ASCII strings: %.3f seconds\n",
              fAsciiConcatenateTimeInSeconds);
      printf(
          "ConcatenateUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8ConcatenateTimeInSeconds);

      printf(
          "\nTiming results for LenConcatenateUtf8() v strncat() (%d calls)\n", 
            g_iModeB_CallsLenUtf8vLenAscii);
      printf(
         "strncat() - for ASCII strings: %.3f seconds\n",
              fAsciiLenConcatenateTimeInSeconds);         
      printf(
          "LenConcatenateUtf8() - for UTF-8-encoded content: %.3f seconds\n\n",
              fUtf8LenConcatenateTimeInSeconds);
   }

   return bAllPassed;
}

// Combined whole-content comparison tests.  Performance comparisons include 
// case-sensitive (Mode A) and case-insensitive (Mode B) tests.
//
int compare_testswithrollup(void)
{
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();
      g_bAccumulateFunctionTimes = TRUE;
   }

   // ASCII-only tests with actual content.
   bAllPassed &= testset_compare_ascii();

   // ASCII-only tests with one or both inbound strings empty.
   bAllPassed &= testset_compare_empty();

   if (g_bComparePerformance)
   {
      g_bAccumulateFunctionTimes = FALSE;
   }

   // UTF-8 whole content comparison tests.
   bAllPassed &= testset_compare_utf8();

   if (g_uModeA_AccumulatedTimeAscii)
   {
      // Function call counts and their timings and have been accumulated via 
      // file-scope data.
      const double fBase = 10.0;
      const double fExpNanoseconds = 9.0;
      const double fExpMilliseconds = 3.0;

      // Represent the timings in seconds, to millisecond precision.
      double fTimeCumulativeAscii = 
            ((double) (g_uModeA_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeCaseAscii = 
            ((double) (g_uModeB_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenAscii = 
            ((double) (g_uModeA_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenCaseAscii = 
            ((double) (g_uModeB_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);            
      double fTimeCumulativeUtf8 = 
            ((double) (g_uModeA_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeCaseUtf8 = 
            ((double) (g_uModeB_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8 = 
            ((double) (g_uModeA_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenCaseUtf8 = 
            ((double) (g_uModeB_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);            
      // Can set up similar calculations for more performance comparisons.

      float fAsciiTimeInSeconds = (float) (fTimeCumulativeAscii / 1000);
      float fAsciiCaseTimeInSeconds = (float) (fTimeCumulativeCaseAscii / 1000);
      float fAsciiLenTimeInSeconds = (float) (fTimeCumulativeLenAscii / 1000);
      float fAsciiLenCaseTimeInSeconds = (float) (fTimeCumulativeLenCaseAscii / 1000);
      float fUtf8TimeInSeconds = (float) (fTimeCumulativeUtf8 / 1000);
      float fUtf8CaseTimeInSeconds = (float) (fTimeCumulativeCaseUtf8 / 1000);
      float fUtf8LenTimeInSeconds = (float) (fTimeCumulativeLenUtf8 / 1000);
      float fUtf8LenCaseTimeInSeconds = (float) (fTimeCumulativeLenCaseUtf8 / 1000);

      // Show the timing results.
      printf(
          "\nTiming results for CompareUtf8() v strcmp() (%d calls)\n", 
            g_iModeA_CallsUtf8vAscii);
      printf(
        "strcmp() - for ASCII strings: %.3f seconds\n",
              fAsciiTimeInSeconds);
      printf(
         "CompareUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8TimeInSeconds);

      printf(
          "\nTiming results for CaseCompareUtf8() v strcasecmp() (%d calls)\n", 
            g_iModeB_CallsUtf8vAscii);
      printf(
         "strcasecmp() - for ASCII strings: %.3f seconds\n",
              fAsciiCaseTimeInSeconds);         
      printf(
          "CaseCompareUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8CaseTimeInSeconds);

      printf(
          "\nTiming results for LenCompareUtf8() v strncmp() (%d calls)\n", 
            g_iModeA_CallsLenUtf8vLenAscii);
      printf(
          "strncmp() - for ASCII strings: %.3f seconds\n",
              fAsciiLenTimeInSeconds);
      printf(
          "LenCompareUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8LenTimeInSeconds);

      printf(
          "\nTiming results for LenCaseCompareUtf8() v strncasecmp() (%d calls)\n", 
            g_iModeB_CallsLenUtf8vLenAscii);
      printf(
          "strncasecmp() - for ASCII strings: %.3f seconds\n",
              fAsciiLenCaseTimeInSeconds);               
      printf(
          "LenCaseCompareUtf8() - for UTF-8-encoded content: %.3f seconds\n\n",
              fUtf8LenCaseTimeInSeconds);
   }

   return bAllPassed;
}

// Combined partial-content comparison tests.  Performance comparisons 
// include case-sensitive (Mode A) and case-insensitive (Mode B) tests.
//
int slicecompare_testswithrollup(void)
{
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();
      g_bAccumulateFunctionTimes = TRUE;
   }

   // ASCII-only tests with actual content.
   bAllPassed &= testset_find_ascii();

   // ASCII-only tests with one or both inbound strings empty.
   bAllPassed &= testset_find_empty();

   if (g_bComparePerformance)
   {
      g_bAccumulateFunctionTimes = FALSE;
   }

   // UTF-8 whole content comparison tests.
   bAllPassed &= testset_find_utf8();

   if (g_uModeA_AccumulatedTimeAscii)
   {
      // Function call counts and their timings and have been accumulated via 
      // file-scope data.
      const double fBase = 10.0;
      const double fExpNanoseconds = 9.0;
      const double fExpMilliseconds = 3.0;

      // Represent the timings in seconds, to millisecond precision.
      double fTimeCumulativeAscii = 
            ((double) (g_uModeA_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeCaseAscii = 
            ((double) (g_uModeB_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenAscii = 
            ((double) (g_uModeA_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenCaseAscii = 
            ((double) (g_uModeB_AccumulatedTimeLenAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);            
      double fTimeCumulativeUtf8 = 
            ((double) (g_uModeA_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeCaseUtf8 = 
            ((double) (g_uModeB_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8 = 
            ((double) (g_uModeA_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenCaseUtf8 = 
            ((double) (g_uModeB_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);            
      // Can set up similar calculations for more performance comparisons.

      float fAsciiTimeInSeconds = (float) (fTimeCumulativeAscii / 1000);
      float fAsciiCaseTimeInSeconds = (float) (fTimeCumulativeCaseAscii / 1000);
      float fAsciiLenTimeInSeconds = (float) (fTimeCumulativeLenAscii / 1000);
      float fAsciiLenCaseTimeInSeconds = (float) (fTimeCumulativeLenCaseAscii / 1000);
      float fUtf8TimeInSeconds = (float) (fTimeCumulativeUtf8 / 1000);
      float fUtf8CaseTimeInSeconds = (float) (fTimeCumulativeCaseUtf8 / 1000);
      float fUtf8LenTimeInSeconds = (float) (fTimeCumulativeLenUtf8 / 1000);
      float fUtf8LenCaseTimeInSeconds = (float) (fTimeCumulativeLenCaseUtf8 / 1000);

      // Show the timing results.
      printf(
          "\nTiming results for FindUtf8() v strstr() (%d calls)\n", 
            g_iModeA_CallsUtf8vAscii);
      printf(
         "strstr() - for ASCII strings: %.3f seconds\n",
              fAsciiTimeInSeconds);
      printf(
          "FindUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8TimeInSeconds);

      printf(
          "\nTiming results for CaseFindUtf8() v strcasestr() (%d calls)\n", 
            g_iModeB_CallsUtf8vAscii);
      printf(
          "strcasestr() - for ASCII strings: %.3f seconds\n",
              fAsciiCaseTimeInSeconds);         
      printf(
          "CaseFindUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8CaseTimeInSeconds);

      printf(
          "\nTiming results for LenFindUtf8() v strnstr() (%d calls)\n", 
            g_iModeA_CallsLenUtf8vLenAscii);
      printf(
          "strnstr() - for ASCII strings: %.3f seconds\n",
              fAsciiLenTimeInSeconds);
      printf(
          "LenFindUtf8() - for UTF-8-encoded content: %.3f seconds\n",
              fUtf8LenTimeInSeconds);

      printf(
          "\nTiming results for LenCaseFindUtf8() v LenFindAscii() (%d calls)\n", 
            g_iModeB_CallsLenUtf8vLenAscii);
      printf(
          "LenFindAscii() - for ASCII strings: %.3f seconds\n",
              fAsciiLenCaseTimeInSeconds);               
      printf(
          "LenCaseFindUtf8() - for UTF-8-encoded content: %.3f seconds\n\n",
              fUtf8LenCaseTimeInSeconds);
   }

   return bAllPassed;
}

// Combined tests for UTF-8 matching wildcards functions.
//
int wildcompare_testswithrollup(void)
{
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();
      g_bAccumulateFunctionTimes = TRUE;
   }

   // ASCII-only tests for all-tame scenarios.
   bAllPassed &= testset_wildcompare_tame();

   // ASCII-only tests for empty-content scenarios.
   bAllPassed &= testset_wildcompare_empty();

   // ASCII-only tests for matching wildcards, including actual wildcards.
   bAllPassed &= testset_wildcompare_wild();

   if (g_bComparePerformance)
   {
      g_bAccumulateFunctionTimes = FALSE;
   }

   // UTF-8 tests for matching wildcards, including actual wildcards.
   bAllPassed &= testset_wildcompare_utf8();

   if (g_uModeA_AccumulatedTimeAscii)
   {
      // Timings have been accumulated via file-scope data.
      const double fBase = 10.0;
      const double fExpNanoseconds = 9.0;
      const double fExpMilliseconds = 3.0;

      // Represent the timings in seconds, to millisecond precision.
      double fTimeCumulativeAsciiVersion = 
            ((double) (g_uModeA_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeUtf8Version = 
            ((double) (g_uModeA_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8Version = 
            ((double) (g_uModeA_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      // Can set up similar calculations for more performance comparisons.

      float fAsciiVersionTimeInSeconds = (float) (fTimeCumulativeAsciiVersion / 1000);
      float fUtf8VersionTimeInSeconds = (float) (fTimeCumulativeUtf8Version / 1000);
      float fUtf8LenVersionTimeInSeconds = (float) (fTimeCumulativeLenUtf8Version / 1000);

      // Show the timing results.
      printf(
          "\nTiming results for Wild*CompareUtf8() v FastWildCompare() (%d calls)\n", 
            g_iModeA_CallsUtf8vAscii);
      printf(
          "FastWildCompare() - for ASCII strings: %.3f seconds\n",
              fAsciiVersionTimeInSeconds);
      printf(
          "Wild[Case]CompareUtf8() - for UTF-8-encoded strings: %.3f seconds\n",
              fUtf8VersionTimeInSeconds);
      printf(
          "WildLen[Case]CompareUtf8() - for UTF-8-encoded strings: %.3f seconds\n",
              fUtf8LenVersionTimeInSeconds);
   }

   return bAllPassed;
}

// Tests for UTF-8 content search / targeted wildcard search functions.
//
int targetedsearch_testswithrollup(void)
{
   int bAllPassed = TRUE;

   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();
      g_bAccumulateFunctionTimes = TRUE;
   }

   // Tests for all-tame or single-wildcard scenarios.
   bAllPassed &= testset_targetedsearch_tame();

   // ASCII-only tests for empty-content scenarios.
   bAllPassed &= testset_targetedsearch_empty();

   // ASCII-only tests for matching wildcards, including targeted terms.
   bAllPassed &= testset_targetedsearch_latin();

   if (g_bComparePerformance)
   {
      g_bAccumulateFunctionTimes = FALSE;
   }

   // UTF-8 tests for matching wildcards, including targeted terms.
   bAllPassed &= testset_targetedsearch_global();

   if (g_uModeA_AccumulatedTimeAscii)
   {
      // Timings have been accumulated via file-scope data.
      const double fBase = 10.0;
      const double fExpNanoseconds = 9.0;
      const double fExpMilliseconds = 3.0;

      // Represent the timings in seconds, to millisecond precision.
      double fTimeCumulativeAsciiVersion = 
            ((double) (g_uModeA_AccumulatedTimeAscii) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeUtf8Version = 
            ((double) (g_uModeA_AccumulatedTimeUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      double fTimeCumulativeLenUtf8Version = 
            ((double) (g_uModeA_AccumulatedTimeLenUtf8) /
                POW(fBase, fExpNanoseconds)) * POW(fBase, fExpMilliseconds);
      // Can set up similar calculations for more performance comparisons.

      float fUtf8VersionTimeInSeconds = (float) (fTimeCumulativeUtf8Version / 1000);
      float fUtf8LenVersionTimeInSeconds = (float) (fTimeCumulativeLenUtf8Version / 1000);

      // Show the timing results.
      printf(
          "\nTiming results for Wild*FindUtf8() (%d calls)\n", 
            g_iModeA_CallsUtf8vAscii);
      printf(
          "Wild[Case]FindUtf8() - for UTF-8-encoded strings: %.3f seconds\n",
              fUtf8VersionTimeInSeconds);
      printf(
          "WildLen[Case]FindUtf8() - for UTF-8-encoded strings: %.3f seconds\n",
              fUtf8LenVersionTimeInSeconds);
   }

   return bAllPassed;
}

// Tests for UTF-8 tokenset search functions.
//
int tokensetsearch_testswithrollup(void)
{
   if (g_bComparePerformance)
   {
      ClearAccumulatedTimes();

      // These tests don't invoke any third-party module for UTF-8 
      // validation or conversion.  For performance comparison, one or more 
      // such calls could be added.  Printing of rolled-up timing results 
      // can then be similar to what's done in the functions below.
   }

   return testset_findtoken();
}

// Entry point for tests.
//
int main(int argc, char **argv)
{
   int bAllPassed = TRUE;

   // Initialize sets of mappings for case folding.  The mappings are used 
   // for case-insensitive matching.
   CaseMappingSetupUtf8();

   // Get performance comparison results against standard library ASCII 
   // string functions?
   if (argc > 1 && (argv[1][0] == 'w' || 
                    argv[1][0] == 'p' || 
                    argv[1][0] == 'c'))
   {
      g_bComparePerformance = TRUE;
      g_iTestRepetitions = 0;

      // How many times to repeat tests, for function time accumulation?
      if (argc > 2)
      {
         g_iTestRepetitions = atoi(argv[2]);
      }

      if (!g_iTestRepetitions)
      {
         g_iTestRepetitions = 1000000;
      }
   }   
   else
   {
      g_bComparePerformance = FALSE;
   }

   // This flag enables collection of timing data.  It's set only for those 
   // tests that pass in ASCII strings, for performance comparison.
   g_bAccumulateFunctionTimes = FALSE;

   // Tests for UTF-8 validation and fallback functions.
   bAllPassed &= validateandconvert_testswithrollup();

   // Tests for case-insensitive code point conversion, useful for content 
   // matching, combined with tests for UTF-8 content replication functions.
   bAllPassed &= foldcopyandduplicate_testswithrollup();

   // Tests for UTF-8 content separation (tokenization), concatenation, and 
   // substring-like functions.
   bAllPassed &= separateconcatenateandslice_testswithrollup();

   // Tests for whole UTF-8 content comparison functions.
   bAllPassed &= compare_testswithrollup();

   // Tests for partial UTF-8 content comparison functions.
   bAllPassed &= slicecompare_testswithrollup();

   // Tests for UTF-8 matching wildcards functions.
   bAllPassed &= wildcompare_testswithrollup();

   // Tests for UTF-8 targeted wildcard search functions.
   bAllPassed &= targetedsearch_testswithrollup();

   // Tests for UTF-8 tokenset search functions.
   bAllPassed &= tokensetsearch_testswithrollup();

   if (bAllPassed)
   {
      printf("\nAll correctness tests passed.\n");
   }
 
   return bAllPassed ? 0 : 1;
}
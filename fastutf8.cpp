// UTF-8-ready C/C++ routines.
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

#if defined(__cplusplus)
#include <cstdlib>           // For memory management
#include <cstring>           // For char *
#include <optional>          // For std::optional
#include <iterator>
#include <cctype>            // For compatibility with C-style isspace()
#include <iostream>          // For cout

#else   // !__cplusplus
#include <stdlib.h>          // For memory management
#include <ctype.h>           // For isspace()
#endif  // !__cplusplus

#include "fastutf8.h"
#include "casemappings.h"

// The following values are set according to the UTF-8 encoding standard 
// described at
//
//     https://en.wikipedia.org/wiki/UTF-8#Description
//
// Effectively, the number of bytes after a sequence of leading 1's, at  
// the start of a code point, is limited to the maximum value of that 
// first byte, given that the leading 1's are followed by a 0, followed 
// by further 1's to complete the byte.
//
#define HALF_ASCII_LIMIT  0x7F   // 0nnnnnnn  (an entire 1-byte code point)
#define SINGLETON_LIMIT   0xBF   // 10nnnnnn  (an intra-code-point byte)
#define TWOFER_LIMIT      0xDF   // 110nnnnn  (first of a 2-byte code point)
#define THREESOME_LIMIT   0xEF   // 1110nnnn  (first of a 3-byte code point)

//#define FREE_INVALID_CONTENT 0x1  // Deallocate invalid UTF-8 blocks

// These C-style functions grovel over UTF-8 code points.  Within the FastUtf8 
// namespace, several of them are invoked by the Uniseries iterator.  They 
// also underlie many of the C-style *Utf8() functions, coded in this file, 
// that are callable either directly or via the Uniseries methods.
//

// Given a pointer to a UTF-8 code point, advances it to any next UTF-8 code 
// point.  Returns true if there is a further code point, or false if the 
// next content is a terminating null.  PERFORMS NO UTF-8 VALIDATION OTHER
// THAN NULL CHECKING.
//
inline bool CodePointAdvanceUtf8(const uint8_t **ppContent)
{
   *ppContent += (**ppContent > 0) +
       ((**ppContent > SINGLETON_LIMIT) && *(1 + *ppContent)) + 
       ((**ppContent > TWOFER_LIMIT) && *(2 + *ppContent)) + 
       ((**ppContent > THREESOME_LIMIT) && *(3 + *ppContent));
   return (bool) **ppContent;
}

// Given a pointer to a UTF-8 code point and a pointer to the beginning of 
// the series that contains it, backtracks the first pointer to any code 
// point immediately "below" it in memory.  Returns true if such a previous 
// code point exists at or "above" the beginning of the series, or false 
// otherwise.  This logic is all exercised in testset_targetedsearch_latin() 
// [testutf8.cpp].
//
inline bool CodePointBacktrackUtf8(const uint8_t **ppContent, 
                      const uint8_t *pContentStart)
{
   if ((*ppContent - 1) >= pContentStart && 
       *(*ppContent - 1) <= HALF_ASCII_LIMIT)
   {
      *ppContent -= 1;
	  return true;
   }
   else if ((*ppContent - 2) >= pContentStart && 
            *(*ppContent - 2) > SINGLETON_LIMIT)
   {
      *ppContent -= 2;
	  return true;
   }
   else if ((*ppContent - 3) >= pContentStart && 
             *(*ppContent - 3) > SINGLETON_LIMIT)
   {
      *ppContent -= 3;
	  return true;
   }
   else if ((*ppContent - 4) >= pContentStart)
   {
      *ppContent -= 4;
	  return true;
   }

   return false;
}

// Compares two UTF-8 code points.  Returns true if the code points are 
// identical.  Returns false otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline bool CodePointCompareUtf8(const uint8_t *pContentA, 
                      const uint8_t *pContentB)
{
   if (*pContentA != *pContentB)
   {
      return false;
   }
   else if (*pContentA > SINGLETON_LIMIT &&
            *(1 + pContentA) != *(1 + pContentB))
   {
      return false;
   }
   else if (*pContentA > TWOFER_LIMIT &&
            *(2 + pContentA) != *(2 + pContentB))
   {
      return false;
   }
   else if (*pContentA > THREESOME_LIMIT &&
            *(3 + pContentA) != *(3 + pContentB))
   {
      return false;
   }

   return true;
}

// Compares two UTF-8 code points.  Advances the second pointer to any next 
// UTF-8 code point.  Returns true if the code points are identical.  Returns 
// false otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline bool CodePointAdvanceAndCompareUtf8(
                      const uint8_t *pContentA, const uint8_t **ppContentB)
{
   // Advance the second pointer.
   *ppContentB += (**ppContentB > 0) +
       ((**ppContentB > SINGLETON_LIMIT) && *(1 + *ppContentB)) + 
       ((**ppContentB > TWOFER_LIMIT) && *(2 + *ppContentB)) + 
       ((**ppContentB > THREESOME_LIMIT) && *(3 + *ppContentB));

   // Compare the code points.
   if (*pContentA != **ppContentB)
   {
      return false;
   }
   else if (*pContentA > SINGLETON_LIMIT &&
      *(1 + pContentA) != *(1 + *ppContentB))
   {
      return false;
   }
   else if (*pContentA > TWOFER_LIMIT &&
      *(2 + pContentA) != *(2 + *ppContentB))
   {
      return false;
   }
   else if (*pContentA > THREESOME_LIMIT &&
      *(3 + pContentA) != *(3 + *ppContentB))
   {
      return false;
   }

   return true;
}

// Compares two UTF-8 code points.  Returns true if the code points are 
// identical after case folding.  Returns false otherwise.  PERFORMS NO 
// UTF-8 VALIDATION.
//
inline bool CodePointCaseCompareUtf8(
                      const uint8_t *pContentA, const uint8_t *pContentB)
{
   uint32_t nFoldedA, nFoldedB;  // Folded code points.
   uint16_t nMapping;            // Array selector for 0xF09nnnnn range.

   // Have we got half-ASCII code points?  Map to lowercase and compare.
   if (*pContentA <= HALF_ASCII_LIMIT && *pContentB <= HALF_ASCII_LIMIT)
   {
      return CM08[*pContentA] == CM08[*pContentB];
   }

   // Either of the code points may comprise multiple bytes.  Get them into 
   // local variables for lookup.
   if (*pContentA > TWOFER_LIMIT)
   {
      if (*pContentA > THREESOME_LIMIT)
      {
         nMapping = (uint16_t) (*(1 + pContentA) & 0xFF) << 4 | 
                               (*(2 + pContentA) & 0xF0) >> 4;

         // If the code point falls in a range for which we have a mapping, 
         // then fold based on that mapping.
         if (nMapping < 0x91A)
         {
            if (nMapping == 0x909)       // 0xF0909nnn range.
            {
               nFoldedA = CM909[((*(2 + pContentA) & 0xF) << 8) | 
                                ((*(3 + pContentA) & 0xFF))];
            }
            else if (nMapping == 0x90B)  // 0xF090Bnnn range.
            {
               nFoldedA = CM90B[((*(2 + pContentA) & 0xF) << 8) | 
                                ((*(3 + pContentA) & 0xFF))];
            }
            else
            {
               nFoldedA = (uint32_t) (*pContentA << 24) | 
                          (uint32_t) (*(1 + pContentA) << 16) | 
                          (uint32_t) (*(2 + pContentA) << 8) | 
                          (uint32_t) *(3 + pContentA);
            }
         }
         else if (nMapping > 0x91A)
         {
            if (nMapping == 0x96B)       // 0xF096Bnnn range.
            {
               nFoldedA = CM96B[((*(2 + pContentA) & 0xF) << 8) | 
                                         ((*(3 + pContentA) & 0xFF))];
            }
            else if (nMapping == 0x9EA)  // 0xF09EAnnn range.
            {
               nFoldedA = CM9EA[((*(2 + pContentA) & 0xF) << 8) | 
                                ((*(3 + pContentA) & 0xFF))];
            }
            else
            {
               nFoldedA = (uint32_t) (*pContentA << 24) | 
                          (uint32_t) (*(1 + pContentA) << 16) | 
                          (uint32_t) (*(2 + pContentA) << 8) | 
                          (uint32_t) *(3 + pContentA);
            }
         }
         else                             // 0xF091Annn range.
         {
            nFoldedA = CM91A[((*(2 + pContentA) & 0xF) << 8) | 
                             ((*(3 + pContentA) & 0xFF))];
         }
      }
      else
      {
         // Look up the code point in the mappings for the 0xEnnnnn range.
         nFoldedA = CMEnn[((*pContentA & 0x0F) << 16) | 
                          ((*(1 + pContentA) & 0xFF) << 8) | 
                          ((*(2 + pContentA) & 0xFF))];
      }
   }
   else if (*pContentA > SINGLETON_LIMIT)
   {
      // Two of the two-byte code points map to three-byte code points.
      if (*pContentA == 0xC8 && *(1 + pContentA) == 0xBA)
      {
         nFoldedA = 0xE2B1A5;
      }
      else if (*pContentA == 0xC8 && *(1 + pContentA) == 0xBE)
      {
         nFoldedA = 0xE2B1A5;
      }
      else
      {
         nFoldedA = CM00n[((*pContentA & 0x1F) << 8) | 
                          ((*(1 + pContentA) & 0xFF))];
      }
   }
   else
   {
      nFoldedA = CM08[*pContentA];
   }

   if (*pContentB > TWOFER_LIMIT)
   {
      if (*pContentB > THREESOME_LIMIT)
      {
         nMapping = (uint16_t) (*(1 + pContentB) & 0xFF) << 4 | 
                               (*(2 + pContentB) & 0xF0) >> 4;

         // If the code point falls in a range for which we have a mapping, 
         // then fold based on that mapping.
         if (nMapping < 0x91A)
         {
            if (nMapping == 0x909)
            {
               nFoldedB = CM909[((*(2 + pContentB) & 0xF) << 8) | 
                                ((*(3 + pContentB) & 0xFF))];
            }
            else if (nMapping == 0x90B)
            {
               nFoldedB = CM90B[((*(2 + pContentB) & 0xF) << 8) | 
                                ((*(3 + pContentB) & 0xFF))];
            }
            else
            {
               nFoldedB = (uint32_t) (*pContentB << 24) | 
                          (uint32_t) (*(1 + pContentB) << 16) | 
                          (uint32_t) (*(2 + pContentB) << 8) | 
                          (uint32_t) *(3 + pContentB);
            }
         }
         else if (nMapping > 0x91A)
         {
            if (nMapping == 0x96B)
            {
               nFoldedB = CM96B[((*(2 + pContentB) & 0xF) << 8) | 
                                ((*(3 + pContentB) & 0xFF))];
            }
            else if (nMapping == 0x9EA)
            {
               nFoldedB = CM9EA[((*(2 + pContentB) & 0xF) << 8) | 
                                ((*(3 + pContentB) & 0xFF))];
            }
            else
            {
               nFoldedB = (uint32_t) (*pContentB << 24) | 
                          (uint32_t) (*(1 + pContentB) << 16) | 
                          (uint32_t) (*(2 + pContentB) << 8) | 
                          (uint32_t) *(3 + pContentB);
            }
         }
         else
         {
            nFoldedB = CM91A[((*(2 + pContentB) & 0xF) << 8) | 
                             ((*(3 + pContentB) & 0xFF))];
         }
      }
      else
      {
         // Look up the code point in the mappings for the 0xEnnnnn range.
         nFoldedB = CMEnn[((*pContentB & 0x0F) << 16) | 
                          ((*(1 + pContentB) & 0xFF) << 8) | 
                          ((*(2 + pContentB) & 0xFF))];
      }
   }
   else if (*pContentB > SINGLETON_LIMIT)
   {
      // Two of the two-byte code points map to three-byte code points.
      if (*pContentB == 0xC8 && *(1 + pContentB) == 0xBA)
      {
         nFoldedB = 0xE2B1A5;
      }
      else if (*pContentB == 0xC8 && *(1 + pContentB) == 0xBE)
      {
         nFoldedB = 0xE2B1A5;
      }
      else
      {
         nFoldedB = CM00n[((*pContentB & 0x1F) << 8) | 
                          ((*(1 + pContentB) & 0xFF))];
      }
   }
   else
   {
      nFoldedB = CM08[*pContentB];
   }

   return nFoldedA == nFoldedB;
}

// Compares two UTF-8 code points after advancing the second pointer to any 
// next UTF-8 code point.  Returns true if the folded code points are 
// identical.  Returns false otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline bool CodePointAdvanceAndCaseCompareUtf8(
                      const uint8_t *pContentA, const uint8_t **ppContentB)
{
   uint32_t nFoldedA, nFoldedB;  // Folded code points.
   uint16_t nMapping;            // Array selector for 0xF09nnnnn range.

   // Advance the second pointer.
   *ppContentB += (**ppContentB > 0) +
       ((**ppContentB > SINGLETON_LIMIT) && *(1 + *ppContentB)) + 
       ((**ppContentB > TWOFER_LIMIT) && *(2 + *ppContentB)) + 
       ((**ppContentB > THREESOME_LIMIT) && *(3 + *ppContentB));

   // Have we got half-ASCII code points?  Map to lowercase and compare.
   if (*pContentA <= HALF_ASCII_LIMIT && 
       *(uint8_t *) *ppContentB <= HALF_ASCII_LIMIT)
   {
      return CM08[*pContentA] == CM08[**ppContentB];
   }

   // Either of the code points may comprise multiple bytes.  Get them into 
   // local variables for lookup.
   if (*pContentA > TWOFER_LIMIT)
   {
      if (*pContentA > THREESOME_LIMIT)
      {
         nMapping = (uint16_t) (*(1 + pContentA) & 0xFF) << 4 | 
                               (*(2 + pContentA) & 0xF0) >> 4;

         // If the code point falls in a range for which we have a mapping, 
         // then fold based on that mapping.
         if (nMapping < 0x91A)
         {
            if (nMapping == 0x909)       // 0xF0909nnn range.
            {
               nFoldedA = CM909[((*(2 + pContentA) & 0xF) << 8) | 
                                ((*(3 + pContentA) & 0xFF))];
            }
            else if (nMapping == 0x90B)  // 0xF090Bnnn range.
            {
               nFoldedA = CM90B[((*(2 + pContentA) & 0xF) << 8) | 
                                ((*(3 + pContentA) & 0xFF))];
            }
            else
            {
               nFoldedA = (uint32_t) (*pContentA << 24) | 
                          (uint32_t) (*(1 + pContentA) << 16) | 
                          (uint32_t) (*(2 + pContentA) << 8) | 
                          (uint32_t) *(3 + pContentA);
            }
         }
         else if (nMapping > 0x91A)
         {
            if (nMapping == 0x96B)       // 0xF096Bnnn range.
            {
               nFoldedA = CM96B[((*(2 + pContentA) & 0xF) << 8) | 
                                         ((*(3 + pContentA) & 0xFF))];
            }
            else if (nMapping == 0x9EA)  // 0xF09EAnnn range.
            {
               nFoldedA = CM9EA[((*(2 + pContentA) & 0xF) << 8) | 
                                ((*(3 + pContentA) & 0xFF))];
            }
            else
            {
               nFoldedA = (uint32_t) (*pContentA << 24) | 
                          (uint32_t) (*(1 + pContentA) << 16) | 
                          (uint32_t) (*(2 + pContentA) << 8) | 
                          (uint32_t) *(3 + pContentA);
            }
         }
         else                             // 0xF091Annn range.
         {
            nFoldedA = CM91A[((*(2 + pContentA) & 0xF) << 8) | 
                             ((*(3 + pContentA) & 0xFF))];
         }
      }
      else
      {
         // Look up the code point in the mappings for the 0xEnnnnn range.
         nFoldedA = CMEnn[((*pContentA & 0x0F) << 16) | 
                          ((*(1 + pContentA) & 0xFF) << 8) | 
                          ((*(2 + pContentA) & 0xFF))];
      }
   }
   else if (*pContentA > SINGLETON_LIMIT)
   {
      // Two of the two-byte code points map to three-byte code points.
      if (*pContentA == 0xC8 && *(1 + pContentA) == 0xBA)
      {
         nFoldedA = 0xE2B1A5;
      }
      else if (*pContentA == 0xC8 && *(1 + pContentA) == 0xBE)
      {
         nFoldedA = 0xE2B1A5;
      }
      else
      {
         nFoldedA = CM00n[((*pContentA & 0x1F) << 8) | 
                          ((*(1 + pContentA) & 0xFF))];
      }
   }
   else
   {
      nFoldedA = CM08[*pContentA];
   }

   if (*(uint8_t *) *ppContentB > TWOFER_LIMIT)
   {
      if (*(uint8_t *) *ppContentB > THREESOME_LIMIT)
      {
         nMapping = (uint16_t) (*(1 + *ppContentB) & 0xFF) << 4 | 
                               (*(2 + *ppContentB) & 0xF0) >> 4;

         // If the code point falls in a range for which we have a mapping, 
         // then fold based on that mapping.
         if (nMapping < 0x91A)
         {
            if (nMapping == 0x909)
            {
               nFoldedB = CM909[((*(2 + *ppContentB) & 0xF) << 8) | 
                                ((*(3 + *ppContentB) & 0xFF))];
            }
            else if (nMapping == 0x90B)
            {
               nFoldedB = CM90B[((*(2 + *ppContentB) & 0xF) << 8) | 
                                ((*(3 + *ppContentB) & 0xFF))];
            }
            else
            {
               nFoldedB = (uint32_t) (**ppContentB << 24) | 
                          (uint32_t) (*(1 + *ppContentB) << 16) | 
                          (uint32_t) (*(2 + *ppContentB) << 8) | 
                          (uint32_t) *(3 + *ppContentB);
            }
         }
         else if (nMapping > 0x91A)
         {
            if (nMapping == 0x96B)
            {
               nFoldedB = CM96B[((*(2 + *ppContentB) & 0xF) << 8) | 
                                ((*(3 + *ppContentB) & 0xFF))];
            }
            else if (nMapping == 0x9EA)
            {
               nFoldedB = CM9EA[((*(2 + *ppContentB) & 0xF) << 8) | 
                                ((*(3 + *ppContentB) & 0xFF))];
            }
            else
            {
               nFoldedB = (uint32_t) (**ppContentB << 24) | 
                          (uint32_t) (*(1 + *ppContentB) << 16) | 
                          (uint32_t) (*(2 + *ppContentB) << 8) | 
                          (uint32_t) *(3 + *ppContentB);
            }
         }
         else
         {
            nFoldedB = CM91A[((*(2 + *ppContentB) & 0xF) << 8) | 
                             ((*(3 + *ppContentB) & 0xFF))];
         }
      }
      else
      {
         // Look up the code point in the mappings for the 0xEnnnnn range.
         nFoldedB = CMEnn[((**ppContentB & 0x0F) << 16) | 
                          ((*(1 + *ppContentB) & 0xFF) << 8) | 
                          ((*(2 + *ppContentB) & 0xFF))];
      }
   }
   else if (*(uint8_t *) *ppContentB > SINGLETON_LIMIT)
   {
      // Two of the two-byte code points map to three-byte code points.
      if (**ppContentB == 0xC8 && *(1 + *ppContentB) == 0xBA)
      {
         nFoldedB = 0xE2B1A5;
      }
      else if (**ppContentB == 0xC8 && *(1 + *ppContentB) == 0xBE)
      {
         nFoldedB = 0xE2B1A5;
      }
      else
      {
         nFoldedB = CM00n[((**ppContentB & 0x1F) << 8) | 
                          ((*(1 + *ppContentB) & 0xFF))];
      }
   }
   else
   {
      nFoldedB = CM08[**ppContentB];
   }

   return nFoldedA == nFoldedB;
}

// One or another of these validation functions may be best invoked no more 
// than once for a given series of UTF-8 content.  Besides null checks, the 
// other *Utf8() functions don't perform any UTF-8 validation.  In case 
// content fails validation, there's probably no ideal general-purpose next 
// step, but calling either of the next functions here...
//
//    Convert8BitAsciiToUtf8()
//    LenConvert8BitAsciiToUtf8()
//
// ...may qualify as a best practice.
//
// Why?  To replace any invalid code points with artifically-selected valid 
// code points, and to continue from there as if nothing had been changed, 
// would turn potentially uncorrupted data into definitely corrupted data.  
// No solution seems more reasonable than to treat the invalid code points as 
// though valid 8-bit ASCII has made its way into a series, and to convert it 
// to UTF-8 content so that it can be displayed as such with no chance of 
// further data corruption.

// Counts the number of contiguous valid code points in the given null-
// terminated content, starting from the beginning of the content.  Returns 
// true if every code point prior to the terminating null is valid.  Returns 
// false otherwise.
//
bool ValidateUtf8(
            const uint8_t *pContent,       // Content to validate
            int           *piCount)        // Returned code point count
{
   *piCount = 0;

   while (*pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         (*piCount)++;
         pContent++;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         if (*pContent > TWOFER_LIMIT)
         {
            if (*pContent > THREESOME_LIMIT)
            {
               if (*(pContent + 3) <= HALF_ASCII_LIMIT || 
                   *(pContent + 3) > SINGLETON_LIMIT)
               {
                  return false;
               }

               (*piCount)++;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return false;
            }

            (*piCount)++;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return false;
         }

         (*piCount)++;
         pContent += 2;
      }
      else
      {
         return false;
      }
   }

   return true;
}

// Validates the given content, up to the specified number of code points, 
// starting from the beginning of the content.  Returns true if as many code 
// points are valid.  Returns false otherwise.
//
bool LenValidateUtf8(
            const uint8_t *pContent,       // Content to validate
            int           lenContent)      // Code point count (specified)
{
   int iContent = 0;

   while (iContent < lenContent && *pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++iContent;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         if (*pContent > TWOFER_LIMIT)
         {
            if (*pContent > THREESOME_LIMIT)
            {
               if (*(pContent + 3) <= HALF_ASCII_LIMIT || 
                   *(pContent + 3) > SINGLETON_LIMIT)
               {
                  return false;
               }

               ++iContent;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return false;
            }

            ++iContent;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return false;
         }

         ++iContent;
         pContent += 2;
      }
      else
      {
         return false;
      }
   }

   return true;
}

// Validates the given content, up to a terminating null, starting from the 
// beginning of the content.  Counts the number of contiguous valid code 
// points.  Sets the pbIs7BitCharString flag if every code point represents a 
// 7-bit ASCII character.  Returns the number of bytes in the content, if the 
// code points are valid.  Returns zero otherwise.  This function and the one 
// that follows it are useful for constructing an object of a class that can 
// handle ASCII character strings optimally and that also can handle UTF-8 
// content.
//
size_t ValidateWithIs7BitUtf8(
            const uint8_t *pContent,       // Content to validate
            int  *piCount,                 // Returned code point count
            bool *pbIs7BitCharString)      // Returned 7-bit ASCII flag
{
   const uint8_t *pContentOrig = pContent;

   *piCount = 0;
   *pbIs7BitCharString = true;

   while (*pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         (*piCount)++;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         *pbIs7BitCharString = false;

         if (*pContent > TWOFER_LIMIT)
         {
            if (*pContent > THREESOME_LIMIT)
            {
               if (*(pContent + 3) <= HALF_ASCII_LIMIT || 
                   *(pContent + 3) > SINGLETON_LIMIT)
               {
                  return 0;
               }

               (*piCount)++;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return 0;
            }

            (*piCount)++;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return 0;
         }

         (*piCount)++;
         pContent += 2;
      }
      else
      {
         *pbIs7BitCharString = false;
         return 0;
      }
   }

   return pContent - pContentOrig;
}

// Validates the given content, up to the specified number of code points, 
// starting from the beginning of the content.  Returns the number of bytes in 
// the content, if the code points are valid.  Returns zero otherwise.  Sets 
// the pbIs7BitCharString flag if every code point represents a 7-bit ASCII 
// character.
//
size_t LenValidateWithIs7BitUtf8(
            const uint8_t *pContent,       // Content to validate
            int  lenContent,               // Code point count (specified)
            bool *pbIs7BitCharString)      // Returned 7-bit ASCII flag
{
   const uint8_t *pContentOrig = pContent;
   int     iContent = 0;

   *pbIs7BitCharString = true;

   while (iContent < lenContent && *pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++iContent;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         *pbIs7BitCharString = false;

         if (*pContent > TWOFER_LIMIT)
         {
            if (*pContent > THREESOME_LIMIT)
            {
               if (*(pContent + 3) <= HALF_ASCII_LIMIT || 
                   *(pContent + 3) > SINGLETON_LIMIT)
               {
                  return 0;
               }

               ++iContent;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return 0;
            }

            ++iContent;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return 0;
         }

         ++iContent;
         pContent += 2;
      }
      else
      {
         *pbIs7BitCharString = false;
         return 0;
      }
   }

   return pContent - pContentOrig;
}

// Validates the given content, up to the specified number of bytes.  Returns 
// the number of code points in the content, if the code points are valid.  
// Returns zero otherwise.  Sets the pbIs7BitCharString flag if every code 
// point represents a 7-bit ASCII character.
//
int SizeValidateWithIs7BitUtf8(
            const uint8_t *pContent,        // Beginning of content to validate
            const uint8_t *pLast,           // End of content to validate
            bool *pbIs7BitCharString)       // Returned 7-bit ASCII flag
{
   int iContent = 0;

   *pbIs7BitCharString = true;

   while (pContent < pLast && *pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++iContent;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         *pbIs7BitCharString = false;

         if (*pContent > TWOFER_LIMIT)
         {
            if (*pContent > THREESOME_LIMIT)
            {
               if (*(pContent + 3) <= HALF_ASCII_LIMIT || 
                   *(pContent + 3) > SINGLETON_LIMIT)
               {
                  return 0;
               }

               ++iContent;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return 0;
            }

            ++iContent;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return 0;
         }

         ++iContent;
         pContent += 2;
      }
      else
      {
         *pbIs7BitCharString = false;
         return 0;
      }
   }

   return iContent;
}

// Given null-terminated content comprising what may be an 8-bit ASCII string, 
// allocates a buffer sufficient for the equivalent UTF-8 content and places 
// that content in it.  If FREE_INVALID_CONTENT is set, deallocates the block 
// containing the 8-bit ASCII string.  Returns a pointer to the new  buffer, 
// or nullptr if the content comprises only 7-bit ASCII characters.
//
// The developer is responsible for ensuring that the allocated buffer for 
// UTF-8 content is deallocated via free(), once it is no longer in use.
//
uint8_t * Convert8BitAsciiToUtf8(
            const char *pContent,     // Content to convert
            int  *lenContent)         // Returned length (in code points)
{
   uint8_t       *pUtf8Base;
   uint8_t       *pUtf8 = nullptr;
   bool          bGot8BitAscii = false;
   size_t        sizeNeeded = 0;
   unsigned char *pAscii = (unsigned char *) pContent;

   while (*pAscii)
   {
      if (*pAscii > HALF_ASCII_LIMIT)
      {
         if (!bGot8BitAscii)
         {
            bGot8BitAscii = true;
         }

         sizeNeeded += 2;
      }
      else
      {
         ++sizeNeeded;
      }

      ++pAscii;
   }

   if (bGot8BitAscii)
   {
      pUtf8 = pUtf8Base = (uint8_t *) malloc(1 + sizeNeeded);
   }
   else
   {
      pUtf8Base = nullptr;
   }

   if (pUtf8)
   {
      pAscii = (unsigned char *) pContent;

      while (*pAscii)
      {
         if (*pAscii <= HALF_ASCII_LIMIT)
         {
            *pUtf8++ = *pAscii;
         }
         else if (*pAscii <= SINGLETON_LIMIT)
         {
            *pUtf8++ = 0xC2;          // An additional byte follows.
            *pUtf8++ = *pAscii;
         }
         else
         {
            *pUtf8++ = 0xC3;          // Per UTF-8 mappings.
            *pUtf8++ = *pAscii - 0x40;
         }

         ++pAscii;
      }

      *pUtf8 = 0;

#if defined(FREE_INVALID_CONTENT)
      free(pContent);
#endif
   }

   *lenContent = (int) (pAscii - (unsigned char *) pContent);
   return pUtf8Base;
}

// Given an 8-bit ASCII string and its size in bytes, allocates a buffer 
// sufficient for the equivalent UTF-8 content and places that content in it.
// If FREE_INVALID_CONTENT is set, deallocates the block containing the 8-bit 
// ASCII string.  Returns a pointer to the new buffer, or nullptr if the 
// content comprises only 7-bit ASCII characters.
//
// The developer is responsible for ensuring that the allocated buffer for 
// UTF-8 content is deallocated via free(), once it is no longer in use.
//
uint8_t * LenConvert8BitAsciiToUtf8(
            const char *pContent,          // Content to convert
            size_t sizeContent)            // Content size (bytes)
{
   uint8_t *pUtf8Base;
   uint8_t *pUtf8 = nullptr;
   bool    bGot8BitAscii = false;
   size_t  sizeAscii = 0;
   size_t  sizeNeeded = 0;
   unsigned char *pAscii = (unsigned char *) pContent;

   while (sizeAscii < sizeContent && *pAscii)
   {
      if (*pAscii > HALF_ASCII_LIMIT)
      {
         if (!bGot8BitAscii)
         {
            bGot8BitAscii = true;
         }

         sizeNeeded += 2;
      }
      else
      {
         ++sizeNeeded;
      }

      ++pAscii;
      ++sizeAscii;
   }

   if (bGot8BitAscii)
   {
      pUtf8 = pUtf8Base = (uint8_t *) malloc(1 + sizeNeeded);
   }
   else
   {
      pUtf8Base = nullptr;
   }

   if (pUtf8)
   {
      sizeAscii = 0;
      pAscii = (unsigned char *) pContent;

      while (sizeAscii < sizeContent && *pAscii)
      {
         if (*pAscii <= HALF_ASCII_LIMIT)
         {
            *pUtf8++ = *pAscii;
         }
         else if (*pAscii <= SINGLETON_LIMIT)
         {
            *pUtf8++ = 0xC2;          // An additional byte follows.
            *pUtf8++ = *pAscii;
         }
         else
         {
            *pUtf8++ = 0xC3;          // Per UTF-8 mappings.
            *pUtf8++ = *pAscii - 0x40;
         }

         ++pAscii;
         ++sizeAscii;
      }

      *pUtf8 = 0;

#if defined(FREE_INVALID_CONTENT)
      free(pContent);
#endif
   }

   return pUtf8Base;
}

// Given a buffer containing null-terminated UTF-8 content, anticipates the 
// size of its folded equivalent, in bytes.  Anticipates 4 bytes for the 
// noncharacter 0xFFFFFFFF in place of any code-point-sized content that is 
// not valid UTF-8.  DOES NOT CHECK FOR INVALID POINTERS. 
//
size_t SizeOfFoldedUtf8(
            const uint8_t *pContent)       // Content to evaluate
{
   size_t sizeFolded;           // Index for content as individual bytes

   if (!pContent)
   {
      return 0;
   }

   sizeFolded = 0;

   // Determine the case-folded size of a code point at a time.
   do
   {
      // Have we got a 7-bit code point?  Anticipate a lowercase mapping.
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++sizeFolded;

         if (!*pContent)
         {
            return sizeFolded;
         }

		 ++pContent;
      }
      else if (*pContent > TWOFER_LIMIT)
      {
         // The destination code point will comprise multiple bytes.
         if (*pContent > THREESOME_LIMIT)
         {
            sizeFolded += 4;
            pContent += 4;
         }
         else
         {
            sizeFolded += 3;
            pContent += 3;
         }
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         // Two of the two-byte code points map to three-byte code points.
         if (*pContent == 0xC8 && *(1 + pContent) == 0xBA)
         {
            sizeFolded += 3;
         }
         else if (*pContent == 0xC8 && *(1 + pContent) == 0xBE)
         {
            sizeFolded += 3;
         }
         else
         {
            sizeFolded += 2;
         }

         pContent += 2;
      }
      else
      {
         int iInvalidBytes = 4;
         sizeFolded += 4;

         while (iInvalidBytes--)
         {
            if (*(++pContent) < SINGLETON_LIMIT)
            {
              break;
            }
	     }
      }
   } while (*pContent);

   sizeFolded++;  // Anticipate one more byte for the terminating null.
   return sizeFolded;
}

// Given a buffer containing UTF-8 content and a number of code points in the 
// content, anticipates the size of the content's folded equivalent, in bytes. 
// Anticipates 4 bytes for the noncharacter 0xFFFFFFFF in place of any code-
// point-sized content that is not valid UTF-8.  DOES NOT CHECK FOR INVALID 
// POINTERS. 
//
size_t SizeOfFoldedLenUtf8(
            const uint8_t *pContent,       // Content to evaluate
            int           lenContent)      // Code point count
{
   size_t sizeFolded;           // Index for content as individual bytes
   int    iContent;             // Index for content

   if (!pContent)
   {
      return 0;
   }

   sizeFolded = 0;
   iContent = 0;

   // Determine the case-folded size of a code point at a time.
   do
   {
      // Have we got a 7-bit code point?  Anticipate a lowercase mapping.
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++sizeFolded;

         if (!*pContent)
         {
            return sizeFolded;
         }

		 ++pContent;
      }
      else if (*pContent > TWOFER_LIMIT)
      {
         // The destination code point will comprise multiple bytes.
         if (*pContent > THREESOME_LIMIT)
         {
            sizeFolded += 4;
            pContent += 4;
         }
         else
         {
            sizeFolded += 3;
            pContent += 3;
         }
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         // Two of the two-byte code points map to three-byte code points.
         if (*pContent == 0xC8 && *(1 + pContent) == 0xBA)
         {
            sizeFolded += 3;
         }
         else if (*pContent == 0xC8 && *(1 + pContent) == 0xBE)
         {
            sizeFolded += 3;
         }
         else
         {
            sizeFolded += 2;
         }

         pContent += 2;
      }
      else
      {
         int iInvalidBytes = 4;
         sizeFolded += 4;

         while (iInvalidBytes--)
         {
            if (*(++pContent) < SINGLETON_LIMIT)
            {
              break;
            }
         }
      }
   } while (lenContent > ++iContent && *pContent);

   ++sizeFolded;  // Anticipate one more byte for the terminating null.
   return sizeFolded;
}

// Given a source buffer containing UTF-8 content, places its folded 
// equivalent into the given destination buffer, up to the specified number 
// of bytes.  Places the noncharacter 0xFFFFFFFF into the destination buffer 
// in place of any code-point-sized source content that is not valid UTF-8.  
// For Latin, Greek, and most other symbol sets that embody the uppercase and 
// lowercase concept, acts as an iterative tolower() function for UTF-8.  
// DOES NOT CHECK FOR BUFFER OVERFLOW, BUFFER OVERLAP, OR INVALID POINTERS. 
//
// Note: The folded content may occupy fewer or more bytes than the original 
//       content.  A sufficient destination buffer can be allocated based on 
//       an advance call to SizeOfFoldedUtf8() or to SizeOfFoldedLenUtf8().
//
uint8_t * ToFoldedUtf8(
            uint8_t *pDestination,         // Outbound buffer
            const uint8_t *pSource,        // Inbound buffer
            size_t sizeDestination)        // Size of outbound buffer (bytes)
{
   uint8_t *pDestinationOrig;        // Initial pointer to outbound buffer
   size_t  sizePlaced;               // Index for content as individual bytes

   if (!pSource || !pDestination)
   {
      return nullptr;
   }

   pDestinationOrig = pDestination;
   sizePlaced = 0;

   // Convert a code point at a time.
   do
   {
      // Have we got a 7-bit code point?  Map to lowercase.
      if (*pSource <= HALF_ASCII_LIMIT)
      {
         *pDestination = CM08[*pSource];

         if (!*pSource)
         {
            return pDestinationOrig;
         }

		 ++pSource;
		 ++pDestination;
         ++sizePlaced;
      }
      else
      {
         if (*pSource > TWOFER_LIMIT)
         {
            // The destination code point will comprise multiple bytes.
            if (*pSource > THREESOME_LIMIT)
            {
               uint32_t nFolded = 0;
               uint16_t nMapping;      // Array selector for 0xF09nnnnn range.

               nMapping = (uint16_t) (*(1 + pSource) & 0xFF) << 4 | 
                                     (*(2 + pSource) & 0xF0) >> 4;

               // If the code point falls in a range for which we have a mapping, 
               // then fold based on that mapping.
               if (nMapping < 0x91A)
               {
                  if (nMapping == 0x909)       // 0xF0909nnn range.
                  {
                     nFolded = CM909[((*(2 + pSource) & 0xF) << 8) | 
                                        ((*(3 + pSource) & 0xFF))];
                  }
                  else if (nMapping == 0x90B)  // 0xF090Bnnn range.
                  {
                     nFolded = CM90B[((*(2 + pSource) & 0xF) << 8) | 
                                        ((*(3 + pSource) & 0xFF))];
                  }
               }
               else if (nMapping > 0x91A)
               {
                  if (nMapping == 0x96B)       // 0xF096Bnnn range.
                  {
                     nFolded = CM96B[((*(2 + pSource) & 0xF) << 8) | 
                                        ((*(3 + pSource) & 0xFF))];
                  }
                  else if (nMapping == 0x9EA)  // 0xF09EAnnn range.
                  {
                     nFolded = CM9EA[((*(2 + pSource) & 0xF) << 8) | 
                                        ((*(3 + pSource) & 0xFF))];
                  }
               }
               else                            // 0xF091Annn range.
               {
                  nFolded = CM9EA[((*(2 + pSource) & 0xF) << 8) | 
                                        ((*(3 + pSource) & 0xFF))];
               }

               if (nFolded)
               {
                  *pDestination++ = (uint8_t) ((nFolded & 0xFF000000) >> 24);
                  *pDestination++ = (uint8_t) ((nFolded & 0x00FF0000) >> 16);
                  *pDestination++ = (uint8_t) ((nFolded & 0x0000FF00) >> 8);
                  *pDestination++ = (uint8_t) (nFolded & 0x00FF);
                  pSource += 4;
               }
               else
               {
                   for (size_t sizeToCopy = 4; sizeToCopy--;)
                   {
                      *pDestination++ = *pSource++;
                   }
               }

               sizePlaced += 4;
            }
            else
            {
               // Look up the code point in the mappings for the 0xEnnnnn range.
               uint32_t nFolded = CMEnn[((*pSource & 0x0F) << 16) | 
                                        ((*(1 + pSource) & 0xFF) << 8) | 
                                        ((*(2 + pSource) & 0xFF))];
               *pDestination++ = (uint8_t) ((nFolded & 0x00FF0000) >> 16);
               *pDestination++ = (uint8_t) ((nFolded & 0x0000FF00) >> 8);
               *pDestination++ = (uint8_t) (nFolded & 0x000000FF);
               sizePlaced += 3;
               pSource += 3;
            }
         }
         else if (*pSource > SINGLETON_LIMIT)
         {
            // Two of the two-byte code points map to three-byte code points.
            if (*pSource == 0xC8 && *(1 + pSource) == 0xBA)
            {
               *pDestination++ = 0xE2;
               *pDestination++ = 0xB1;
               *pDestination++ = 0xA5;
               sizePlaced += 3;
            }
            else if (*pSource == 0xC8 && *(1 + pSource) == 0xBE)
            {
               *pDestination++ = 0xE2;
               *pDestination++ = 0xB1;
               *pDestination++ = 0xA6;
               sizePlaced += 3;
            }
            else
            {
               // Look up the code point in the two-byte mappings.
               uint16_t nFolded = CM00n[((*pSource & 0x1F) << 8) | 
                                        ((*(1 + pSource) & 0xFF))];

               if (nFolded & 0xFF00)
               {
                  *pDestination++ = (uint8_t) ((nFolded & 0xFF00) >> 8);
                  *pDestination++ = (uint8_t) (nFolded & 0x00FF);
                  sizePlaced += 2;
               }
               else
               {
                  *pDestination++ = (uint8_t) (nFolded & 0x00FF);
                  sizePlaced += 1;
               }
            }

            pSource += 2;
         }
         else
         {
            // Place a 4-byte noncharacter in the outbound buffer.
            for (size_t sizeToPlace = 4; sizeToPlace--;)
            {
               *pDestination++ = 0xFF;
            
               if (*pSource > SINGLETON_LIMIT)
               {
                  ++pSource;
               }
            }

            sizePlaced += 4;
         }
      }
   } while (sizePlaced < sizeDestination && *pSource);

   *pDestination = 0;
   return pDestinationOrig;
}

// Given null-terminated UTF-8 content, returns the number of code points 
// in it.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
int CodePointCountUtf8(
            const uint8_t *pContent)       // Content to evaluate
{
   const uint8_t *pCodePointInContent = (uint8_t *) pContent;
   int iContent = pContent ? *(uint8_t *) pCodePointInContent > 0 : 0;

   if (pContent)
   {
      // Walk the code points.
      while (CodePointAdvanceUtf8(&pCodePointInContent))
      {
         ++iContent;
      }
   }

   return iContent;
}

// Given null-terminated UTF-8 content, returns the number of bytes in it, 
// similar to strlen().  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL 
// CHECKING.
//
size_t SizeOfUtf8(
            const uint8_t *pContent)       // Content to evaluate
{
   const uint8_t *pContentOrig;

   if (!pContent)                          // Got empty input?
   {
      return 0;
   }
   else if (!*pContent)
   {
      return 0;
   }
   else
   {
      pContentOrig = pContent;

      // Walk the code points.
      while (CodePointAdvanceUtf8(&pContent))
      {
         // Cumulative bytes are counted at the end.
      }

      return (pContent - pContentOrig);
   }
}

// Given UTF-8 content and a count of the code points in it, returns the 
// number of bytes in it.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL 
// CHECKING.
//
size_t SizeOfLenUtf8(
            const uint8_t *pContent,       // Content to evaluate
            int lenContent)                // Code point count
{
   const uint8_t *pContentOrig;
   int           iContent;

   if (!pContent)                          // Got empty input?
   {
      return 0;
   }
   else if (!*pContent)
   {
      return 0;
   }
   else
   {
      pContentOrig = pContent;
      iContent = 0;

      // Walk the code points.
      while (iContent++ < lenContent && 
                      CodePointAdvanceUtf8(&pContent))
      {
         // Cumulative bytes are counted at the end.
      }

      return (pContent - pContentOrig);
   }
}

// Given a byte range comprising UTF-8 content, returns the number of code 
// points in the range.  Returns -1 if the range does not begin or end at byte 
// values consistent with valid code point boundaries.  PERFORMS NO OTHER 
// POINTER VALIDATION AND NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
int LenSizeOfUtf8(
            const uint8_t *pContent,       // Content to evaluate
            size_t        sizeContent)     // Size (bytes)
{
   uint8_t *pPrevCodePointInContent;
   int     iContent;
   size_t  nSize;

   // Verify that a valid range was given and that its beginning and end 
   // aren't flagged as intra-code-point bytes.
   if (!pContent)
   {
      iContent = -1;
   }
   else if (!*pContent)
   {
      iContent = 0;
   }
   else if (*pContent > HALF_ASCII_LIMIT && *pContent <= SINGLETON_LIMIT)
   {
      iContent = -1;
   }
   else if (*(uint8_t *) (pContent + sizeContent) > HALF_ASCII_LIMIT && 
       *(uint8_t *) (pContent + sizeContent) <= SINGLETON_LIMIT)
   {
      iContent = -1;
   }
   else
   {
      pPrevCodePointInContent = (uint8_t *) pContent;
      iContent = 0;
      nSize = 0;

      // Walk the range, counting the code points in it.
      while (nSize < sizeContent && 
                      CodePointAdvanceUtf8(&pContent))
      {
         nSize += pContent - pPrevCodePointInContent;
         pPrevCodePointInContent = (uint8_t *) pContent;
         ++iContent;
      }
   }

   return iContent;
}

// Given null-terminated UTF-8 content, determines whether it comprises 
// entirely 7-bit "half ASCII" characters, which would make it compatible 
// with ordinary C/C++ string routines.  Returns true for a 7-bit ASCII 
// string, and false otherwise.
//
bool Is7BitUtf8(
            uint8_t *pContent)             // Content to evaluate
{
   do
   {
      if (*pContent > HALF_ASCII_LIMIT)
      {
         return false;
      }
   } while (*pContent++);

   return true;
}

// Given UTF-8 content and its length in code points, determines whether it 
// comprises entirely 7-bit "half ASCII" characters.  Returns true for a 
// 7-bit ASCII string, and false otherwise.
//
bool IsLen7BitUtf8(
            uint8_t *pContent,             // Content to evaluate
            int lenContent)                // Code point count
{
   int iContent = 0;                       // Index for content

   do
   {
      if (*pContent > HALF_ASCII_LIMIT)
      {
         return false;
      }
   } while (lenContent > ++iContent && *pContent++);

   return true;
}

// Copies UTF-8 (or any) null-terminated content to the given destination 
// buffer from the given source buffer.  DOES NOT CHECK FOR BUFFER OVERFLOW, 
// BUFFER OVERLAP, OR INVALID POINTERS.  PERFORMS NO UTF-8 VALIDATION.
//
uint8_t * CopyUtf8(
            uint8_t *pDestination,         // Buffer
            const uint8_t *pSource)        // Content to copy
{
   uint8_t *pDestinationOrig = pDestination;

   if (pDestination && pSource && *pSource)
   {
      do
      {
         *pDestination++ = *pSource;  // Do byte-wise copy.
      } while (*pSource++);
   }
  
   return pDestinationOrig;
}

// Copies UTF-8 content to the given destination buffer from the given source 
// buffer, up to the specified number of code points.  DOES NOT CHECK FOR 
// BUFFER OVERFLOW, BUFFER OVERLAP, OR INVALID POINTERS.  PERFORMS NO UTF-8 
// VALIDATION OTHER THAN NULL CHECKING.
//
uint8_t * LenCopyUtf8(
            uint8_t *pDestination,         // Buffer
            const uint8_t *pSource,        // Content to copy
            int lenContent)                // Code point count
{
   int iContent = 0;                       // Index for content
   uint8_t *pDestinationOrig = pDestination;

   if (pDestination && pSource && *pSource)
   {
      do
      {
         if (*pSource > HALF_ASCII_LIMIT)
         {
            if (*pSource <= TWOFER_LIMIT)
            {
               // Copy two bytes, checking for embedded nulls.
               *pDestination++ = *pSource++;
     
               if (*pSource)
               {
                  *pDestination++ = *pSource++;
               }
               else
               {
                  break;
               }
            }
            else
            {
               if (*pSource <= THREESOME_LIMIT)
               {
                  // Copy three bytes, checking for embedded nulls.
                  *pDestination++ = *pSource++;
     
                  if (*pSource)
                  {
                     *pDestination++ = *pSource++;
     
                     if (*pSource)
                     {
                        *pDestination++ = *pSource++;
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
               else
               {
                  // Copy four bytes, checking for embedded nulls.
                  *pDestination++ = *pSource++;
     
                  if (*pSource)
                  {
                     *pDestination++ = *pSource++;
     
                     if (*pSource)
                     {
                        *pDestination++ = *pSource++;
     
                        if (*pSource)
                        {
                           *pDestination++ = *pSource++;
                        }
                        else
                        {
                           break;
                        }
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
            }
         }
         else
         {
            *pDestination++ = *pSource++;
         }
      } while (lenContent > ++iContent && *pSource);

      *pDestination = 0;
   }

   return pDestinationOrig;
}

// Allocates a buffer and copies UTF-8 content to it from the given 
// null-terminated source buffer.  DOES NOT CHECK FOR AN INVALID SOURCE 
// BUFFER POINTER.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
// The developer is responsible for ensuring that the allocated buffer is 
// deallocated via free(), once it is no longer in use.
//
uint8_t * DuplicateUtf8(
            const uint8_t *pSource)        // Content to copy
{
   size_t        nBytes = 0;    // Index for content as individual bytes
   const uint8_t *pSourceOrig;
   uint8_t       *pDestination, *pDestinationOrig;

   if (!pSource)
   {
      return nullptr;
   }

   // Find out how much memory to allocate, and allocate it.
   pSourceOrig = pSource;

   do
   {
      ++nBytes;
   } while (*pSource++);

   nBytes = pSource - pSourceOrig;
   pSource = pSourceOrig;
   pDestination = pDestinationOrig = (uint8_t *) malloc(1 + nBytes);

   // Copy a code point at a time.
   if (pDestination)
   {
      do
      {
         if (*pSource > HALF_ASCII_LIMIT)
         {
            if (*pSource <= TWOFER_LIMIT)
            {
               // Copy two bytes, checking for embedded nulls.
               *pDestination++ = *pSource++;
     
               if (*pSource)
               {
                  *pDestination++ = *pSource++;
               }
               else
               {
                  break;
               }
            }
            else
            {
               if (*pSource <= THREESOME_LIMIT)
               {
                  // Copy three bytes, checking for embedded nulls.
                  *pDestination++ = *pSource++;
     
                  if (*pSource)
                  {
                     *pDestination++ = *pSource++;
     
                     if (*pSource)
                     {
                        *pDestination++ = *pSource++;
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
               else
               {
                  // Copy four bytes, checking for embedded nulls.
                  *pDestination++ = *pSource++;
     
                  if (*pSource)
                  {
                     *pDestination++ = *pSource++;
     
                     if (*pSource)
                     {
                        *pDestination++ = *pSource++;
     
                        if (*pSource)
                        {
                           *pDestination++ = *pSource++;
                        }
                        else
                        {
                           break;
                        }
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
            }
         }
         else
         {
            *pDestination++ = *pSource++;
         }
      } while (*pSource);

      *pDestination = 0;
   }

   return pDestinationOrig;
}

// Allocates a buffer and copies UTF-8 content to it from the given source 
// buffer, up to the specified number of code points.  DOES NOT CHECK FOR AN 
// INVALID SOURCE BUFFER POINTER.  PERFORMS NO UTF-8 VALIDATION OTHER THAN 
// NULL CHECKING.
//
// The developer is responsible for ensuring that the allocated buffer is 
// deallocated via free(), once it is no longer in use.
//
uint8_t * LenDuplicateUtf8(
            const uint8_t *pSource,        // Content to copy
            int lenContent)                // Code point count
{
   int           iContent;        // Index for content as code points
   size_t        nBytes;          // Index for content as individual bytes
   const uint8_t *pSourceOrig;
   uint8_t       *pPrevSource, *pDestination, *pDestinationOrig;

   if (!pSource)
   {
      return nullptr;
   }

   // Find out how much memory to allocate, and allocate it.
   iContent = 0;
   nBytes = 0;
   pSourceOrig = pSource;

   do
   {
      pPrevSource = (uint8_t *) pSource;
      CodePointAdvanceUtf8(&pSource);
      nBytes += pSource - pPrevSource;
   } while (*pSource && lenContent > ++iContent);

   nBytes = 1 + pSource - pSourceOrig;
   pSource = pSourceOrig;
   iContent = 0;
   pDestination = pDestinationOrig = (uint8_t *) malloc(1 + nBytes);

   // Copy a code point at a time.
   if (pDestination)
   {
      do
      {
         // CodePointCopyAndAdvanceUtf8();
         if (*pSource > HALF_ASCII_LIMIT)
         {
            if (*pSource <= TWOFER_LIMIT)
            {
               // Copy two bytes, checking for embedded nulls.
               *pDestination++ = *pSource++;
    
               if (*pSource)
               {
                  *pDestination++ = *pSource++;
               }
               else
               {
                  break;
               }
            }
            else
            {
               if (*pSource <= THREESOME_LIMIT)
               {
                  // Copy three bytes, checking for embedded nulls.
                  *pDestination++ = *pSource++;
    
                  if (*pSource)
                  {
                     *pDestination++ = *pSource++;
    
                     if (*pSource)
                     {
                        *pDestination++ = *pSource++;
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
               else
               {
                  // Copy four bytes, checking for embedded nulls.
                  *pDestination++ = *pSource++;
    
                  if (*pSource)
                  {
                     *pDestination++ = *pSource++;
    
                     if (*pSource)
                     {
                        *pDestination++ = *pSource++;
    
                        if (*pSource)
                        {
                           *pDestination++ = *pSource++;
                        }
                        else
                        {
                           break;
                        }
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
            }
         }
         else
         {
            *pDestination++ = *pSource++;
         }
      } while (lenContent > ++iContent && *pSource);
    
      *pDestination = 0;
   }

   return pDestinationOrig;
}

// Given a buffer partially initialized with null-terminated content, copies 
// additional null-terminated content to it, beginning by overwriting the 
// original content's terminating null and continuing to the additional 
// content's terminating null.  If the buffer is too small to hold the 
// concatenated content, returns nullptr.  Otherwise returns a pointer to the 
// beginning of the buffer.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL 
// CHECKING.
//
uint8_t * ConcatenateUtf8(
            uint8_t       *pContent,             // Original content
            size_t        sizeContentBuffer,     // Whole buffer size
            const uint8_t *pAdditionalContent)   // Content to add
{
   const uint8_t *pContentBase;
   const uint8_t *pAdditionalContentBase;
   size_t        sizeContent;

   // Got any empty input?
   if (!pContent)
   {
      return nullptr;
   }

   if (!pAdditionalContent)
   {
      return pContent;
   }

   // Find the terminating null in the buffer's initial content, then 
   // calculate its size in bytes.
   pContentBase = pContent;

   while (*pContent)
   {
      pContent++;
   }

   sizeContent = pContent - pContentBase;
   pAdditionalContentBase = pAdditionalContent;

   // Copy the additional content.
   while (*pAdditionalContent)
   {
      if (sizeContent + (pAdditionalContent - 
             pAdditionalContentBase) >= sizeContentBuffer)
      {
         return nullptr;  // Insufficient buffer size for total content.
      }

      *pContent++ = *pAdditionalContent++;
   }

   *pContent = 0;         // Add the terminator.
   return (uint8_t *) pContentBase;   // Copy to buffer complete.
}

// Given a buffer partially initialized with UTF-8 content comprising a 
// specified number of code points, copies additional UTF-8 content to it, 
// beginning after that given length and continuing to the length of the 
// additional content, also given as a specified number of code points.  
// If the buffer is too small to hold the concatenated content, returns 
// nullptr.  Otherwise returns a pointer to the beginning of the buffer.
// PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.  THE PERFORMANCE 
// IS TERRIBLE, RELATIVE TO LENGTH-LIMITED ASCII STRING CONCATENATION.
//
uint8_t * LenConcatenateUtf8(
            uint8_t       *pContent,             // Original content
            size_t        sizeContentBuffer,     // Whole buffer size
            const uint8_t *pAdditionalContent,   // Content to add
            int           lenContent,            // Original code point count
            int           lenAdditionalContent)  // Added code point count
{
   const uint8_t *pContentBase;
   const uint8_t *pAdditionalContentBase;
   size_t        sizeContent;
   int           iContent;

   // Got any empty input?
   if (!pContent)
   {
      return nullptr;
   }

   if (!pAdditionalContent)
   {
      return pContent;
   }

   // Find the terminating null in the buffer's initial content, then 
   // calculate its size in bytes.
   pContentBase = pContent;
   pAdditionalContentBase = pContent;
   sizeContent = 0;
   iContent = 0;

   while (lenContent > iContent && *pContent)
   {
      CodePointAdvanceUtf8((const uint8_t **) &pContent);
      ++iContent;
   }

   sizeContent = pContent - pAdditionalContentBase;
   pAdditionalContentBase = pAdditionalContent;
   iContent = 0;

   // Copy code points from the additional content.
   do
   {
      if (*pAdditionalContent > HALF_ASCII_LIMIT)
      {
         if (*pAdditionalContent <= TWOFER_LIMIT)
         {
            // Copy two bytes, checking for embedded nulls.
            *pContent++ = *pAdditionalContent++;

            if (*pAdditionalContent)
            {
               *pContent++ = *pAdditionalContent++;
            }
            else
            {
               break;
            }
         }
         else
         {
            if (*pAdditionalContent <= THREESOME_LIMIT)
            {
               // Copy three bytes, checking for embedded nulls.
               *pContent++ = *pAdditionalContent++;

               if (*pAdditionalContent)
               {
                  *pContent++ = *pAdditionalContent++;

                  if (*pAdditionalContent)
                  {
                     *pContent++ = *pAdditionalContent++;
                  }
                  else
                  {
                     break;
                  }
               }
               else
               {
                  break;
               }
            }
            else
            {
               // Copy four bytes, checking for embedded nulls.
               *pContent++ = *pAdditionalContent++;

               if (*pAdditionalContent)
               {
                  *pContent++ = *pAdditionalContent++;

                  if (*pAdditionalContent)
                  {
                     *pContent++ = *pAdditionalContent++;

                     if (*pAdditionalContent)
                     {
                        *pContent++ = *pAdditionalContent++;
                     }
                     else
                     {
                        break;
                     }
                  }
                  else
                  {
                     break;
                  }
               }
               else
               {
                  break;
               }
            }
         }
      }
      else
      {
         *pContent++ = *pAdditionalContent++;
      }

      if (sizeContent + 
             (pAdditionalContent - pAdditionalContentBase) >= sizeContentBuffer)
      {
         return nullptr;  // Insufficient buffer size for total content.
      }
   } while (lenAdditionalContent > ++iContent && *pAdditionalContent);

   *pContent = 0;         // Add the terminator.
   return (uint8_t *) pContentBase;   // Copy to buffer complete.
}

// Given a pointer to UTF-8 content and a pointer to one or more delimiter 
// code points, searches the content for the first occurrence of any 
// delimiter.  Replaces that code point in the content with a null terminator, 
// including enough nulls to replace the entire code point.  Returns a pointer 
// to any first delimited content, or nullptr if there is no content.  
// PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
uint8_t * SeparateUtf8(
            uint8_t       **ppContent,     // Pointer to content to modify
            const uint8_t *pTokenSet)      // Delimiter(s)
{
   uint8_t *pTopOfContent;       // Pointer to portion to be separated
   uint8_t *pToken;              // Pointer to token in the set
   uint8_t *pTerminator;         // Pointer to nulls replacing a code point

   // Got any empty input?
   if (!ppContent)
   {
      return (uint8_t *) ppContent;
   }

   if (!*ppContent)
   {
      return *ppContent;
   }

   if (!**ppContent)
   {
      *ppContent = nullptr;
      return *ppContent;
   }

   pTopOfContent = *ppContent;
   pToken = (uint8_t *) pTokenSet;

   while (**ppContent)
   {
      if (CodePointCompareUtf8(*ppContent, pToken))
      {
         // Matched a token: fill the code point bytes with nulls.
         pTerminator = *ppContent;
         CodePointAdvanceUtf8((const uint8_t **) ppContent);

         while (pTerminator < *ppContent)
         {
            *pTerminator++ = 0;
         }

         break;  // Return a pointer to the tokenized content.
      }
      else
      {
         // Advance to the next token.
         CodePointAdvanceUtf8((const uint8_t **) &pToken);

         if (!*pToken)
         {
            // No more tokens: advance to the next code point in the content.
            CodePointAdvanceUtf8((const uint8_t **) ppContent);

            if (!**ppContent)
            {
               *ppContent = nullptr;
               break;  // Return a pointer to content after the last token.
            }

            pToken = (uint8_t *) pTokenSet;
         }
      }
   }

   return pTopOfContent;
}

// Given a pointer to an ASCII string and a pointer to one or more delimiter 
// characters, searches the string for the first occurrence of a delimiter. 
// Replaces that character in the string with a null terminator.  Returns a 
// pointer to any first delimited portion of the string, or nullptr if the 
// string is empty.  DOES NOT HANDLE UTF-8.
//
char * SeparateAscii(
            char       **ppszText,         // Pointer to string to modify
            const char *pszTokenSet)       // Delimiter(s)
{
   char *pszTopOfText;          // Pointer to tokenized portion of the string
   char *pszToken;              // Pointer to token in the set

   // Got any empty input?
   if (!ppszText)
   {
      return (char *) ppszText;
   }

   if (!*ppszText)
   {
      return *ppszText;
   }

   if (!**ppszText)
   {
      *ppszText = nullptr;
      return *ppszText;
   }

   pszTopOfText = *ppszText;
   pszToken = (char *) pszTokenSet;

   while (**ppszText)
   {
      if (**ppszText == *pszToken)
      {
         // Matched a token: place a null in the string, advance to the 
         // string's next character, and return a pointer to the portion of 
         // the string before the token.
         **ppszText = '\0';
         (*ppszText)++;
         break;
      }
      else
      {
         ++pszToken;            // Advance to the next token.

         if (!*pszToken)
         {
            // No more tokens: advance to the string's next character.
            (*ppszText)++;

            if (!**ppszText)
            {
               // Return a pointer to the portion of the string after the 
			   // last token.
               *ppszText = nullptr;
               break;
            }

            pszToken = (char *) pszTokenSet;
         }
      }
   }

   return pszTopOfText;
}

// Given a pointer to null-terminated UTF-8 content and a pointer to a null- 
// terminated set of one or more delimiter code points, searches the content 
// for the first occurrence of any delimiter.  Bypasses any initial delimiters 
// at the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the code point immediately prior to it.  
// Returns nullptr if no delimiter is found.  PERFORMS NO UTF-8 VALIDATION 
// OTHER THAN NULL CHECKING.
//
uint8_t * TokenFindUtf8(
            const uint8_t *pContent,       // Content to search
            const uint8_t *pTokenSet)      // Delimiter(s)
{
   uint8_t *pToken;               // Pointer to token in the set
   uint8_t *pContentNext;         // Pointer to next code point in content

   // Got any empty input?
   if (!pContent || !pTokenSet)
   {
      return nullptr;
   }

   pToken = (uint8_t *) pTokenSet;

   // Bypass any initial matching tokens.
   do
   {
      if (CodePointCompareUtf8(pContent, pToken))
      {
         if (!CodePointAdvanceUtf8(&pContent))
         {
            return nullptr;
         }
	 
         pToken = (uint8_t *) pTokenSet;
      }
      else
      {
         if (!CodePointAdvanceUtf8((const uint8_t **) &pToken))
         {
            pToken = (uint8_t *) pTokenSet;
            break;
         }
      }
   } while (true);

   pToken = (uint8_t *) pTokenSet;
   pContentNext = (uint8_t *) pContent;
   CodePointAdvanceUtf8((const uint8_t **) &pContentNext);

   // Search the content for a match against a token.
   if (*pContentNext)
   {
      do
      {
         if (CodePointCompareUtf8(pContentNext, pToken))
         {
            // Found a match: return a pointer to the location ahead of it.
            return (uint8_t *) pContentNext;
         }
         else
         {
            // Advance to the next token.
            if (!CodePointAdvanceUtf8((const uint8_t **) &pToken))
            {
               // No more tokens.
               // Advance to the next code point in the content.
               if (!CodePointAdvanceUtf8((const uint8_t **) &pContentNext))
               {
                  return nullptr;
               }
      
               pToken = (uint8_t *) pTokenSet;
            }
         }
      } while (true);
   }

   return nullptr;
}

// Given a pointer to length-limited UTF-8 content and a pointer to a length-
// limited set of one or more delimiter code points, searches the content for 
// the first occurrence of any delimiter.  Bypasses any initial delimiters at 
// the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the code point immediately prior to it.  
// Returns nullptr if no delimiter is found.  PERFORMS NO UTF-8 VALIDATION 
// OTHER THAN NULL CHECKING.
//
uint8_t * TokenLenFindUtf8(
            const uint8_t *pContent,       // Content to search
            const uint8_t *pTokenSet,      // Delimiter(s)
            int           lenContent,      // Count of code points in content
            int           lenTokenSet)     // Code points in token set
{
   int     iContent;              // Index for content
   int     iToken;                // Index for token in set
   uint8_t *pToken;               // Pointer to token in the set
   uint8_t *pContentNext;         // Pointer to next code point in content

   // Got any empty input?
   if (!pContent || !pTokenSet)
   {
      return nullptr;
   }

   iContent = iToken = 0;
   pToken = (uint8_t *) pTokenSet;

   // Bypass any initial matching tokens.
   while (lenTokenSet > iToken)
   {
      if (CodePointCompareUtf8(pContent, pToken))
      {
         if (!CodePointAdvanceUtf8(&pContent))
         {
            return nullptr;
         }

         ++iContent;

         if (iContent > lenContent)
         {
            return nullptr;
         }

         iToken = 0;
         pToken = (uint8_t *) pTokenSet;
      }
      else
      {
         if (!CodePointAdvanceUtf8((const uint8_t **) &pToken))
         {
            pToken = (uint8_t *) pTokenSet;
            break;
         }

        ++iToken;
      }
   }

   iToken = 0;
   pToken = (uint8_t *) pTokenSet;
   pContentNext = (uint8_t *) pContent;
   CodePointAdvanceUtf8((const uint8_t **) &pContentNext);

   // Search the content for a match against a token.
   if (*pContentNext)
   {
      while (lenContent > 1 + iContent)
      {
         if (CodePointCompareUtf8(pContentNext, pToken))
         {
            // Found a match: return a pointer to the location ahead of it.
            return (uint8_t *) pContentNext;
         }
         else
         {
            // Advance to the next token.
            if (!CodePointAdvanceUtf8((const uint8_t **) &pToken))
            {
               // No more tokens.
               // Advance to the next code point in the content.
               if (!CodePointAdvanceUtf8((const uint8_t **) &pContentNext))
               {
                  return nullptr;
               }
     
               ++iContent;
               iToken = 0;
               pToken = (uint8_t *) pTokenSet;
            }
            else
            {
               ++iToken;
            }
         }
      }
   }

   return nullptr;
}

// Given a pointer to a null-terminated ASCII string and a pointer to a null- 
// terminated set of one or more delimiter characters, searches the string for 
// the first occurrence of a delimiter.  Bypasses any initial delimiters at 
// the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the character immediately prior to it.  
// Returns nullptr if no delimiter is found.  DOES NOT HANDLE UTF-8.
//
char * TokenFindAscii(
            const char *pContent,       // Content to search
            const char *pTokenSet)      // Delimiter(s)
{
   char *pToken;               // Pointer to token in the set
   char *pContentNext;         // Pointer to next character in content

   // Got any empty input?
   if (!pContent || !pTokenSet)
   {
      return nullptr;
   }

   pToken = (char *) pTokenSet;

   // Bypass any initial matching tokens.
   do
   {
      if (*pContent == *pToken)
      {
         ++pContent;

         if (!*pContent)
         {
            return nullptr;
         }
	 
         pToken = (char *) pTokenSet;
      }
      else
      {
         ++pToken;

         if (!*pToken)
         {
            pToken = (char *) pTokenSet;
            break;
         }
      }
   } while (true);

   pToken = (char *) pTokenSet;
   pContentNext = 1 + (char *) pContent;

   // Search the content for a match against a token.
   if (*pContentNext)
   {
      do
      {
         if (*pContentNext == *pToken)
         {
            // Found a match: return a pointer to the location ahead of it.
            return (char *) pContent;
         }
         else
         {
            // Advance to the next token.
            ++pToken;

            if (!*pToken)
            {
               // No more tokens.
               // Advance to the next code point in the content.
               ++pContentNext;
    
               if (!*pContentNext)
               {
                  return nullptr;
               }
    
               pContent = pContentNext;
               pToken = (char *) pTokenSet;
            }
         }
      } while (true);
   }

   return nullptr;
}

// Given a pointer to a length-limited ASCII string and a pointer to a length- 
// limited set of one or more delimiter characters, searches the string for 
// the first occurrence of a delimiter.  Bypasses any initial delimiters at 
// the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the character immediately prior to it.  
// Returns nullptr if no delimiter is found.  DOES NOT HANDLE UTF-8.
//
char * TokenLenFindAscii(
            const char *pContent,       // Content to search
            const char *pTokenSet,      // Delimiter(s)
            int        lenContent,      // Count of code points in content
            int        lenTokenSet)     // Code points in token set
{
   int  iContent;              // Index for content
   int  iToken;                // Index for token set
   char *pToken;               // Pointer to token in the set
   char *pContentNext;         // Pointer to next code point in content

   // Got any empty input?
   if (!pContent || !pTokenSet)
   {
      return nullptr;
   }

   iContent = iToken = 0;
   pToken = (char *) pTokenSet;

   // Bypass any initial matching tokens.
   while (lenTokenSet > iToken)
   {
      if (pContent == pToken)
      {
         ++pContent;

         if (!*pContent)
         {
            return nullptr;
         }

         ++iContent;
         iToken = 0;
         pToken = (char *) pTokenSet;
      }
      else
      {
         ++pToken;

         if (!*pToken)
         {
            pToken = (char *) pTokenSet;
            break;
         }

        ++iToken;
      }
   }

   iToken = 0;
   pToken = (char *) pTokenSet;
   pContentNext = 1 + (char *) pContent;

   // Search the content for a match against a token.
   if (*pContentNext)
   {
      while (lenContent > 1 + iContent)
      {
         if (*pContentNext == *pToken)
         {
            // Found a match: return a pointer to the location ahead of it.
            return (char *) pContent;
         }
         else
         {
            // Advance to the next token.
            ++pToken;

            if (!*pToken)
            {
               // No more tokens.
               // Advance to the next code point in the content.
               ++pContentNext;
    
               if (!*pContentNext)
               {
                  return nullptr;
               }

			   pContent = pContentNext;
               ++iContent;
               pToken = (char *) pTokenSet;
               iToken = 0;
            }
            else
            {
               ++iToken;
            }
         }
      }
   }

   return nullptr;
}

// Returns the UTF-8 code point at the given index within the content.
// THE PERFORMANCE IS TERRIBLE, RELATIVE TO ASCII STRING INDEXING.
//
uint32_t IndexUtf8(
            uint8_t *pContent,  // Content to find
            int iIndex)         // Index at which to find it
{
   uint32_t nCodePoint;
   int iContent;                // Index for content

   if (!pContent)               // Got empty input?
   {
      return 0;
   }

   iContent = 0;

   // Walk the code points to reach the index.
   while (iIndex > iContent)
   {
      if (!CodePointAdvanceUtf8((const uint8_t **) &pContent))
      {
         return 0;
      }

      ++iContent;
   }

   // Have we got a half-ASCII code point?  Return it.
   if (*pContent <= HALF_ASCII_LIMIT)
   {
      nCodePoint = (uint32_t) (*(uint8_t *) pContent);
   }
   else if (*pContent > TWOFER_LIMIT)
   {
      if (*pContent > THREESOME_LIMIT)
      {
         nCodePoint = (uint32_t) ((*(uint8_t *) pContent) << (3 * 8));
         nCodePoint += (uint32_t) ((*(uint8_t *) (1 + pContent)) << (2 * 8));
         nCodePoint += (uint32_t) ((*(uint8_t *) (2 + pContent)) << 8);
         nCodePoint += (uint32_t) (*(uint8_t *) (3 + pContent));
      }
      else
      {
         nCodePoint = (uint32_t) ((*(uint8_t *) pContent) << (2 * 8));
         nCodePoint += (uint32_t) ((*(uint8_t *) (1 + pContent)) << 8);
         nCodePoint += (uint32_t) (*(uint8_t *) (2 + pContent));
      }
   }
   else if (*pContent > SINGLETON_LIMIT)
   {
      nCodePoint = (uint32_t) (*(uint8_t *) pContent) << 8;
      nCodePoint += (uint32_t) (*(uint8_t *) (1 + pContent));
   }
   else
   {
      nCodePoint = ~(uint32_t) 0;   // A noncharacter.
   }

   return nCodePoint;
}

// Removes leading and trailing spaces from null-terminated UTF-8 content, 
// modifying the content in place.  Returns a pointer to the beginning of 
// the content.  In case the content occupies a heap memory block, in order 
// to deallocate that block, the caller will need to retain the original 
// pointer to it.
//
uint8_t * TrimUtf8(
            uint8_t *pContent)             // Content to trim
{
   int     iCount;
   size_t  sizeTrimmed;
   uint8_t *pEndContent;
   uint8_t *pTrimmedContent;

   if (!pContent)                          // Got empty input?
   {
      return pContent;
   }

   if (!*pContent)
   {
      return pContent;
   }

   while (isspace(*(unsigned char *) pContent))
   {
      // Found a leading space: advance to further content.
      CodePointAdvanceUtf8((const uint8_t **) &pContent);
   }

   // Keep a pointer to the beginning, to be returned, then find the end.
   pTrimmedContent = pContent;
   sizeTrimmed = SizeOfUtf8(pTrimmedContent) - 1;
   pEndContent = pTrimmedContent + sizeTrimmed;
   iCount = 0;

   while (iCount < (int) sizeTrimmed && 
          isspace(*(unsigned char *) pEndContent))
   {
      // Found a trailing space: replace it with a null and backtrack.
      *pEndContent = 0;
      CodePointBacktrackUtf8((const uint8_t **) &pEndContent, 
                      (const uint8_t *) pTrimmedContent);
      ++iCount;
   }

   return pTrimmedContent;
}

// Removes leading and trailing spaces from a null-terminated ASCII string, 
// modifying the string in place.  Returns a pointer to the beginning of the 
// string.  In case the string occupies a heap memory block, in order to 
// deallocate that block, the caller will need to retain the original pointer 
// to it.
//
char * TrimAscii(
            char *pszText)                 // String to trim
{
   int     iCount;
   size_t  sizeTrimmed;
   char    *pszEndText;
   char    *pszTrimmedText;

   if (!pszText)                           // Got empty input?
   {
      return pszText;
   }

   if (!*pszText)
   {
      return pszText;
   }

   while (isspace(*(unsigned char *) pszText))
   {
      pszText++;  // Found a leading space: advance to the next character.
   }

   // Keep a pointer to the beginning, to be returned, then find the end.
   pszTrimmedText = pszText;
   pszEndText = pszTrimmedText;
 
   while (*(1 + pszEndText))
   {
      ++pszEndText;
   }
 
   sizeTrimmed = pszEndText - pszTrimmedText;
   iCount = 0;

   while (iCount < (int) sizeTrimmed && 
          isspace(*(unsigned char *) pszEndText))
   {
      // Found a trailing space: replace it with a null and backtrack.
      *pszEndText-- = '\0';
      ++iCount;
   }

   return pszTrimmedText;
}

// Returns a buffer containing the UTF-8 code points beginning at the given 
// first index within the null-terminated content and ending at the last 
// index.  If the last index is less than the first index, creates and returns 
// an empty buffer.
// 
// If the indices are negative, indexing is based on the end of the content; 
// i.e. counts backward from the end of the content to get the code points 
// beginning at the first index relative to the end, and ending at the code 
// point prior to the last index relative to the end.
// 
// Unlike the JavaScript slice() method, a negative first index (iFirst) and 
// zero last index (iLast) returns the last portion of the content, beginning 
// -iFirst code points from the end.
//
// The developer is responsible for ensuring that the allocated buffer for 
// UTF-8 content is deallocated via free(), once it is no longer in use.
//
// THE PERFORMANCE IS TERRIBLE, RELATIVE TO TYPICAL ASCII SUBSTRING 
// FUNCTIONALITY IN C/C++.
//
uint8_t * SliceUtf8(
            const uint8_t *pContent,       // Content to slice
            int           iFirst,          // Index at which slice begins
            int           iLast)           // Index at which slice ends
{
   const uint8_t *pContentOrig; // Pointer to beginning of content
   uint8_t       *pBufferOrig;   // Pointer to copy of slice to return
   uint8_t       *pBuffer;      // Pointer to slice content
   uint8_t       *pFirst;       // Pointer to first byte of slice in content
   uint8_t       *pLast;        // Pointer to its last byte
   uint8_t       *pTerm;        // Pointer to terminating null
   int           iContent;      // Index for content
   int           iTerm;         // Index for terminating null

   if (!pContent)               // Got empty input?
   {
      return (uint8_t *) pContent;
   }

   if (iLast && iLast <= iFirst)
   {
      // The caller has specified an empty string; return one.
      pBufferOrig = (uint8_t *) malloc(1);

      if (pBufferOrig)
      {
         *pBufferOrig = 0;
      }

      return pBufferOrig;
   }
   else if (iFirst >= 0)
   {
      // Walk the content to reach its end.
      pTerm = (uint8_t *) pContent;
      iContent = 0;

      do
      {
         ++pTerm;
      } while (*pTerm);

      // Walk the code points to reach the first index.
      while (iFirst > iContent && *pContent)
      {
         ++iContent;

         if (!CodePointAdvanceUtf8(&pContent))
         {
            break;
         }
      }

      if (pContent >= pTerm)
      {
         // The first index is out of bounds; return an empty string.
         pBufferOrig = (uint8_t *) malloc(1);

         if (pBufferOrig)
         {
            *pBufferOrig = 0;
         }

         return pBufferOrig;
      }

      pFirst = (uint8_t *) pContent;

      // Walk the code points to reach the last index.
      while (iLast > iContent && *pContent)
      {
         ++iContent;

         if (!CodePointAdvanceUtf8(&pContent))
         {
            break;
         }
     }

      if (pContent > pTerm)
      {
         pLast = pTerm;      // Keep the pointer within bounds.
      }
      else
      {
         pLast = (uint8_t *) pContent;
      }
   }
   else  // iFirst < 0
   {
      // Walk the code points to reach the end of the content.
      pContentOrig = pContent;
      iContent = 0;

      do
      {
         ++iContent;
          
         if (!CodePointAdvanceUtf8(&pContent))
         {
            break;
         }
      } while (true);

      iTerm = iContent;

      // Backtrack to the last index.
      if (iLast)
      {
         while (iContent && iContent > iTerm + iLast)
         {
            if (!CodePointBacktrackUtf8(&pContent, pContentOrig))
            {
               break;
            }

            iContent--;
         }
      }

      pLast = (uint8_t *) pContent;

      // Backtrack to the first index.
      while (iContent && iContent > iTerm + iFirst)
      {
         if (!CodePointBacktrackUtf8(&pContent, pContentOrig))
         {
            break;
         }

         iContent--;
      }

      pFirst = (uint8_t *) pContent;

      if (pLast <= pContentOrig)
      {
         // The last index is out of bounds; return an empty string.
         pBufferOrig = (uint8_t *) malloc(1);

         if (pBufferOrig)
         {
            *pBufferOrig = 0;
         }

         return pBufferOrig;
      }

      if (pFirst < pContentOrig)
      {
         pFirst = (uint8_t *) pContentOrig;  // Keep the pointer within bounds.
      }
   }

   // Allocate the buffer to be returned, and fill it with the slice's bytes.
   pBufferOrig = pBuffer = (uint8_t *) malloc(1 + pLast - pFirst);

   if (pBuffer)
   {
      while (pFirst < pLast)
      {
         *pBuffer++ = *pFirst++;
      }

      *pBuffer = 0;
   }

   return pBufferOrig;
}

// Similar to the above function, but for ASCII text, and much faster.
//
// The developer is responsible for ensuring that the allocated buffer for 
// ASCII text is deallocated via free(), once it is no longer in use.
//
char * SliceAscii(
            const char *pContent,          // String to slice
            int        iFirst,             // Index at which slice begins
            int        iLast)              // Index at which slice ends
{
   char *pBufferOrig;           // Pointer to copy of slice to return
   char *pBuffer;               // Pointer to slice content
   char *pFirst;                // Pointer to first byte of slice in string
   char *pLast;                 // Pointer to its last byte
   char *pTerm;                 // Pointer to terminating null

   if (!pContent)               // Got empty input?
   {
      return (char *) pContent;
   }

   if (iLast && iLast <= iFirst)
   {
      // The caller has specified an empty string; return one.
      pBufferOrig = (char *) malloc(1);

      if (pBufferOrig)
      {
         *pBufferOrig = 0;
      }

      return pBufferOrig;
   }
   else
   {
      // Walk the string to reach its end.
      pTerm = (char *) pContent;

      do
      {
         ++pTerm;
      } while (*pTerm);

      if (iFirst >= 0)
      {
         // Get pointers to the locations represented by the first and last 
         // indices regarded as offsets from the string's beginning.
         pFirst = (char *) pContent + (size_t) iFirst;
         pLast = (char *) pContent + (size_t) iLast;

         if (pFirst >= pTerm)
         {
            // The first index is out of bounds; return an empty string.
            pBufferOrig = (char *) malloc(1);

            if (pBufferOrig)
            {
               *pBufferOrig = 0;
            }

            return pBufferOrig;
         }

         if (pLast > pTerm)
         {
            pLast = pTerm;      // Keep the pointer within bounds.
         }
      }
      else  // iFirst < 0
      {
         // Get pointers to the locations represented by the first and last 
         // indices regarded as offsets from the string's end.
         pFirst = pTerm + (size_t) iFirst;
         pLast = pTerm + (size_t) iLast;

         if (pLast <= pContent)
         {
            // The last index is out of bounds; return an empty string.
            pBufferOrig = (char *) malloc(1);

            if (pBufferOrig)
            {
               *pBufferOrig = 0;
            }

            return pBufferOrig;
         }

         if (pFirst < pContent)
         {
            pFirst = (char *) pContent;  // Keep the pointer within bounds.
         }
      }
   }

   // Allocate the buffer to be returned, and fill it with the slice's bytes.
   pBufferOrig = pBuffer = (char *) malloc(1 + pLast - pFirst);

   if (pBuffer)
   {
      while (pFirst < pLast)
      {
         *pBuffer++ = *pFirst++;
      }

      *pBuffer = 0;
   }

   return pBufferOrig;
}

// Returns a buffer containing the UTF-8 code points beginning at the given 
// first index within the content and ending at the last index.  If the last 
// index is less than the first index creates and returns an empty string.  
// 
// If the indices are negative, indexing is done based on the end of the 
// content; i.e., counts backward from the end of the content to get the code 
// points beginning at the first index relative to the end, and ending at the 
// code point prior to the last index relative to the end.
// 
// Unlike the JavaScript slice() method, a negative first index (iFirst) and 
// zero last index (iLast) returns the last portion of the content, beginning 
// -iFirst code points from the end.
//
// The developer is responsible for ensuring that the allocated buffer for 
// UTF-8 content is deallocated via free(), once it is no longer in use.
//
// THE PERFORMANCE IS TERRIBLE, RELATIVE TO TYPICAL ASCII SUBSTRING 
// FUNCTIONALITY IN C/C++.
//
uint8_t * LenSliceUtf8(
            const uint8_t *pContent,       // Content to slice
            int           iFirst,          // Index at which slice begins
            int           iLast,           // Index at which slice ends
            int           lenContent)      // Code point count (whole content)
{
   const uint8_t *pContentOrig; // Pointer to beginning of content
   uint8_t       *pBufferOrig;  // Pointer to copy of slice to return
   uint8_t       *pBuffer;      // Pointer to slice content
   uint8_t       *pFirst;       // Pointer to first byte of slice in content
   uint8_t       *pLast;        // Pointer to its last byte
   int           iContent;      // Index for content
   int           iTerm;         // Index for terminating null (if any)

   if (!pContent)               // Got empty input?
   {
      return (uint8_t *) pContent;
   }

   if (iLast && iLast <= iFirst)
   {
      // The caller has specified an empty string; return one.
      pBufferOrig = (uint8_t *) malloc(1);

      if (pBufferOrig)
      {
         *pBufferOrig = 0;
      }

      return pBufferOrig;
   }
   else if (iFirst >= 0)
   {
      // Walk the code points to reach the first index.
      iContent = 0;

      while (iFirst > iContent && iContent < lenContent)
      {
         ++iContent;
          
         if (!CodePointAdvanceUtf8(&pContent))
         {
            break;
         }
      }

      pFirst = (uint8_t *) pContent;

      // Walk the code points to reach the last index.
      while (iLast > iContent && iContent < lenContent)
      {
         ++iContent;

         if (!CodePointAdvanceUtf8(&pContent))
         {
            break;
         }
      }

      pLast = (uint8_t *) pContent;
   }
   else  // iFirst < 0
   {
      // Walk the code points to reach the end of the content.
      pContentOrig = pContent;
      iContent = 0;

      while (iContent < lenContent)
      {
         ++iContent;

         if (!CodePointAdvanceUtf8(&pContent))
         {
            break;
         }
      }

      // Backtrack from the content's end to the last index.
      iTerm = iContent;

      if (iLast)
      {
         while (iContent > iTerm + iLast)
         {
            if (!CodePointBacktrackUtf8(&pContent, pContentOrig))
            {
               break;
            }

            iContent--;
         }
      }

      pLast = (uint8_t *) pContent;

      // Backtrack to the first index.
      while (iContent > iTerm + iFirst)
      {
         if (!CodePointBacktrackUtf8(&pContent, pContentOrig))
         {
            break;
         }

         iContent--;
      }

      pFirst = (uint8_t *) pContent;
   }

   // Allocate the buffer to be returned, and fill it with the slice's bytes.
   pBufferOrig = pBuffer = (uint8_t *) malloc(1 + pLast - pFirst);

   if (pBuffer)
   {
      while (pFirst < pLast)
      {
         *pBuffer++ = *pFirst++;
      }

      *pBuffer = 0;
   }

   return pBufferOrig;
}

// Similar to the above function, but for ASCII text, and much faster.
//
// The developer is responsible for ensuring that the allocated buffer for 
// ASCII text is deallocated via free(), once it is no longer in use.
//
char * LenSliceAscii(
            const char *pContent,          // String to slice
            int        iFirst,             // Index at which slice begins
            int        iLast,              // Index at which slice ends
            int        lenContent)         // Whole string length
{
   char *pBufferOrig;           // Pointer to copy of slice to return
   char *pBuffer;               // Pointer to slice content
   char *pFirst;                // Pointer to first byte of slice in string
   char *pLast;                 // Pointer to its last byte
   char *pTerm;                 // Pointer to terminating null
   int  iContent;               // Index for content

   if (!pContent)               // Got empty input?
   {
      return (char *) pContent;
   }

   if (iLast && iLast <= iFirst)
   {
      // The caller has specified an empty string; return one.
      pBufferOrig = (char *) malloc(1);

      if (pBufferOrig)
      {
         *pBufferOrig = 0;
      }

      return pBufferOrig;
   }
   else
   {
      // Walk the string to reach its end or its given length, whichever is 
      // first.
      pTerm = (char *) pContent;
      iContent = 0;

      do
      {
         ++pTerm;
      } while (lenContent > ++iContent && *pTerm);

      if (iFirst >= 0)
      {
         // Get pointers to the locations represented by the first and last 
         // indices regarded as offsets from the string's beginning.
         pFirst = (char *) pContent + (size_t) iFirst;
         pLast = (char *) pContent + (size_t) iLast;

         if (pFirst >= pTerm)
         {
            // The first index is out of bounds; return an empty string.
            pBufferOrig = (char *) malloc(1);

            if (pBufferOrig)
            {
               *pBufferOrig = 0;
            }

            return pBufferOrig;
         }

         if (pLast > pTerm)
         {
            pLast = pTerm;      // Keep the pointer within bounds.
         }
      }
      else  // iFirst < 0
      {
         // Get pointers to the locations represented by the first and last 
         // indices regarded as offsets from the string's end.
         pFirst = pTerm - (size_t) iFirst;
         pLast = pTerm - (size_t) iLast;

         if (pLast <= pContent)
         {
            // The last index is out of bounds; return an empty string.
            pBufferOrig = (char *) malloc(1);

            if (pBufferOrig)
            {
               *pBufferOrig = 0;
            }

            return pBufferOrig;
         }

         if (pFirst < pContent)
         {
            pFirst = (char *) pContent;  // Keep the pointer within bounds.
         }
      }
   }

   // Allocate the buffer to be returned, and fill it with the slice's bytes.
   pBufferOrig = pBuffer = (char *) malloc(1 + pLast - pFirst);

   if (pBuffer)
   {
      while (pFirst < pLast)
      {
         *pBuffer++ = *pFirst++;
      }

      *pBuffer = 0;
   }

   return pBufferOrig;
}

// Determines whether null-terminated UTF-8 content matches entirely.
// Returns true for matching content, and false otherwise.  PERFORMS NO 
// UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
// Some ASCII string comparison functions can return values that indicate 
// whether one string might be considered numerically "less than" another. 
// Though that may be useful for certain sorting arrangements, UTF-8 content 
// sorting might best be coded specifically for one locale or another.
//
bool CompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB)      // ...with other content
{
   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return false;
   }

   do
   {
      if (CodePointCompareUtf8(pContentA, pContentB))
      {
         CodePointAdvanceUtf8(&pContentA);
         CodePointAdvanceUtf8(&pContentB);
      }
      else
      {
         return false;
      }
   } while (*pContentA);

   return (*pContentB == 0);
}

// Determines whether null-terminated UTF-8 content matches, entirely, after 
// case folding.  Returns true for matching content, and false otherwise.
// PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
bool CaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB)      // ...with other content
{
   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return false;
   }

   do
   {
      if (CodePointCaseCompareUtf8(pContentA, pContentB))
      {
         CodePointAdvanceUtf8(&pContentA);
         CodePointAdvanceUtf8(&pContentB);
      }
      else
      {
         return false;
      }
   } while (*pContentA);

   return (*pContentB == 0);
}

// Determines whether UTF-8 content matches, up to a given number of code 
// points or any terminating null.  Returns true for matching content, and 
// false otherwise.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
bool LenCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent)      // Code point count
{
   int iContent;                      // Index for content

   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return false;
   }

   iContent = 0;

   do
   {
      if (CodePointCompareUtf8(pContentA, pContentB))
      {
         CodePointAdvanceUtf8(&pContentA);
         CodePointAdvanceUtf8(&pContentB);
      }
      else
      {
         return !(*pContentA && *pContentB);
      }
   } while (lenContent > ++iContent && *pContentA);

   return (lenContent == iContent ? true : 
           *pContentA ? true : *pContentB == 0);
}

// Determines whether UTF-8 content matches, up to a given number of code 
// points or any terminating null, after case folding.  Returns true for 
// matching content, and false otherwise.  PERFORMS NO UTF-8 VALIDATION OTHER 
// THAN NULL CHECKING.
//
bool LenCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent)      // Code point count
{
   int iContent;                      // Index for content

   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return false;
   }

   iContent = 0;

   do
   {
      if (CodePointCaseCompareUtf8(pContentA, pContentB))
      {
         CodePointAdvanceUtf8(&pContentA);
         CodePointAdvanceUtf8(&pContentB);
      }
      else
      {
         return !(*pContentA && *pContentB);
      }
   } while (lenContent > ++iContent && *pContentA);

   return (lenContent == iContent ? true : 
           *pContentA ? true : *pContentB == 0);
}

// Determines whether content matches, up to a specified number of bytes or 
// any terminating null.  Returns true for matching content, and false 
// otherwise.  PERFORMS NO POINTER VALIDATION AND NO UTF-8 VALIDATION OTHER 
// THAN NULL CHECKING.
//
bool SizeCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            size_t        sizeContent)     // Content size (bytes)
{
   size_t nSize;                      // Accumulate size of content

   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return false;
   }

   if (!*pContentA && !*pContentB)
   {
      return true;
   }

   nSize = 0;

   do
   {
      if (*pContentA == *pContentB)
      {
         ++pContentA;
         ++pContentB;
      }
      else
      {
         return !(*pContentA && *pContentB);
      }
   } while (sizeContent > ++nSize && *pContentA);

   return (sizeContent == nSize ? true : 
           *pContentA ? true : *pContentB == 0);
}

// Determines whether content matches after case folding, up to a specified 
// number of bytes.  Returns true if the given ranges begin and end at byte 
// values consistent with valid code point boundaries and if there is a case-
// insensitive match.  Returns false otherwise.  PERFORMS NO FURTHER POINTER 
// VALIDATION AND NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
bool SizeCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            size_t        sizeContent)     // Content size (bytes)
{
   size_t  nSize;                     // Accumulate size of content
   uint8_t *pPrevCodePointInContentA;

   // Got any empty input?
   if (!pContentA || !pContentB)
   {
      return false;
   }

   if (!*pContentA && !*pContentB)
   {
      return true;
   }

   // Verify that the beginning of the range isn't flagged as an intra- 
   // code-point byte.
   if (*pContentA > HALF_ASCII_LIMIT && *pContentA <= SINGLETON_LIMIT)
   {
      return false;
   }

   if (*pContentB > HALF_ASCII_LIMIT && *pContentB <= SINGLETON_LIMIT)
   {
      return false;
   }

   nSize = 0;
   pPrevCodePointInContentA = (uint8_t *) pContentA;

   do
   {
      if (CodePointCaseCompareUtf8(pContentA, pContentB))
      {
         CodePointAdvanceUtf8(&pContentA);
         CodePointAdvanceUtf8(&pContentB);
         nSize += (uint8_t *) pContentA - pPrevCodePointInContentA;
         pPrevCodePointInContentA = (uint8_t *) pContentA;
      }
      else
      {
         return !(*pContentA && *pContentB);
      }
   } while (sizeContent > nSize && *pContentA);

   return (sizeContent == nSize ? true : 
           *pContentA ? true : *pContentB == 0);
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- returns a pointer 
// to any first matching sequence within the larger content.  Returns nullptr 
// and sets *ppLast to nullptr if no match is found.
//
uint8_t * FindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent,  // Needle
            uint8_t       **ppLast)         // Returned loc where match ends
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match

   if (!*pSearchContent)              // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (uint8_t *) pContent;
      }

      return (uint8_t *) pContent;
   }

   if (!*pContent)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   pSliceInitial = (uint8_t *) pSearchContent;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);

         if (!*pContent)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pContentSequence = (uint8_t *) pContent;

      do
      {
         // The inner loop walks through each potential match.
         CodePointAdvanceUtf8(&pSearchContent);

         if (!*pSearchContent)
         {
            if (ppLast)
            {
               *ppLast = (uint8_t *) pContent;
            }

            return pContentSequence;  // "ab" includes "b".
         }

         CodePointAdvanceUtf8(&pContent);

         if (!*pContent)
         {
            if (ppLast)               // "ab" doesn't include "bc".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         if (!CodePointCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.  The inner loop has walked to 
      // or beyond that "next" code point already.  The overhead of 
      // managing another variable to store that "next" code point would 
      // likely be greater, in many cases, than simply "re-advancing" from 
      // where the outer loop has left things.
      //
      pContent = pContentSequence;
      pSearchContent = pSliceInitial;
      CodePointAdvanceUtf8(&pContent);
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- and a certain 
// number of code points in each, returns a pointer to any first matching 
// sequence within the larger content.  Returns nullptr and sets *ppLast to 
// nullptr if no match is found.
//
uint8_t * LenFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent,  // Needle 
            int           lenContent,       // Code point count (haystack)
            int           lenSlice,         // Code point count (needle)
            uint8_t       **ppLast)         // Returned loc where match ends
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match
   int     iContent;                  // Index for larger content
   int     iContentSequence;          // Index for prospective match in content
   int     iSlice;                    // Index for prospective match in slice

   if (!*pSearchContent)              // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (uint8_t *) pContent;
      }

      return (uint8_t *) pContent;
   }

   if (!*pContent)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   if (!lenContent)                   // Got zero length?
   {
      if (!lenSlice)
      {
         if (ppLast)
         {
            *ppLast = (uint8_t *) pContent;
         }

         return (uint8_t *)  pContent;
      }
      else
      {
         lenContent = CodePointCountUtf8(pContent);
      }
   }

   if (!lenSlice)
   {
      lenSlice = CodePointCountUtf8(pSearchContent);
   }

   pSliceInitial = (uint8_t *) pSearchContent;
   iContent = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);
         ++iContent;

         if (lenContent <= iContent || !*pContent)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pContentSequence = (uint8_t *) pContent;
      iContentSequence = iContent;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         CodePointAdvanceUtf8(&pSearchContent);
         ++iSlice;

         if (lenSlice <= iSlice || !*pSearchContent)
         {
            if (ppLast)
            {
               *ppLast = (uint8_t *) pContent;
            }

            return pContentSequence;  // "ab" includes "b".
         }

         CodePointAdvanceUtf8(&pContent);
         ++iContent;

         if (lenContent <= iContent || !*pContent)
         {
            if (ppLast)               // "ab" doesn't include "bc".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         if (!CodePointCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- returns a pointer 
// to any first matching sequence, within the larger content, after case 
// folding.  Returns nullptr and sets *ppLast to nullptr if no match is found.
//
uint8_t * CaseFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent,  // Needle
            uint8_t       **ppLast)         // Returned loc where match ends
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match

   if (!*pSearchContent)              // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (uint8_t *) pContent;
      }

      return (uint8_t *) pContent;
   }

   if (!*pContent)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   pSliceInitial = (uint8_t *) pSearchContent;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);

         if (!*pContent)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pContentSequence = (uint8_t *) pContent;

      do
      {
         // The inner loop walks through potential matches.
         CodePointAdvanceUtf8(&pSearchContent);

         if (!*pSearchContent)
         {
            if (ppLast)
            {
               *ppLast = (uint8_t *) pContent;
            }

            return pContentSequence;  // "ab" includes "b".
         }

         CodePointAdvanceUtf8(&pContent);

         if (!*pContent)
         {
            if (ppLast)               // "ab" doesn't include "bc".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.  The inner loop has walked to 
      // or beyond that "next" code point already.  For this case-
      // insensitive routine, it's unclear whether the overhead of managing 
      // another variable to store that "next" code point would be less, 
      // in typical cases, than "re-advancing" here.  The performance 
      // difference is small enough that this code is similar to the 
      // corresponding code in the case-sensitive routine, for consistency.
      pContent = pContentSequence;
      pSearchContent = pSliceInitial;
      CodePointAdvanceUtf8(&pContent);
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- and a certain 
// number of code points in each, returns a pointer to any first matching 
// sequence, within the larger content, after case folding.  Returns nullptr 
// and sets *ppLast to nullptr if no match is found.
//
uint8_t * LenCaseFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent,  // Needle 
            int           lenContent,       // Code point count (haystack)
            int           lenSlice,         // Code point count (needle)
            uint8_t       **ppLast)         // Returned loc where match ends
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match
   int     iContent;                  // Index for larger content
   int     iContentSequence;          // Index for prospective match in content
   int     iSlice;                    // Index for prospective match in slice

   if (!*pSearchContent)              // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (uint8_t *) pContent;
      }

      return (uint8_t *) pContent;
   }

   if (!*pContent)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   if (!lenContent)                   // Got zero length?
   {
      if (!lenSlice)
      {
         if (ppLast)
         {
            *ppLast = (uint8_t *) pContent;
         }

         return (uint8_t *) pContent;
      }
      else
      {
         lenContent = CodePointCountUtf8(pContent);
      }
   }

   if (!lenSlice)
   {
      lenSlice = CodePointCountUtf8(pSearchContent);
   }

   pSliceInitial = (uint8_t *) pSearchContent;
   iContent = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);
         ++iContent;

         if (lenContent <= iContent || !*pContent)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pContentSequence = (uint8_t *) pContent;
      iContentSequence = iContent;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         CodePointAdvanceUtf8(&pSearchContent);
         ++iSlice;

         if (lenSlice <= iSlice || !*pSearchContent)
         {
            if (ppLast)
            {
               *ppLast = (uint8_t *) pContent;
            }

            return pContentSequence;  // "ab" includes "b".
         }

         CodePointAdvanceUtf8(&pContent);
         ++iContent;

         if (lenContent <= iContent || !*pContent)
         {
            if (ppLast)               // "ab" doesn't include "bc".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- and a certain 
// number of characters in each, returns a pointer to any first matching 
// sequence within the larger string.  Returns nullptr and sets *ppLast to 
// nullptr if no match is found.
//
char * LenFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // Character count (haystack)
            int        lenSlice,           // Character count (needle)
            char       **ppLast)           // Returned loc where match ends
{
   char *pszSliceInitial;             // Initial pointer to prospective match
   char *pszTextSequence;             // Pointer to prospective match
   int  iText;                        // Index for larger string
   int  iTextSequence;                // Index for prospective match in string
   int  iSlice;                       // Index for prospective match in slice

   if (!*pszSearchText)               // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (char *) pszText;
      }

      return (char *) pszText;
   }

   if (!*pszText)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   if (!lenText)                      // Got zero length?
   {
      if (!lenSlice)
      {
         if (ppLast)
         {
            *ppLast = (char *) pszText;
         }

         return (char *) pszText;
      }
      else
      {
         // Get the haystack string's length.
         pszTextSequence = 1 + (char *) pszText;
 
         while (*pszTextSequence)
         {
        	++pszTextSequence;
         }

         lenText = (int) (pszTextSequence - (char *) pszText);
      }
   }

   if (!lenSlice)
   {
      // Get the needle string's length.
      pszTextSequence = 1 + (char *) pszSearchText;

      while (*pszTextSequence)
      {
         ++pszTextSequence;
      }

      lenSlice = (int) (pszTextSequence - pszSearchText);
   }

   pszSliceInitial = (char *) pszSearchText;
   iText = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (*pszText != *pszSearchText)
      {
         ++pszText;
         ++iText;

         if (lenText <= iText || !*pszText)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pszTextSequence = (char *) pszText;
      iTextSequence = iText;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         ++pszSearchText;
         ++iSlice;

         if (lenSlice <= iSlice || !*pszSearchText)
         {
            if (ppLast)
            {
               *ppLast = (char *) pszText;
            }

            return pszTextSequence;     // "ab" includes "b".
         }

         ++pszText;
         ++iText;

         if (lenText <= iText || !*pszText)
         {
            if (ppLast)               // "ab" doesn't include "bc".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }
      } while (*pszText == *pszSearchText);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pszSearchText = pszSliceInitial;
      pszText = pszTextSequence;
      iText = 1 + iTextSequence;
      ++pszText;
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- returns a 
// pointer to any first matching sequence, within the larger string, after 
// case folding.  Returns nullptr and sets *ppLast to nullptr if no match is 
// found.
//
char * CaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle
            char       **ppLast)           // Returned loc where match ends
{
   char *pszSliceInitial;             // Initial pointer to prospective match
   char *pszTextSequence;             // Pointer to prospective match

   if (!*pszSearchText)               // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (char *) pszText;
      }

      return (char *) pszText;
   }

   if (!*pszText)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   pszSliceInitial = (char *) pszSearchText;

   do
   {
      // The outer loop walks through mismatches.
      if (CM08[(unsigned char) *pszText] != 
             CM08[(unsigned char) *pszSearchText])
      {
         ++pszText;

         if (!*pszText)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pszTextSequence = (char *) pszText;

      do
      {
         // The inner loop walks through potential matches.
         ++pszSearchText;

         if (!*pszSearchText)
         {
            if (ppLast)
            {
               *ppLast = (char *) pszText;
            }

            return pszTextSequence;   // "ab" includes "b".
         }

         ++pszText;

         if (!*pszText)               // "ab" doesn't include "bc".
         {
            if (ppLast)
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }
      } while (CM08[(unsigned char) *pszText] == 
                  CM08[(unsigned char) *pszSearchText]);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.  The inner loop has walked to 
      // or beyond that "next" code point already.  For this case-
      // insensitive routine, it's unclear whether the overhead of managing 
      // another variable to store that "next" code point would be less, 
      // in typical cases, than "re-advancing" here.  The performance 
      // difference is small enough that this code is similar to the 
      // corresponding code in the case-sensitive routine, for consistency.
      pszText = pszTextSequence;
      pszSearchText = pszSliceInitial;
      ++pszText;
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- and a certain 
// number of characters in each, returns a pointer to any first matching 
// sequence, within the larger string, after case folding.  Returns nullptr 
// and sets *ppLast to nullptr if no match is found.
//
char * LenCaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // String length (haystack)
            int        lenSlice,           // String length (needle)
            char       **ppLast)           // Returned loc where match ends
{
   char *pszSliceInitial;             // Initial pointer to prospective match
   char *pszTextSequence;             // Pointer to prospective match
   int  iText;                        // Index for larger string
   int  iTextSequence;                // Index for prospective match in string
   int  iSlice;                       // Index for prospective match in slice

   if (!*pszSearchText)               // Got any empty input?
   {
      if (ppLast)
      {
         *ppLast = (char *) pszText;
      }

      return (char *) pszText;
   }

   if (!*pszText)
   {
      if (ppLast)
      {
         *ppLast = nullptr;
      }

      return nullptr;
   }

   if (!lenText)                      // Got zero length?
   {
      if (!lenSlice)
      {
         if (ppLast)
         {
            *ppLast = (char *) pszText;
         }

         return (char *) pszText;
      }
      else
      {
         // Get the haystack string's length.
         pszTextSequence = 1 + (char *) pszText;
 
         while (*pszTextSequence)
         {
        	++pszTextSequence;
         }

         lenText = (int) (pszTextSequence - pszText);
      }
   }

   if (!lenSlice)
   {
      // Get the needle string's length.
      pszTextSequence = 1 + (char *) pszSearchText;

      while (*pszTextSequence)
      {
         ++pszTextSequence;
      }

      lenSlice = (int) (pszTextSequence - pszSearchText);
   }

   pszSliceInitial = (char *) pszSearchText;
   iText = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (CM08[(unsigned char) *pszText] != 
             CM08[(unsigned char) *pszSearchText])
      {
         ++pszText;
         ++iText;

         if (lenText <= iText || !*pszText)
         {
            if (ppLast)               // "ab" doesn't include "c".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }

         continue;
      }

      pszTextSequence = (char *) pszText;
      iTextSequence = iText;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         ++pszSearchText;
         ++iSlice;

         if (lenSlice <= iSlice || !*pszSearchText)
         {
            if (ppLast)
            {
               *ppLast = (char *) pszText;
            }

            return pszTextSequence;   // "ab" includes "b".
         }

         ++pszText;
         ++iText;

         if (lenText <= iText || !*pszText)
         {
            if (ppLast)               // "ab" doesn't include "bc".
            {
               *ppLast = nullptr;
            }

            return nullptr;
         }
      } while (CM08[(unsigned char) *pszText] == 
                  CM08[(unsigned char) *pszSearchText]);

      // Continue from the character, within the larger string, after the 
      // last one checked in the outer loop.
      pszSearchText = pszSliceInitial;
      pszText = pszTextSequence;
      iText = 1 + iTextSequence;
      ++pszText;
   } while (true);

   if (ppLast)
   {
      *ppLast = nullptr;
   }

   return nullptr;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- returns an index of 
// any first matching sequence within the larger content.  Returns -1 if no 
// match is found.
//
int IndexFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent)  // Needle
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match
   int     iContent;                  // Index for larger content; return value

   if (!*pSearchContent)              // Got any empty input?
   {
      return -1;
   }

   if (!*pContent)
   {
      return -1;
   }

   pSliceInitial = (uint8_t *) pSearchContent;
   iContent = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);

         if (!*pContent)
         {
            return -1;                // "ab" doesn't include "c".
         }

         ++iContent;
         continue;
      }

      pContentSequence = (uint8_t *) pContent;

      do
      {
         // The inner loop walks through each potential match.
         CodePointAdvanceUtf8(&pContent);
         CodePointAdvanceUtf8(&pSearchContent);

         if (!*pSearchContent)
         {
            return iContent;            // "ab" includes "b".
         }

         if (!*pContent)
         {
            return -1;                // "ab" doesn't include "bc".
         }

         if (!CodePointCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.  The inner loop has walked to 
      // or beyond that "next" code point already.  The overhead of 
      // managing another variable to store that "next" code point would 
      // likely be greater, in many cases, than simply "re-advancing" from 
      // where the outer loop has left things.
      //
      pContent = pContentSequence;
      pSearchContent = pSliceInitial;
      CodePointAdvanceUtf8(&pContent);
      ++iContent;
   } while (true);

   return -1;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- and a certain 
// number of code points in each, returns an index of to any first matching 
// sequence within the larger content.  Returns -1 if no match is found.
//
int IndexLenFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent,  // Needle 
            int           lenContent,       // Code point count (haystack)
            int           lenSlice)         // Code point count (needle)
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match
   int     iContent;                  // Index for larger content; return value
   int     iContentSequence;          // Index for prospective match in content
   int     iSlice;                    // Index for prospective match in slice

   if (!*pSearchContent)              // Got any empty input?
   {
      return -1;
   }

   if (!*pContent)
   {
      return -1;
   }

   if (!lenContent)                   // Got zero length?
   {
      if (!lenSlice)
      {
         return 0;
      }
      else
      {
         lenContent = CodePointCountUtf8(pContent);
      }
   }

   if (!lenSlice)
   {
      lenSlice = CodePointCountUtf8(pSearchContent);
   }

   pSliceInitial = (uint8_t *) pSearchContent;
   iContent = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);
         ++iContent;

         if (lenContent <= iContent || !*pContent)
         {
            return -1;                // "ab" doesn't include "c".
         }

         continue;
      }

      pContentSequence = (uint8_t *) pContent;
      iContentSequence = iContent;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         CodePointAdvanceUtf8(&pContent);
         ++iContent;
         CodePointAdvanceUtf8(&pSearchContent);
         ++iSlice;

         if (lenSlice <= iSlice || !*pSearchContent)
         {
            return iContentSequence;  // "ab" includes "b".
         }

         if (lenContent <= iContent || !*pContent)
         {
            return -1;                // "ab" doesn't include "bc".
         }

         if (!CodePointCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (true);

   return -1;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- returns an index of 
// any first matching sequence, within the larger content, after case folding. 
// Returns -1 if no match is found.
//
int IndexCaseFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent)  // Needle
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match
   int     iContent;                  // Index for larger content; return value

   if (!*pSearchContent)              // Got any empty input?
   {
      return -1;
   }

   if (!*pContent)
   {
      return -1;
   }

   pSliceInitial = (uint8_t *) pSearchContent;
   iContent = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);

         if (!*pContent)
         {
            return -1;                // "ab" doesn't include "c".
         }

         ++iContent;
         continue;
      }

      pContentSequence = (uint8_t *) pContent;

      do
      {
         // The inner loop walks through potential matches.
         CodePointAdvanceUtf8(&pContent);
         CodePointAdvanceUtf8(&pSearchContent);

         if (!*pSearchContent)
         {
            return iContent;          // "ab" includes "b".
         }

         if (!*pContent)
         {
            return -1;                // "ab" doesn't include "bc".
         }

         if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.  The inner loop has walked to 
      // or beyond that "next" code point already.  For this case-
      // insensitive routine, it's unclear whether the overhead of managing 
      // another variable to store that "next" code point would be less, 
      // in typical cases, than "re-advancing" here.  The performance 
      // difference is small enough that this code is similar to the 
      // corresponding code in the case-sensitive routine, for consistency.
      pContent = pContentSequence;
      pSearchContent = pSliceInitial;
      CodePointAdvanceUtf8(&pContent);
      ++iContent;
   } while (true);
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- and a certain 
// number of code points in each, returns an index of any first matching 
// sequence, within the larger content, after case folding.  Returns -1 if no 
// match is found.
//
int IndexLenCaseFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent,  // Needle 
            int           lenContent,       // Code point count (haystack)
            int           lenSlice)         // Code point count (needle)
{
   uint8_t *pSliceInitial;            // Initial pointer to prospective match
   uint8_t *pContentSequence;         // Pointer to prospective match
   int     iContent;                  // Index for larger content; return value
   int     iContentSequence;          // Index for prospective match in content
   int     iSlice;                    // Index for prospective match in slice

   if (!*pSearchContent)              // Got any empty input?
   {
      return -1;
   }

   if (!*pContent)
   {
      return -1;
   }

   if (!lenContent)                   // Got zero length?
   {
      if (!lenSlice)
      {
         return 0;
      }
      else
      {
         lenContent = CodePointCountUtf8(pContent);
      }
   }

   if (!lenSlice)
   {
      lenSlice = CodePointCountUtf8(pSearchContent);
   }

   pSliceInitial = (uint8_t *) pSearchContent;
   iContent = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
      {
         CodePointAdvanceUtf8(&pContent);
         ++iContent;

         if (lenContent <= iContent || !*pContent)
         {
            return -1;                // "ab" doesn't include "c".
         }

         continue;
      }

      pContentSequence = (uint8_t *) pContent;
      iContentSequence = iContent;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         CodePointAdvanceUtf8(&pContent);
         ++iContent;
         CodePointAdvanceUtf8(&pSearchContent);
         ++iSlice;

         if (lenSlice <= iSlice || !*pSearchContent)
         {
            return iContentSequence;  // "ab" includes "b".
         }

         if (lenContent <= iContent || !*pContent)
         {
            return -1;                // "ab" doesn't include "bc".
         }

         if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (true);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (true);

   return -1;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- and a certain 
// number of characters in each, returns an index of any first matching 
// sequence within the larger string.  Returns -1 if no match is found.
//
int IndexLenFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // Character count (haystack)
            int        lenSlice)           // Character count (needle)
{
   char *pszSliceInitial;             // Initial pointer to prospective match
   char *pszTextSequence;             // Pointer to prospective match
   int  iText;                        // Index for larger string; return value
   int  iTextSequence;                // Index for prospective match in string
   int  iSlice;                       // Index for prospective match in slice

   if (!*pszSearchText)               // Got any empty input?
   {
      return -1;
   }

   if (!*pszText)
   {
      return -1;
   }

   if (!lenText)                      // Got zero length?
   {
      if (!lenSlice)
      {
         return 0;
      }
      else
      {
         // Get the haystack string's length.
         pszTextSequence = 1 + (char *) pszText;
 
         while (*pszTextSequence)
         {
        	++pszTextSequence;
         }

         lenText = (int) (pszTextSequence - pszText);
      }
   }

   if (!lenSlice)
   {
      // Get the needle string's length.
      pszTextSequence = 1 + (char *) pszSearchText;

      while (*pszTextSequence)
      {
         ++pszTextSequence;
      }

      lenSlice = (int) (pszTextSequence - pszSearchText);
   }

   pszSliceInitial = (char *) pszSearchText;
   iText = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (*pszText != *pszSearchText)
      {
         ++pszText;
         ++iText;

         if (lenText <= iText || !*pszText)
         {
            return -1;                // "ab" doesn't include "c".
         }

         continue;
      }

      pszTextSequence = (char *) pszText;
      iTextSequence = iText;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         ++pszText;
         ++iText;
         ++pszSearchText;
         ++iSlice;

         if (lenSlice <= iSlice || !*pszSearchText)
         {
            return iTextSequence;     // "ab" includes "b".
         }

         if (lenText <= iText || !*pszText)
         {
            return -1;                // "ab" doesn't include "bc".
         }
      } while (*pszText == *pszSearchText);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pszSearchText = pszSliceInitial;
      pszText = pszTextSequence;
      iText = 1 + iTextSequence;
      ++pszText;
   } while (true);

   return -1;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- returns an 
// index of any first matching sequence, within the larger string, after 
// case folding.  Returns -1 if no match is found.
//
int IndexCaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText)     // Needle
{
   char *pszSliceInitial;             // Initial pointer to prospective match
   char *pszTextSequence;             // Pointer to prospective match
   int  iText;                        // Index for larger string; return value

   if (!*pszSearchText)               // Got any empty input?
   {
      return -1;
   }

   if (!*pszText)
   {
      return -1;
   }

   pszSliceInitial = (char *) pszSearchText;
   iText = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (CM08[(unsigned char) *pszText] != 
             CM08[(unsigned char) *pszSearchText])
      {
         ++pszText;

         if (!*pszText)
         {
            return -1;                // "ab" doesn't include "c".
         }

         ++iText;
         continue;
      }

      pszTextSequence = (char *) pszText;

      do
      {
         // The inner loop walks through potential matches.
         ++pszText;
         ++pszSearchText;

         if (!*pszSearchText)
         {
            return iText;             // "ab" includes "b".
         }

         if (!*pszText)
         {
            return -1;                // "ab" doesn't include "bc".
         }
      } while (CM08[(unsigned char) *pszText] == 
                  CM08[(unsigned char) *pszSearchText]);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.  The inner loop has walked to 
      // or beyond that "next" code point already.  For this case-
      // insensitive routine, it's unclear whether the overhead of managing 
      // another variable to store that "next" code point would be less, 
      // in typical cases, than "re-advancing" here.  The performance 
      // difference is small enough that this code is similar to the 
      // corresponding code in the case-sensitive routine, for consistency.
      pszText = pszTextSequence;
      pszSearchText = pszSliceInitial;
      ++iText;
      ++pszText;
   } while (true);

   return -1;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- and a certain 
// number of characters in each, returns an index of any first matching 
// sequence, within the larger string, after case folding.  Returns -1 if no 
// match is found.
//
int IndexLenCaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // String length (haystack)
            int        lenSlice)           // String length (needle)
{
   char *pszSliceInitial;             // Initial pointer to prospective match
   char *pszTextSequence;             // Pointer to prospective match
   int  iText;                        // Index for larger string
   int  iTextSequence;                // Index for prospective match in string
   int  iSlice;                       // Index for prospective match in slice

   if (!*pszSearchText)               // Got any empty input?
   {
      return -1;
   }

   if (!*pszText)
   {
      return -1;
   }

   if (!lenText)                      // Got zero length?
   {
      if (!lenSlice)
      {
         return 0;
      }
      else
      {
         // Get the haystack string's length.
         pszTextSequence = 1 + (char *) pszText;
 
         while (*pszTextSequence)
         {
        	++pszTextSequence;
         }

         lenText = (int) (pszTextSequence - pszText);
      }
   }

   if (!lenSlice)
   {
      // Get the needle string's length.
      pszTextSequence = 1 + (char *) pszSearchText;

      while (*pszTextSequence)
      {
         ++pszTextSequence;
      }

      lenSlice = (int) (pszTextSequence - pszSearchText);
   }

   pszSliceInitial = (char *) pszSearchText;
   iText = 0;

   do
   {
      // The outer loop walks through mismatches.
      if (CM08[(unsigned char) *pszText] != 
             CM08[(unsigned char) *pszSearchText])
      {
         ++pszText;
         ++iText;

         if (lenText <= iText || !*pszText)
         {
            return -1;                // "ab" doesn't include "c".
         }

         continue;
      }

      pszTextSequence = (char *) pszText;
      iTextSequence = iText;
	  iSlice = 0;

      do
      {
         // The inner loop walks through any prospective match.
         ++pszText;
         ++iText;
         ++pszSearchText;
         ++iSlice;

         if (lenSlice <= iSlice || !*pszSearchText)
         {
            return iTextSequence;     // "ab" includes "b".
         }

         if (lenText <= iText || !*pszText)
         {
            return -1;                // "ab" doesn't include "bc".
         }
      } while (CM08[(unsigned char) *pszText] == 
                  CM08[(unsigned char) *pszSearchText]);

      // Continue from the character, within the larger string, after the 
      // last one checked in the outer loop.
      pszSearchText = pszSliceInitial;
      pszText = pszTextSequence;
      iText = 1 + iTextSequence;
      ++pszText;
   } while (true);

   return -1;
}

// Compares null-terminated UTF-8 content, matching wildcards.  Accepts '?' 
// as a single-code-point wildcard.  For each '*' wildcard, seeks out a 
// matching sequence of any code points beyond it.  Otherwise compares the 
// content a code point at a time.  PERFORMS NO UTF-8 VALIDATION.
//
bool WildCompareUtf8(
            const uint8_t *pWild,   // Content (may include wildcards)
            const uint8_t *pTame)   // Content to compare (no wildcards)
{
   uint8_t *pWildSequence;    // Points to prospective match after '*'
   uint8_t *pTameSequence;    // Points to prospective match in tame content

   // Find a first wildcard, if one exists, and the beginning of any 
   // prospectively matching sequence after it.
   do
   {
      // Check for the end from the start.  Get out fast, if possible.
      if (!*pTame)
      {
         if (*pWild)
         {
            while (*pWild == '*')
            {
               if (!(*(++pWild)))
               {
                  return true;     // "ab*" matches "ab".
               }
            }

             return false;         // "abcd" doesn't match "abc".
         }
         else
         {
            return true;           // "abc" matches "abc".
         }
      }
      else if (*pWild == '*')
      {
         // Got wild: set up for the second loop and skip on down there.
         while (CodePointAdvanceUtf8(&pWild) && *pWild == '*')
         {
            continue;
         }

         if (!*pWild)
         {
            return true;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*bc" doesn't match "ab".
               }
            }
         }

         // Keep fallback positions for retry in case of incomplete match.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
         break;
      }
      else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         return false;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (true);

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         while (*(++pWild) == '*')
         {
            continue;
         }

         if (!*pWild)
         {
            return true;           // "ab*c*" matches "abcd".
         }

         if (!*pTame)
         {
            return false;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*b*c" doesn't match "ab".
               }
            }
         }

         // Keep the new fallback positions.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
      }
      else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (!*pTame)
         {
            return false;          // "*bc" doesn't match "abcd".
         }

         // A fine time for questions.
         while (*pWildSequence == '?')
         {
            ++pWildSequence;
            ++pTameSequence;
         }

         // Fall back, but never so far again.
         pWild = pWildSequence;

         while (!CodePointAdvanceAndCompareUtf8(pWild, 
                      (const uint8_t **) &pTameSequence))
         {
            if (!*pTameSequence)
            {
               return false;       // "*bcd" doesn't match "abc".
            }
         }

         pTame = pTameSequence;
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            return true;           // "*bc" matches "abc".
         }
         else
         {
            return false;          // "*bcd" doesn't match "abc".
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (true);
}

// Compares UTF-8 content, up to a specified number of code points, matching 
// wildcards.  Accepts '?' as a single-code-point wildcard.  For each '*' 
// wildcard, seeks out a matching sequence of any code points beyond it.  
// Otherwise compares the content a code point at a time.  PERFORMS NO UTF-8 
// VALIDATION.
//
bool WildLenCompareUtf8(
            const uint8_t *pWild,   // Content (may include wildcards)
            const uint8_t *pTame,   // Content to compare (no wildcards)
            int           lenWild,  // Count of code points in content
            int           lenTame)  // Code points in prospective match
{
   int     iWild = 0;         // Index for both inputs in upper loop
   int     iTame = 0;         // Index for tame content, used in lower loop
   int     iWildSequence;     // Index for prospective match after '*'
   int     iTameSequence;     // Index for match in tame content
   uint8_t *pWildSequence;    // Points to prospective match after '*'
   uint8_t *pTameSequence;    // Points to prospective match in tame content

   // Find a first wildcard, if one exists, and the beginning of any 
   // prospectively matching sequence after it.
   do
   {
      // Check for the end from the start.  Get out fast, if possible.
      if (lenTame <= iWild || !*pTame)
      {
         if (lenWild > iWild && *pWild)
         {
            while (*(pWild++) == '*')
            {
               if (lenWild <= ++iWild)
               {
                  return true;     // "ab*" matches "ab".
               }
            }

             return false;         // "abcd" doesn't match "abc".
         }
         else
         {
            return true;           // "abc" matches "abc".
         }
      }
      else if (lenWild <= iWild)
      {
         return false;             // "abc" doesn't match "abcd".
      }
      else if (*pWild == '*')
      {
         // Got wild: set up for the second loop and skip on down there.
         iTame = iWild;

         while (CodePointAdvanceUtf8(&pWild))
         {
            iWild++;

            if (*pWild == '*' && iWild < lenWild)
            {
               continue;
            }
            else
            {
               break;
            }
         }

         if (!*pWild)
         {
            return true;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*bc" doesn't match "ab".
               }

               iTame++;
            }
         }

         // Keep fallback positions for retry in case of incomplete match.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
         iWildSequence = iWild;
         iTameSequence = iTame;
         break;
      }
      else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         return false;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
   } while (true);

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         iWild++;

         while (*(++pWild) == '*')
         {
            if (lenWild <= ++iWild)
            {
               return true;        // "ab**c*" matches "abcd".
            }
         }

         if (lenWild <= iWild || !*pWild)
         {
            return true;           // "ab*c*" matches "abcd".
         }

         if (lenTame <= iTame || !*pTame)
         {
            return false;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*b*c" doesn't match "ab".
               }

               iTame++;
            }
         }

         // Keep the new fallback positions.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
         iWildSequence = iWild;
         iTameSequence = iTame;
      }
      else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (lenTame <= iTame || !*pTame)
         {
            return false;          // "*bc" doesn't match "abcd".
         }

         // A fine time for questions.
         while (*pWildSequence == '?')
         {
            ++pWildSequence;
            ++pTameSequence;
            ++iWildSequence;
            ++iTameSequence;
         }

         // Fall back, but never so far again.
         pWild = (uint8_t *) pWildSequence;
         iWild = iWildSequence;

         while (!CodePointAdvanceAndCompareUtf8(pWild, 
                      (const uint8_t **) &pTameSequence))
         {
            if (lenTame <= ++iTameSequence || !*pTameSequence)
            {
               return false;       // "*a*b" doesn't match "ac".
            }
         }

         pTame = pTameSequence;
         iTame = iTameSequence;
      }

      // Another check for the end, at the end.
      if (lenTame <= iTame || !*pTame)
      {
         if (lenWild <= iWild || !*pWild)
         {
            return true;           // "*bc" matches "abc".
         }

         return false;             // "*bcd" doesn't match "abc".
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
      iTame++;
   } while (true);
}

// Case folds and compares null-terminated UTF-8 content, matching wildcards. 
// Accepts '?' as a single-code-point wildcard.  For each '*' wildcard, seeks 
// out a matching sequence of any code points beyond it.  Otherwise compares 
// the content a code point at a time.  PERFORMS NO UTF-8 VALIDATION.
//
bool WildCaseCompareUtf8(
            const uint8_t *pWild,   // Content (may include wildcards)
            const uint8_t *pTame)   // Content to compare (no wildcards)
{
   uint8_t *pWildSequence;    // Points to prospective match after '*'
   uint8_t *pTameSequence;    // Points to prospective match in tame content

   // Find a first wildcard, if one exists, and the beginning of any 
   // prospectively matching sequence after it.
   do
   {
      // Check for the end from the start.  Get out fast, if possible.
      if (!*pTame)
      {
         if (*pWild)
         {
            while (*pWild == '*')
            {
               if (!(*(++pWild)))
               {
                  return true;     // "ab*" matches "ab".
               }
            }

            return false;          // "abcd" doesn't match "abc".
         }
         else
         {
            return true;           // "abc" matches "abc".
         }
      }
      else if (*pWild == '*')
      {
         // Got wild: set up for the second loop and skip on down there.
         while (CodePointAdvanceUtf8(&pWild) && *pWild == '*')
         {
            continue;
         }

         if (!*pWild)
         {
            return true;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*bc" doesn't match "ab".
               }
            }
         }

         // Keep fallback positions for retry in case of incomplete match.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
         break;
      }
      else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         return false;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (true);

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         while (*(++pWild) == '*')
         {
            continue;
         }

         if (!*pWild)
         {
            return true;           // "ab*c*" matches "abcd".
         }

         if (!*pTame)
         {
            return false;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*b*c" doesn't match "ab".
               }
            }
         }

         // Keep the new fallback positions.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
      }
      else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (!*pTame)
         {
            return false;          // "*bc" doesn't match "abcd".
         }

         // A fine time for questions.
         while (*pWildSequence == '?')
         {
            ++pWildSequence;
            ++pTameSequence;
         }

         // Fall back, but never so far again.
         pWild = pWildSequence;

         while (!CodePointAdvanceAndCaseCompareUtf8(pWild, 
                      (const uint8_t **) &pTameSequence))
         {
            if (!*pTameSequence)
            {
               return false;       // "*a*b" doesn't match "ac".
            }
         }

         pTame = pTameSequence;
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            return true;           // "*bc" matches "abc".
         }
         else
         {
            return false;          // "*bcd" doesn't match "abc".
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (true);
}

// Case folds and compares UTF-8 content, up to a specified number of code 
// points, matching wildcards.  Accepts '?' as a single-code-point wildcard. 
// For each '*' wildcard, seeks out a matching sequence of any code points 
// beyond it.  Otherwise folds and compares the content a code point at a 
// time.  PERFORMS NO UTF-8 VALIDATION.
//
bool WildLenCaseCompareUtf8(
            const uint8_t *pWild,   // Content (may include wildcards)
            const uint8_t *pTame,   // Content to compare (no wildcards)
            int           lenWild,  // Count of code points in content
            int           lenTame)  // Code points in prospective match
{
   int     iWild = 0;         // Index for both inputs in upper loop
   int     iTame = 0;         // Index for tame content, used in lower loop
   int     iWildSequence;     // Index for prospective match after '*'
   int     iTameSequence;     // Index for match in tame content
   uint8_t *pWildSequence;    // Points to prospective match after '*'
   uint8_t *pTameSequence;    // Points to prospective match in tame content

   // Find a first wildcard, if one exists, and the beginning of any 
   // prospectively matching sequence after it.
   do
   {
      // Check for the end from the start.  Get out fast, if possible.
      if (lenTame <= iWild || !*pTame)
      {
         if (lenWild > iWild && *pWild)
         {
            while (*(pWild++) == '*')
            {
               if (lenWild <= ++iWild)
               {
                  return true;     // "ab*" matches "ab".
               }
            }

             return false;         // "abcd" doesn't match "abc".
         }
         else
         {
            return true;           // "abc" matches "abc".
         }
      }
      else if (lenWild <= iWild)
      {
         return false;             // "abc" doesn't match "abcd".
      }
      else if (*pWild == '*')
      {
         // Got wild: set up for the second loop and skip on down there.
         iTame = iWild;

         while (CodePointAdvanceUtf8(&pWild))
         {
            iWild++;

            if (*pWild == '*' && iWild < lenWild)
            {
               continue;
            }
            else
            {
               break;
            }
         }

         if (!*pWild)
         {
            return true;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*bc" doesn't match "ab".
               }

               iTame++;
            }
         }

         // Keep fallback positions for retry in case of incomplete match.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
         iWildSequence = iWild;
         iTameSequence = iTame;
         break;
      }
      else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         return false;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
   } while (true);

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         iWild++;

         while (*(++pWild) == '*')
         {
            if (lenWild <= ++iWild)
            {
               return true;        // "ab**c*" matches "abcd".
            }
         }

         if (lenWild <= iWild || !*pWild)
         {
            return true;           // "ab*c*" matches "abcd".
         }

         if (lenTame <= iTame || !*pTame)
         {
            return false;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return false;    // "a*b*c" doesn't match "ab".
               }

               iTame++;
            }
         }

         // Keep the new fallback positions.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
         iWildSequence = iWild;
         iTameSequence = iTame;
      }
      else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (lenTame <= iTame || !*pTame)
         {
            return false;          // "*bc" doesn't match "abcd".
         }

         // A fine time for questions.
         while (*pWildSequence == '?')
         {
            ++pWildSequence;
            ++pTameSequence;
            ++iWildSequence;
            ++iTameSequence;
         }

         // Fall back, but never so far again.
         pWild = pWildSequence;
         iWild = iWildSequence;

         while (!CodePointAdvanceAndCaseCompareUtf8(pWild, 
                      (const uint8_t **) &pTameSequence))
         {
            if (lenTame <= ++iTameSequence || !*pTameSequence)
            {
               return false;       // "*a*b" doesn't match "ac".
            }
         }

         pTame = pTameSequence;
         iTame = iTameSequence;
      }

      // Another check for the end, at the end.
      if (lenTame <= iTame || !*pTame)
      {
         if (lenWild <= iWild || !*pWild)
         {
            return true;           // "*bc" matches "abc".
         }

         return false;             // "*bcd" doesn't match "abc".
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
      iTame++;
   } while (true);
}

//
// The Wild[Len][Case]FindUtf8() family of functions shares a common design.  
// In each of the functions, processing is based around four "do" loops 
// controlled via continue and break.  The loops, in the order they appear in 
// the code, operate for each of these functions as follows:
//
// Either...
// 1: Handle any wildcards at the beginning of the search pattern.
// ...or...
// 2: Seek a match on the search pattern, prior to a '*' wildcard, by 
//    walking through mismatches between the content's code points and the 
//    search pattern's first (non-'*') code point.
// 3: Like the first loop in WildCompareUtf8(), prior to a '*' wildcard, 
//    walk through matches between the search pattern's code points and the 
//    content's code points.
// ...then...
// 4: Like the second loop in WildCompareUtf8(), after a '*' wildcard, walk 
//    through further matches, or backtrack for mismatches.
//
// Most of the function's variables are shared among all of those loops.  Any 
// compiler warnings that may appear, concerning their initialization status, 
// seem unjustified based on code inspection.
//
// A majority of each function's logic either (a) arranges the conditions 
// necessary for passing control into one or another of those loops, or (b) 
// arrives at an overall match / mismatch determination and appropriately sets 
// the values to be returned.  If loops 3 or 4 arrive at a mismatch that is 
// not definitive over the entire inbound content, their logic returns control 
// to the top of loop 2.  Conceptually, the function is designed as though 
// WildCompareUtf8() is situated within a larger loop that scans the inbound 
// content for any code point that may serve as the beginning of a match.  But 
// loop 2 is, by far, the least complex of the four loops, and the logic of 
// WildCompareUtf8() is modified only as needed to fit the wider context of 
// WildFindUtf8().
//

// Case-sensitive targeted wildcard search for null-terminated content.
//
// Given null-terminated UTF-8 content, and given a null-terminated UTF-8 
// search pattern that includes at least one '*' wildcard and that can include 
// '?' wildcards, searches the content for a match.  If a match is found, sets 
// *ppFirst, *ppLast, and *ppTarget as follows:
//
// *ppFirst is given as the beginning of the content to search.  If a match 
//    is found, *ppFirst will point to the location within the content where 
//    the whole match begins,
// *ppLast will point to the location within the content where the whole 
//    match ends, and
// *ppTarget will point to the location where the last wildcard-matching 
//    portion of the content begins, i.e., the content corresponding to the 
//    the last '*' wildcard in the search pattern.
//
// Returns a pointer to the content corresponding to the first matching '*' 
// wildcard in the search pattern.  If no match is found, or if no first 
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to nullptr and 
// returns nullptr.
//
uint8_t * WildFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget)      // Returned location after last '*'
{
   uint8_t *pTame;           // Code point in content to search
   uint8_t *pWild;           // Code point in search pattern
   uint8_t *pTameMatch;      // Prospective *ppFirst match in content
   uint8_t *pWildMatch;      // Prospective match in content with first '*'
   uint8_t *pTameSequence;   // Prospective match in content after '*'
   uint8_t *pWildSequence;   // Prospective match in pattern after '*'
   int     iQCount;          // Length of target that consists of '?' wildcards

   if (ppFirst && pSearchPattern)
   {
      pTame = *ppFirst;
   }
   else
   {
      if (ppLast)                  // Got empty input.
      {
         *ppLast = nullptr;
      }

      if (ppTarget)
      {
         *ppTarget = nullptr;
      }

      return nullptr;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = nullptr;

      do
      {
         if (*pSearchPattern == '*')
         {
            if (!pWildMatch)
            {
                pWildMatch = pTame;
            }

            // Keep fallback positions for retry in case of incomplete match.
            ++pSearchPattern;
            pWildSequence = (uint8_t *) pSearchPattern;
            pTameSequence = (uint8_t *) pTame;
         }
         else if (*pSearchPattern == '?')
         {
            if (!pTameMatch)
            {
               pTameMatch = pTame;
            }

            ++pSearchPattern;
            CodePointAdvanceUtf8((const uint8_t **) &pTame);

            if (!*pTame)
            {
               if (*pSearchPattern)
               {
                  while (*pSearchPattern == '*')
                  {
                     pTameSequence = pTame;
                     pSearchPattern++;
                  }
               }

               if (*pSearchPattern)  // "?*?" doesn't match "a".
               {
                   pTame = pTameMatch = pWildMatch = nullptr;
               }
               else                // "*" matches "a".
               {
                  CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);
               }

               if (ppTarget) 
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               if (ppLast)
               {
                   *ppLast = pTame;
               }

               *ppFirst = pTameMatch;
               return pWildMatch;
            }

            ++iQCount;

            if (!pWildMatch)
            {
               pWildSequence = nullptr;
            }
         }
         else if (!*pSearchPattern)
         {
            if (ppTarget)          // "*" matches "a".
            {
               *ppTarget = pTame;
            }

            while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
            {
                continue;
            }

            CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

            if (ppLast)
            {
                *ppLast = pTame;
            }

            if (ppTarget)
            {
                *ppTarget = pTame;
                iQCount--;

                while (iQCount && CodePointBacktrackUtf8(
                         (const uint8_t **) ppTarget, *ppFirst))
                {
                    iQCount--;
                }
            }

            *ppFirst = pTameMatch ? pTameMatch : pWildMatch;
            return pWildMatch;
         }
         else
         {
            break;
         }
      } while (true);

      if (!pTameMatch)
      {
         pTameMatch = pTame;
      }

      pWild = (uint8_t *) pSearchPattern;
      iQCount = 0;
   }
   else
   {
      // Find a code point in the content that matches the first non-'*' code 
	  // point in the search pattern.
      seek_pattern:
      do
      {
         if (CodePointCompareUtf8(pSearchPattern, pTame))
         {
            break;
         }

         if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
         {
            if (ppLast)
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      } while (true);

      pTameSequence = pWildMatch = pTameMatch = nullptr;
      pWild = (uint8_t *) pSearchPattern;
      
      // Find a first '*' wildcard, if one exists, and the beginning of any 
      // prospectively matching sequence after it.
      do
      {
         // Check for the end from the start.  Get out fast, if possible.
         if (!*pTame)
         {
            if (*pWild)
            {
               while (*pWild == '*')
               {
                  if (!(*(++pWild)))
                  {
                     if (ppLast)   // "ab*" matches "ab".
                     {
                        *ppLast = pTame;
                        CodePointBacktrackUtf8(
                           (const uint8_t **) ppLast, *ppFirst);
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch ? pWildMatch : pTame;
                  }
               }
      
               if (ppLast)         // "abcd" doesn't match "abc".
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }

               *ppFirst = nullptr;
               return nullptr;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "abc" matches "abc".
               {
                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               if (ppTarget)
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
            else
            {
                if (ppTarget)      // No wildcard found.
                {
                    *ppTarget = nullptr;
                }

                *ppFirst = nullptr;
                return pWildMatch;
            }
         }
         else if (*pWild == '*')
         {
            // Set up for the fourth "do" loop and skip on down there.
            pWildMatch = pTame;

            while (CodePointAdvanceUtf8((const uint8_t **) &pWild) && 
                         *pWild == '*')
            {
               continue;
            }

            if (!*pWild)
            {
               if (ppTarget)       // "ab*" matches "abc".
               {
                  *ppTarget = pTame;
               }

               if (ppLast)
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }

            // Search for the next prospective match.
            if (*pWild != '?')
            {
               while (!CodePointCompareUtf8(pWild, pTame))
               {
                  if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     if (pTameMatch)   // "a*bc" doesn't match "ab".
                     {
                        pTame = pTameMatch;
                        CodePointAdvanceUtf8((const uint8_t **) &pTame);
                     }

                     goto seek_pattern;
                  }
               }
            }

            // Keep fallback positions.
            pWildSequence = pWild;
            pTameSequence = pTame;
            break;
         }
         else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
         {
            if (pTameMatch)            // "abc" doesn't match "abd".
            {
               pTame = pTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            goto seek_pattern;
         }
         else if (!pTameMatch)
         {
            pTameMatch = pTame;        // Got prospective match on search pattern.
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
      } while (true);
   }

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         while (*(++pWild) == '*')
         {
            continue;
         }

         if (!pWildMatch)
         {
            pWildMatch = pTame;
         }

         if (!*pWild)
         {
            if (ppTarget)          // "a*c*" matches "abcd".
            {
               *ppTarget = pTame;
            }

            if (ppLast)
            {
               while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  continue;
               }

               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }

         if (!*pTame)
         {
            if (ppLast)            // "*bcd*" doesn't match "abc".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't match "ab".
                  {
                     pTame = pTameMatch;
                  }

                  CodePointAdvanceUtf8((const uint8_t **) &pTame);
                  goto seek_pattern;
               }
            }
         }

         // Keep the new fallback positions.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
      }
      else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (!*pTame)
         {
            if (ppLast)            // "*bc" doesn't match "abcd".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         if (!*pWild)
         {
            // A fine time for questions.
            iQCount = 0;

            if (pWildSequence)
            {
               while (*pWildSequence == '?')
               {
                  ++pWildSequence;
                  ++iQCount;
                  CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
               }

               if (!*pWildSequence)   
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  if (iQCount)
                  {
                     CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

                     if (ppLast)   // "a*?" matches "abcd".
                     {
                        *ppLast = pTame;
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                        iQCount--;

                        while (iQCount && CodePointBacktrackUtf8(
                           (const uint8_t **) ppTarget, *ppFirst))
                        {
                           iQCount--;
                        }
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch;
                  }

                  if (ppLast)      // "a*?d" matches "abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" matches "abcd".
                  {
                     *ppLast = pTame;
                     CodePointBacktrackUtf8((const uint8_t **) ppLast, 
                        *ppFirst);
                  }

                  if (ppTarget)
                  {
                     *ppTarget = pTameSequence;
                  }
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
			else
            {
               if (ppLast)         // No wildcard found.
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         // Another fine time for questions.
         if (pWildSequence)
         {
            while (*pWildSequence == '?')
            {
               CodePointAdvanceUtf8((const uint8_t **) &pWildSequence);
               CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
            }

            // Fall back, but never so far again.
            pWild = pWildSequence;

            while (!CodePointAdvanceAndCompareUtf8(pWild, 
                         (const uint8_t **) &pTameSequence))
            {
               if (!*pTameSequence)
               {
                  if (ppLast)      // "*a*b" doesn't match "ac".
                  {
                     *ppLast = nullptr;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = nullptr;
                  }

                  *ppFirst = nullptr;
                  return nullptr;
               }
            }

            pTame = pTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't match "ab".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            if (ppLast)            // "*bc" matches "abc".
            {
               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }
                  
            if (ppTarget)
            {
               *ppTarget = pTameSequence ? pTameSequence : pTame;
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }
         else
         {
            if (ppLast)            // "*bcd" doesn't match "abc".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
   } while (true);
}

// Case-sensitive targeted wildcard search for length-limited content.
//
// Given length-limited UTF-8 content, and given a length-limited UTF-8 search 
// pattern that includes at least one '*' wildcard and that can include '?' 
// wildcards, searches the content for a match.  If a match is found, sets 
// *ppFirst, *ppLast, and *ppTarget as follows:
//
// *ppFirst is given as the beginning of the content to search.  If a match 
//    is found, *ppFirst will point to the location within the content where 
//    the whole match begins,
// *ppLast will point to the location within the content where the whole 
//    match ends, and
// *ppTarget will point to the location where the last wildcard-matching 
//    portion of the content begins, i.e., the content corresponding to the 
//    the last '*' wildcard in the search pattern.
//
// Returns a pointer to the content corresponding to the first matching '*' 
// wildcard in the search pattern.  If no match is found, or if no first 
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to nullptr and 
// returns nullptr.
//
uint8_t * WildLenFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       int           lenContent,      // Count of code points in content
       int           lenPattern,      // Code points in search pattern
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget)      // Returned location after last '*'
{
   int     iTame;           // Index for content to search
   int     iWild;           // Index for search pattern
   int     iSearchPattern;  // Index for pattern start (after any leading '*')
   int     iTameMatch;      // Index for prospective *ppFirst match in content
   int     iWildSequence;   // Index for prospective match in pattern after '*'
   int     iTameSequence;   // Index for match in content to search
   int     iQCount;         // Length of target that consists of '?' wildcards
   uint8_t *pTame;          // Code point in content to search
   uint8_t *pWild;          // Code point in search pattern
   uint8_t *pTameMatch;     // Prospective *ppFirst match in content
   uint8_t *pWildMatch;     // Prospective match in content with first '*'
   uint8_t *pTameSequence;  // Prospective match in content after '*'
   uint8_t *pWildSequence;  // Prospective match in pattern after '*'

   if (ppFirst && pSearchPattern)
   {
      iTame = iSearchPattern = 0;
      pTame = *ppFirst;
   }
   else
   {
      if (ppLast)                  // Got empty input.
      {
         *ppLast = nullptr;
      }

      if (ppTarget)
      {
         *ppTarget = nullptr;
      }

      return nullptr;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = nullptr;

      do
      {
         if (*pSearchPattern == '*')
         {
            if (!pWildMatch)
            {
                pWildMatch = pTame;
            }

            // Keep fallback positions for retry in case of incomplete match.
            ++pSearchPattern;
            ++iSearchPattern;
            pWildSequence = (uint8_t *) pSearchPattern;
            iWildSequence = iSearchPattern;
            pTameSequence = (uint8_t *) pTame;
            iTameSequence = iTame;
         }
         else if (*pSearchPattern == '?')
         {
            if (!pTameMatch)
            {
               pTameMatch = pTame;
               iTameMatch = iTame;
            }

            ++pSearchPattern;
            ++iSearchPattern;
            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            ++iTame;
            ++iQCount;

            if (lenContent <= iTame || !*pTame)
            {
               if (lenPattern > iSearchPattern && *pSearchPattern)
               {
                  while (*pSearchPattern == '*')
                  {
                     pTameSequence = pTame;
                     iTameSequence = iTame;
                     ++pSearchPattern;
                     ++iSearchPattern;
                  }
               }

               if (*pSearchPattern)  // "?*?" doesn't match "a".
               {
                   pTame = pTameMatch = pWildMatch = nullptr;
               }
               else                // "*" matches "a".
               {
                  CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);
               }

               if (ppTarget) 
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               if (ppLast)
               {
                   *ppLast = pTame;
               }

               *ppFirst = pTameMatch;
               return pWildMatch;
            }

            if (!pWildMatch)
            {
               pWildSequence = nullptr;
               iWildSequence = 0;
            }
         }
         else if (lenPattern <= iSearchPattern || !*pSearchPattern)
         {
            if (ppTarget)          // "*" matches "a".
            {
               *ppTarget = pTame;
            }

            while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
            {
                continue;
            }

            CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

            if (ppLast)
            {
                *ppLast = pTame;
            }

            if (ppTarget)
            {
                *ppTarget = pTame;
                iQCount--;

                while (iQCount && CodePointBacktrackUtf8(
                         (const uint8_t **) ppTarget, *ppFirst))
                {
                    iQCount--;
                }
            }

            *ppFirst = pTameMatch ? pTameMatch : pWildMatch;
            return pWildMatch;
         }
         else
         {
            break;
         }
      } while (true);

      if (!pTameMatch)
      {
         pTameMatch = pTame;
      }

      pWild = (uint8_t *) pSearchPattern;
      iWild = iSearchPattern;
      iQCount = 0;
   }
   else
   {
      // Find a code point in the content that matches the first non-'*' code 
	  // point in the search pattern.
      seek_pattern:
      do
      {
         if (CodePointCompareUtf8(pSearchPattern, pTame))
         {
            break;
         }

          if (CodePointAdvanceUtf8((const uint8_t **) &pTame))
          {
             ++iTame;
          }
          else
          {
             if (ppLast)
             {
                *ppLast = nullptr;
             }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      } while (true);

      pTameSequence = pWildMatch = pTameMatch = nullptr;
      pWild = (uint8_t *) pSearchPattern;
      iWild = iSearchPattern;

      // Find a first '*' wildcard, if one exists, and the beginning of any 
      // prospectively matching sequence after it.
      do
      {
         // Check for the end from the start.  Get out fast, if possible.
         if (lenContent <= iTame || !*pTame)
         {
            if (lenPattern > iWild && *pWild)
            {
               while (*pWild == '*')
               {
                  if (lenPattern <= ++iWild)
                  {
                     if (ppLast)   // "ab*" matches "ab".
                     {
                        *ppLast = pTame;
                        CodePointBacktrackUtf8(
                           (const uint8_t **) ppLast, *ppFirst);
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch ? pWildMatch : pTame;
                  }
               }

               if (ppLast)         // "abcd" doesn't match "abc".
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }

               *ppFirst = nullptr;
               return nullptr;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "abc" matches "abc".
               {
                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               if (ppTarget)
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
            else
            {
                if (ppTarget)      // No wildcard found.
                {
                    *ppTarget = nullptr;
                }

                *ppFirst = nullptr;
                return pWildMatch;
            }
         }
         else if (lenPattern <= iWild)
         {
            if (pTameMatch)        // "abc" doesn't match "abcd".
            {
               pTame = pTameMatch;
               iTame = iTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            goto seek_pattern;
         }
         else if (*pWild == '*')
         {
            // Set up for the fourth "do" loop and skip on down there.
            pWildMatch = pTame;

            while (CodePointAdvanceUtf8((const uint8_t **) &pWild))
            {
               iWild++;

               if (*pWild == '*' && iWild < lenPattern)
               {
                  continue;
               }
               else
               {
                  break;
               }
            }

            if (!*pWild)
            {
               if (ppTarget)       // "ab*" matches "abc".
               {
                  *ppTarget = pTame;
               }

               if (ppLast)
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }

            // Search for the next prospective match.
            if (*pWild != '?')
            {
               while (!CodePointCompareUtf8(pWild, pTame))
               {
                  if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     if (pTameMatch)  // "a*bc" doesn't match "ab".
                     {
                        pTame = pTameMatch;
                        CodePointAdvanceUtf8((const uint8_t **) &pTame);
                        iTame = 1 + iTameMatch;
                     }

                     goto seek_pattern;
                  }

                  iTame++;
               }
            }

            // Keep fallback positions.
            pWildSequence = (uint8_t *) pWild;
            pTameSequence = (uint8_t *) pTame;
            iWildSequence = iWild;
            iTameSequence = iTame;
            break;
         }
         else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
         {
            if (pTameMatch)        // "abc" doesn't match "abd".
            {
               pTame = pTameMatch;
               iTame = iTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            iTame++;
            goto seek_pattern;
         }
         else if (!pTameMatch)
         {
            pTameMatch = pTame;    // Got prospective match on search pattern.
            iTameMatch = iTame;
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
         iWild++;
         iTame++;
      } while (true);
   }

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         iWild++;

         if (!pWildMatch)
         {
            pWildMatch = pTame;
         }

         while (*(++pWild) == '*')
         {
            if (lenPattern <= ++iWild)
            {
               if (ppLast)         // "ab**c*" matches "abcd".
               {
                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               if (ppTarget)
               {
                  *ppTarget = pTame;
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
         }

         if (lenPattern <= iWild || !*pWild)
         {
            if (ppTarget)          // "a*c*" matches "abcd".
            {
               *ppTarget = pTame;
            }

            if (ppLast)
            {
               while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  continue;
               }

               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }

         if (lenContent <= iTame || !*pTame)
         {
            if (ppLast)            // "*bcd*" doesn't match "abc".
            {
               *ppLast = nullptr;
            }
                  
            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr; 
            return nullptr;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't match "ab".
                  {
                     pTame = pTameMatch;
                     iTame = iTameMatch;
                  }

                  CodePointAdvanceUtf8((const uint8_t **) &pTame);
                  iTame++;
                  goto seek_pattern;
               }

               iTame++;
            }
         }

         // Keep the new fallback positions.
         pWildSequence = pWild;
         pTameSequence = pTame;
         iWildSequence = iWild;
         iTameSequence = iTame;
      }
      else if (!CodePointCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (lenContent <= iTame || !*pTame)
         {
            if (ppLast)            // "*bc" doesn't match "abcd".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr; 
            return nullptr;
         }

         if (lenPattern <= iWild || !*pWild)
         {
            // A fine time for questions.
            iQCount = 0;

            if (pWildSequence)
            {
               while (*pWildSequence == '?')
               {
                  ++pWildSequence;
                  ++iQCount;
                  CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
               }

               if (!*pWildSequence)   
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  if (iQCount)
                  {
                     CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

                     if (ppLast)   // "a*?" matches "abcd".
                     {
                        *ppLast = pTame;
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                        iQCount--;
			     
                        while (iQCount && CodePointBacktrackUtf8(
                           (const uint8_t **) ppTarget, *ppFirst))
                        {
                           iQCount--;
                        }
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch;
                  }

                  if (ppLast)      // "a*?d" matches "abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" matches "abcd".
                  {
                     *ppLast = pTame;
                     CodePointBacktrackUtf8((const uint8_t **) ppLast, 
                        *ppFirst);
                  }

                  if (ppTarget)
                  {
                     *ppTarget = pTameSequence;
                  }
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
			else
            {
               if (ppLast)         // No wildcard found.
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         // Another fine time for questions.
         if (pWildSequence)
         {
            while (*pWildSequence == '?')
            {
               CodePointAdvanceUtf8((const uint8_t **) &pWildSequence);
               CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
               ++iWildSequence;
               ++iTameSequence;
            }

            // Fall back, but never so far again.
            pWild = pWildSequence;
            iWild = iWildSequence;

            while (!CodePointAdvanceAndCompareUtf8(pWild, 
                      (const uint8_t **) &pTameSequence))
            {
               if (lenContent <= ++iTameSequence || !*pTameSequence)
               {
                  if (ppLast)      // "*a*b" doesn't match "ac".
                  {
                     *ppLast = nullptr;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = nullptr;
                  }

                  *ppFirst = nullptr; 
                  return nullptr;
               }
            }

            pTame = pTameSequence;
            iTame = iTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't match "ab".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      }

      // Another check for the end, at the end.
      if (lenContent <= iTame || !*pTame)
      {
         if (lenPattern <= iWild || !*pWild)
         {
            if (ppLast)            // "*bc" matches "abc".
            {
               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }

            if (ppTarget)
            {
               *ppTarget = pTameSequence ? pTameSequence : pTame;
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }
         else
         {
            if (ppLast)            // "*bcd" doesn't match "abc".
            {
               *ppLast = nullptr;
            }
                  
            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr; 
            return nullptr;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
      iWild++;
      iTame++;
   } while (true);
}

// Case-insensitive targeted wildcard search for null-terminated content.
//
// Given null-terminated UTF-8 content, and given a null-terminated UTF-8 
// search pattern that includes at least one '*' wildcard and that can include 
// '?' wildcards, searches the content for a case-insensitive match.  If a 
// match is found, sets *ppFirst, *ppLast, and *ppTarget as follows:
//
// *ppFirst is given as the beginning of the content to search.  If a match 
//    is found, *ppFirst will point to the location within the content where 
//    the whole match begins,
// *ppLast will point to the location within the content where the whole 
//    match ends, and
// *ppTarget will point to the location where the last wildcard-matching 
//    portion of the content begins, i.e., the content corresponding to the 
//    the last '*' wildcard in the search pattern.
//
// Returns a pointer to the content corresponding to the first matching '*' 
// wildcard in the search pattern.  If no match is found, or if no first 
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to nullptr and 
// returns nullptr.
//
uint8_t * WildCaseFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget)      // Returned location after last '*'
{
   uint8_t *pTame;           // Code point in content to search
   uint8_t *pWild;           // Code point in search pattern
   uint8_t *pTameMatch;      // Prospective *ppFirst match in content
   uint8_t *pWildMatch;      // Prospective match in content with first '*'
   uint8_t *pTameSequence;   // Prospective match in content after '*'
   uint8_t *pWildSequence;   // Prospective match in pattern after '*'
   int     iQCount;          // Length of target that consists of '?' wildcards

   if (ppFirst && pSearchPattern)
   {
      pTame = *ppFirst;
   }
   else
   {
      if (ppLast)                  // Got empty input.
      {
         *ppLast = nullptr;
      }

      if (ppTarget)
      {
         *ppTarget = nullptr;
      }

      return nullptr;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = nullptr;

      do
      {
         if (*pSearchPattern == '*')
         {
            if (!pWildMatch)
            {
                pWildMatch = pTame;
            }

            // Keep fallback positions for retry in case of incomplete match.
            ++pSearchPattern;
            pWildSequence = (uint8_t *) pSearchPattern;
            pTameSequence = (uint8_t *) pTame;
         }
         else if (*pSearchPattern == '?')
         {
            if (!pTameMatch)
            {
               pTameMatch = pTame;
            }

            ++pSearchPattern;
            CodePointAdvanceUtf8((const uint8_t **) &pTame);

            if (!*pTame)
            {
               if (*pSearchPattern)
               {
                  while (*pSearchPattern == '*')
                  {
                     pTameSequence = pTame;
                     pSearchPattern++;
                  }
               }

               if (*pSearchPattern)  // "?*?" doesn't match "a".
               {
                   pTame = pTameMatch = pWildMatch = nullptr;
               }
               else                // "*" matches "a".
               {
                  CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);
               }

               if (ppTarget) 
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               if (ppLast)
               {
                   *ppLast = pTame;
               }

               *ppFirst = pTameMatch;
               return pWildMatch;
            }

            ++iQCount;

            if (!pWildMatch)
            {
               pWildSequence = nullptr;
            }
         }
         else if (!*pSearchPattern)
         {
            if (ppTarget)          // "*" matches "a".
            {
               *ppTarget = pTame;
            }

            while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
            {
                continue;
            }

            CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

            if (ppLast)
            {
                *ppLast = pTame;
            }

            if (ppTarget)
            {
                *ppTarget = pTame;
                iQCount--;

                while (iQCount && CodePointBacktrackUtf8(
                         (const uint8_t **) ppTarget, *ppFirst))
                {
                    iQCount--;
                }
            }

            *ppFirst = pTameMatch ? pTameMatch : pWildMatch;
            return pWildMatch;
         }
         else
         {
            break;
         }
      } while (true);

      if (!pTameMatch)
      {
         pTameMatch = pTame;
      }

      pWild = (uint8_t *) pSearchPattern;
      iQCount = 0;
   }
   else
   {
      // Find a code point in the content that matches the first non-'*' code 
	  // point in the search pattern.
      seek_pattern:
      do
      {
         if (CodePointCaseCompareUtf8(pSearchPattern, pTame))
         {
            break;
         }

         if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
         {
            if (ppLast)
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      } while (true);

      pTameSequence = pWildMatch = pTameMatch = nullptr;
      pWild = (uint8_t *) pSearchPattern;
      
      // Find a first '*' wildcard, if one exists, and the beginning of any 
      // prospectively matching sequence after it.
      do
      {
         // Check for the end from the start.  Get out fast, if possible.
         if (!*pTame)
         {
            if (*pWild)
            {
               while (*pWild == '*')
               {
                  if (!(*(++pWild)))
                  {
                     if (ppLast)   // "ab*" matches "ab".
                     {
                        *ppLast = pTame;
                        CodePointBacktrackUtf8(
                           (const uint8_t **) ppLast, *ppFirst);
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch ? pWildMatch : pTame;
                  }
               }
      
               if (ppLast)         // "abcd" doesn't match "abc".
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }

               *ppFirst = nullptr;
               return nullptr;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "abc" matches "abc".
               {
                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               if (ppTarget)
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
            else
            {
                if (ppTarget)      // No wildcard found.
                {
                    *ppTarget = nullptr;
                }

                *ppFirst = nullptr;
                return pWildMatch;
            }
         }
         else if (*pWild == '*')
         {
            // Set up for the fourth "do" loop and skip on down there.
            pWildMatch = pTame;

            while (CodePointAdvanceUtf8((const uint8_t **) &pWild) && 
                         *pWild == '*')
            {
               continue;
            }

            if (!*pWild)
            {
               if (ppTarget)       // "ab*" matches "abc".
               {
                  *ppTarget = pTame;
               }

               if (ppLast)
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }

            // Search for the next prospective match.
            if (*pWild != '?')
            {
               while (!CodePointCaseCompareUtf8(pWild, pTame))
               {
                  if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     if (pTameMatch)   // "a*bc" doesn't match "ab".
                     {
                        pTame = pTameMatch;
                        CodePointAdvanceUtf8((const uint8_t **) &pTame);
                     }

                     goto seek_pattern;
                  }
               }
            }

            // Keep fallback positions.
            pWildSequence = pWild;
            pTameSequence = pTame;
            break;
         }
         else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
         {
            if (pTameMatch)            // "abc" doesn't match "abd".
            {
               pTame = pTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            goto seek_pattern;
         }
         else if (!pTameMatch)
         {
            pTameMatch = pTame;        // Got prospective match on search pattern.
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
      } while (true);
   }

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         while (*(++pWild) == '*')
         {
            continue;
         }

         if (!pWildMatch)
         {
            pWildMatch = pTame;
         }

         if (!*pWild)
         {
            if (ppTarget)          // "a*c*" matches "abcd".
            {
               *ppTarget = pTame;
            }

            if (ppLast)
            {
               while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  continue;
               }

               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }

         if (!*pTame)
         {
            if (ppLast)            // "*bcd*" doesn't match "abc".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't match "ab".
                  {
                     pTame = pTameMatch;
                  }

                  CodePointAdvanceUtf8((const uint8_t **) &pTame);
                  goto seek_pattern;
               }
            }
         }

         // Keep the new fallback positions.
         pWildSequence = (uint8_t *) pWild;
         pTameSequence = (uint8_t *) pTame;
      }
      else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (!*pTame)
         {
            if (ppLast)            // "*bc" doesn't match "abcd".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         if (!*pWild)
         {
            // A fine time for questions.
            iQCount = 0;

            if (pWildSequence)
            {
               while (*pWildSequence == '?')
               {
                  ++pWildSequence;
                  ++iQCount;
                  CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
               }

               if (!*pWildSequence)   
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  if (iQCount)
                  {
                     CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

                     if (ppLast)   // "a*?" matches "abcd".
                     {
                        *ppLast = pTame;
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                        iQCount--;

                        while (iQCount && CodePointBacktrackUtf8(
                           (const uint8_t **) ppTarget, *ppFirst))
                        {
                           iQCount--;
                        }
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch;
                  }

                  if (ppLast)      // "a*?d" matches "abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" matches "abcd".
                  {
                     *ppLast = pTame;
                     CodePointBacktrackUtf8((const uint8_t **) ppLast, 
                        *ppFirst);
                  }

                  if (ppTarget)
                  {
                     *ppTarget = pTameSequence;
                  }
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
			else
            {
               if (ppLast)         // No wildcard found.
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         // Another fine time for questions.
         if (pWildSequence)
         {
            while (*pWildSequence == '?')
            {
               CodePointAdvanceUtf8((const uint8_t **) &pWildSequence);
               CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
            }

            // Fall back, but never so far again.
            pWild = pWildSequence;

            while (!CodePointAdvanceAndCaseCompareUtf8(pWild,
                      (const uint8_t **) &pTameSequence))
            {
               if (!*pTameSequence)
               {
                  if (ppLast)      // "*a*b" doesn't match "ac".
                  {
                     *ppLast = nullptr;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = nullptr;
                  }

                  *ppFirst = nullptr;
                  return nullptr;
               }
            }

            pTame = pTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't match "ab".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            if (ppLast)            // "*bc" matches "abc".
            {
               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }
                  
            if (ppTarget)
            {
               *ppTarget = pTameSequence ? pTameSequence : pTame;
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }
         else
         {
            if (ppLast)            // "*bcd" doesn't match "abc".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
   } while (true);
}

// Case-insensitive targeted wildcard search for length-limited content.
//
// Given length-limited UTF-8 content, and given a length-limited UTF-8 search 
// pattern that includes at least one '*' wildcard and that can include '?' 
// wildcards, searches the content for a case-insensitive match.  If a match 
// is found, sets *ppFirst, *ppLast, and *ppTarget as follows:
//
// *ppFirst is given as the beginning of the content to search.  If a match 
//    is found, *ppFirst will point to the location within the content where 
//    the whole match begins,
// *ppLast will point to the location within the content where the whole 
//    match ends, and
// *ppTarget will point to the location where the last wildcard-matching 
//    portion of the content begins, i.e., the content corresponding to the 
//    the last '*' wildcard in the search pattern.
//
// Returns a pointer to the content corresponding to the first matching '*' 
// wildcard in the search pattern.  If no match is found, or if no first 
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to nullptr and 
// returns nullptr.
//
uint8_t * WildLenCaseFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       int           lenContent,      // Count of code points in content
       int           lenPattern,      // Code points in search pattern
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget)      // Returned location after last '*'
{
   int     iTame;           // Index for content to search
   int     iWild;           // Index for search pattern
   int     iSearchPattern;  // Index for pattern start (after any leading '*')
   int     iTameMatch;      // Index for prospective *ppFirst match in content
   int     iWildSequence;   // Index for prospective match in pattern after '*'
   int     iTameSequence;   // Index for match in content to search
   int     iQCount;         // Length of target that consists of '?' wildcards
   uint8_t *pTame;          // Code point in content to search
   uint8_t *pWild;          // Code point in search pattern
   uint8_t *pTameMatch;     // Prospective *ppFirst match in content
   uint8_t *pWildMatch;     // Prospective match in content with first '*'
   uint8_t *pTameSequence;  // Prospective match in content after '*'
   uint8_t *pWildSequence;  // Prospective match in pattern after '*'

   if (ppFirst && pSearchPattern)
   {
      iTame = iSearchPattern = 0;
      pTame = *ppFirst;
   }
   else
   {
      if (ppLast)                  // Got empty input.
      {
         *ppLast = nullptr;
      }

      if (ppTarget)
      {
         *ppTarget = nullptr;
      }

      return nullptr;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = nullptr;

      do
      {
         if (*pSearchPattern == '*')
         {
            if (!pWildMatch)
            {
                pWildMatch = pTame;
            }

            // Keep fallback positions for retry in case of incomplete match.
            ++pSearchPattern;
            ++iSearchPattern;
            pWildSequence = (uint8_t *) pSearchPattern;
            iWildSequence = iSearchPattern;
            pTameSequence = (uint8_t *) pTame;
            iTameSequence = iTame;
         }
         else if (*pSearchPattern == '?')
         {
            if (!pTameMatch)
            {
               pTameMatch = pTame;
               iTameMatch = iTame;
            }

            ++pSearchPattern;
            ++iSearchPattern;
            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            ++iTame;
            ++iQCount;

            if (lenContent <= iTame || !*pTame)
            {
               if (lenPattern > iSearchPattern && *pSearchPattern)
               {
                  while (*pSearchPattern == '*')
                  {
                     pTameSequence = pTame;
                     iTameSequence = iTame;
                     ++pSearchPattern;
                     ++iSearchPattern;
                  }
               }

               if (*pSearchPattern)  // "?*?" doesn't match "a".
               {
                   pTame = pTameMatch = pWildMatch = nullptr;
               }
               else                // "*" matches "a".
               {
                  CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);
               }

               if (ppTarget) 
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               if (ppLast)
               {
                   *ppLast = pTame;
               }

               *ppFirst = pTameMatch;
               return pWildMatch;
            }

            if (!pWildMatch)
            {
               pWildSequence = nullptr;
               iWildSequence = 0;
            }
         }
         else if (lenPattern <= iSearchPattern || !*pSearchPattern)
         {
            if (ppTarget)          // "*" matches "a".
            {
               *ppTarget = pTame;
            }

            while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
            {
                continue;
            }

            CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

            if (ppLast)
            {
                *ppLast = pTame;
            }

            if (ppTarget)
            {
                *ppTarget = pTame;
                iQCount--;

                while (iQCount && CodePointBacktrackUtf8(
                         (const uint8_t **) ppTarget, *ppFirst))
                {
                    iQCount--;
                }
            }

            *ppFirst = pTameMatch ? pTameMatch : pWildMatch;
            return pWildMatch;
         }
         else
         {
            break;
         }
      } while (true);

      if (!pTameMatch)
      {
         pTameMatch = pTame;
      }

      pWild = (uint8_t *) pSearchPattern;
      iWild = iSearchPattern;
      iQCount = 0;
   }
   else
   {
      // Find a code point in the content that matches the first non-'*' code 
	  // point in the search pattern.
      seek_pattern:
      do
      {
         if (CodePointCaseCompareUtf8(pSearchPattern, pTame))
         {
            break;
         }

          if (CodePointAdvanceUtf8((const uint8_t **) &pTame))
          {
             ++iTame;
          }
          else
          {
             if (ppLast)
             {
                *ppLast = nullptr;
             }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      } while (true);

      pTameSequence = pWildMatch = pTameMatch = nullptr;
      pWild = (uint8_t *) pSearchPattern;
      iWild = iSearchPattern;

      // Find a first '*' wildcard, if one exists, and the beginning of any 
      // prospectively matching sequence after it.
      do
      {
         // Check for the end from the start.  Get out fast, if possible.
         if (lenContent <= iTame || !*pTame)
         {
            if (lenPattern > iWild && *pWild)
            {
               while (*pWild == '*')
               {
                  if (lenPattern <= ++iWild)
                  {
                     if (ppLast)   // "ab*" matches "ab".
                     {
                        *ppLast = pTame;
                        CodePointBacktrackUtf8(
                           (const uint8_t **) ppLast, *ppFirst);
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch ? pWildMatch : pTame;
                  }
               }

               if (ppLast)         // "abcd" doesn't match "abc".
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }

               *ppFirst = nullptr;
               return nullptr;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "abc" matches "abc".
               {
                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               if (ppTarget)
               {
                  *ppTarget = pTameSequence ? pTameSequence : pTame;
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
            else
            {
                if (ppTarget)      // No wildcard found.
                {
                    *ppTarget = nullptr;
                }

                *ppFirst = nullptr;
                return pWildMatch;
            }
         }
         else if (lenPattern <= iWild)
         {
            if (pTameMatch)        // "abc" doesn't match "abcd".
            {
               pTame = pTameMatch;
               iTame = iTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            goto seek_pattern;
         }
         else if (*pWild == '*')
         {
            // Set up for the fourth "do" loop and skip on down there.
            pWildMatch = pTame;

            while (CodePointAdvanceUtf8((const uint8_t **) &pWild))
            {
               iWild++;

               if (*pWild == '*' && iWild < lenPattern)
               {
                  continue;
               }
               else
               {
                  break;
               }
            }

            if (!*pWild)
            {
               if (ppTarget)       // "ab*" matches "abc".
               {
                  *ppTarget = pTame;
               }

               if (ppLast)
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }

            // Search for the next prospective match.
            if (*pWild != '?')
            {
               while (!CodePointCaseCompareUtf8(pWild, pTame))
               {
                  if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     if (pTameMatch)  // "a*bc" doesn't match "ab".
                     {
                        pTame = pTameMatch;
                        CodePointAdvanceUtf8((const uint8_t **) &pTame);
                        iTame = 1 + iTameMatch;
                     }

                     goto seek_pattern;
                  }

                  iTame++;
               }
            }

            // Keep fallback positions.
            pWildSequence = (uint8_t *) pWild;
            pTameSequence = (uint8_t *) pTame;
            iWildSequence = iWild;
            iTameSequence = iTame;
            break;
         }
         else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
         {
            if (pTameMatch)        // "abc" doesn't match "abd".
            {
               pTame = pTameMatch;
               iTame = iTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            iTame++;
            goto seek_pattern;
         }
         else if (!pTameMatch)
         {
            pTameMatch = pTame;    // Got prospective match on search pattern.
            iTameMatch = iTame;
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
         iWild++;
         iTame++;
      } while (true);
   }

   // Find any further wildcards and any further matching sequences.
   do
   {
      if (*pWild == '*')
      {
         // Got wild again.
         iWild++;

         if (!pWildMatch)
         {
            pWildMatch = pTame;
         }

         while (*(++pWild) == '*')
         {
            if (lenPattern <= ++iWild)
            {
               if (ppLast)         // "ab**c*" matches "abcd".
               {
                  *ppLast = pTame;
                  CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
               }

               if (ppTarget)
               {
                  *ppTarget = pTame;
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
         }

         if (lenPattern <= iWild || !*pWild)
         {
            if (ppTarget)          // "a*c*" matches "abcd".
            {
               *ppTarget = pTame;
            }

            if (ppLast)
            {
               while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  continue;
               }

               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }

         if (lenContent <= iTame || !*pTame)
         {
            if (ppLast)            // "*bcd*" doesn't match "abc".
            {
               *ppLast = nullptr;
            }
                  
            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr; 
            return nullptr;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't match "ab".
                  {
                     pTame = pTameMatch;
                     iTame = iTameMatch;
                  }

                  CodePointAdvanceUtf8((const uint8_t **) &pTame);
                  iTame++;
                  goto seek_pattern;
               }

               iTame++;
            }
         }

         // Keep the new fallback positions.
         pWildSequence = pWild;
         pTameSequence = pTame;
         iWildSequence = iWild;
         iTameSequence = iTame;
      }
      else if (!CodePointCaseCompareUtf8(pWild, pTame) && *pWild != '?')
      {
         // The equivalent portion of the upper loop is really simple.
         if (lenContent <= iTame || !*pTame)
         {
            if (ppLast)            // "*bc" doesn't match "abcd".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr; 
            return nullptr;
         }

         if (lenPattern <= iWild || !*pWild)
         {
            // A fine time for questions.
            iQCount = 0;

            if (pWildSequence)
            {
               while (*pWildSequence == '?')
               {
                  ++pWildSequence;
                  ++iQCount;
                  CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
               }

               if (!*pWildSequence)   
               {
                  while (CodePointAdvanceUtf8((const uint8_t **) &pTame))
                  {
                     continue;
                  }

                  if (iQCount)
                  {
                     CodePointBacktrackUtf8((const uint8_t **) &pTame, *ppFirst);

                     if (ppLast)   // "a*?" matches "abcd".
                     {
                        *ppLast = pTame;
                     }

                     if (ppTarget)
                     {
                        *ppTarget = pTame;
                        iQCount--;
			     
                        while (iQCount && CodePointBacktrackUtf8(
                           (const uint8_t **) ppTarget, *ppFirst))
                        {
                           iQCount--;
                        }
                     }

                     *ppFirst = pTameMatch ? pTameMatch : pTame;
                     return pWildMatch;
                  }

                  if (ppLast)      // "a*?d" matches "abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" matches "abcd".
                  {
                     *ppLast = pTame;
                     CodePointBacktrackUtf8((const uint8_t **) ppLast, 
                        *ppFirst);
                  }

                  if (ppTarget)
                  {
                     *ppTarget = pTameSequence;
                  }
               }

               *ppFirst = pTameMatch ? pTameMatch : pTame;
               return pWildMatch;
            }
			else
            {
               if (ppLast)         // No wildcard found.
               {
                  *ppLast = nullptr;
               }

               if (ppTarget)
               {
                  *ppTarget = nullptr;
               }
            }

            *ppFirst = nullptr;
            return nullptr;
         }

         // Another fine time for questions.
         if (pWildSequence)
         {
            while (*pWildSequence == '?')
            {
               CodePointAdvanceUtf8((const uint8_t **) &pWildSequence);
               CodePointAdvanceUtf8((const uint8_t **) &pTameSequence);
               ++iWildSequence;
               ++iTameSequence;
            }

            // Fall back, but never so far again.
            pWild = pWildSequence;
            iWild = iWildSequence;

            while (!CodePointAdvanceAndCaseCompareUtf8(pWild, 
                      (const uint8_t **) &pTameSequence))
            {
               if (lenContent <= ++iTameSequence || !*pTameSequence)
               {
                  if (ppLast)      // "*a*b" doesn't match "ac".
                  {
                     *ppLast = nullptr;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = nullptr;
                  }

                  *ppFirst = nullptr; 
                  return nullptr;
               }
            }

            pTame = pTameSequence;
            iTame = iTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't match "ab".
            {
               *ppLast = nullptr;
            }

            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr;
            return nullptr;
         }
      }

      // Another check for the end, at the end.
      if (lenContent <= iTame || !*pTame)
      {
         if (lenPattern <= iWild || !*pWild)
         {
            if (ppLast)            // "*bc" matches "abc".
            {
               *ppLast = pTame;
               CodePointBacktrackUtf8((const uint8_t **) ppLast, *ppFirst);
            }

            if (ppTarget)
            {
               *ppTarget = pTameSequence ? pTameSequence : pTame;
            }

            *ppFirst = pTameMatch ? pTameMatch : pTame;
            return pWildMatch;
         }
         else
         {
            if (ppLast)            // "*bcd" doesn't match "abc".
            {
               *ppLast = nullptr;
            }
                  
            if (ppTarget)
            {
               *ppTarget = nullptr;
            }

            *ppFirst = nullptr; 
            return nullptr;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
      iWild++;
      iTame++;
   } while (true);
}

#if defined(__cplusplus)

namespace FastUtf8
{

// An object of the FastUtf8::Uniseries class stores the following data:
//
//  A content buffer
//  An item of metadata
//
// The metadata item comprises a small set of flags and a length field.  It 
// fits into a single address on a 64-bit system, for speedy access.  Other 
// than the slight complexity involving that optimization, the class's methods 
// allow for easily comprehensible management of UTF-8 content and enable 
// these operations:
//
//  Creation, validation, duplication, and deletion of UTF-8 content
//  Slicing (substring extraction) and concatenation of UTF-8 content
//  Optimized slicing and concatenation of ASCII content
//  Separation of UTF-8 content based on UTF-8 separator tokens
//  Optimized separation of ASCII content based on ASCII tokens
//  Whole content comparison: case-sensitive and case-insensitive
//  Partial content (substring) comparison: case-sensitive and case-insensitive
//  Wildcard-based content comparison: case-sensitive and case-insensitive
//  Conversion of 8-bit ASCII text to equivalent UTF-8 content
//  Content length and size determination, in code points and in bytes
//  Trim (removal of outboard white space), index, and case fold operations
//
// The class provides an iterator that can be obtained via its begin() or 
// end() method.  An iterator obtained via end() refers to the position of a 
// terminating null at the end of the content buffer.  The iterator concepts 
// indicate the capability to access content by index, i.e., using operator[]. 
// But indexing into UTF-8 content, unless it comprises entirely 7-bit ASCII
// text, entails quite a performance drag.
//
// To find a code point using the increment and decrement operators, such as 
// operator++(), the necessary UTF-8 traversal involves calls to the C-style 
// functions CodePointAdvanceUtf8() and CodePointABacktrackUtf8(), found just 
// in this file.  Individual code point comparison via operator==() involves a 
// call to CodePointCompareUtf8(), also just in this file.  All other FastUtf8 
// functionality is performed by C-style functions either declared in 
// fastutf8.h (and also defined in this file) or available in the standard C 
// library.
//
FastUtf8::Uniseries::Uniseries(uint8_t *pInbound, bool bWrapBuffer)
{
   int    lenContent;
   bool   bIs7BitCharString;
   size_t nBytes = ValidateWithIs7BitUtf8(
                      pInbound, &lenContent, &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();
   this->m_metadata = 0;

   if (bWrapBuffer)
   {
      this->m_pContent = pInbound;
   }
   else if (nBytes)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));

      if (this->m_pContent)
      {
         CopyUtf8(this->m_pContent, pInbound);
      }
      else
      {
         this->m_pContent = nullptr;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      this->m_pContent = Convert8BitAsciiToUtf8(
                     reinterpret_cast<char *> (pInbound), &lenContent);
   }

   if (this->m_pContent)
   {
      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }

      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return;
}

FastUtf8::Uniseries::Uniseries(char *pInbound, bool bWrapBuffer)
{
   int    lenContent;
   bool   bIs7BitCharString;
   size_t nBytes = ValidateWithIs7BitUtf8(
                      reinterpret_cast<uint8_t *> (pInbound), &lenContent, 
                      &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();
   this->m_metadata = 0;

   if (bWrapBuffer)
   {
      this->m_pContent = reinterpret_cast<uint8_t *> (pInbound);
   }
   else if (nBytes)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));

      if (this->m_pContent)
      {
         CopyUtf8(m_pContent, reinterpret_cast<uint8_t *> (pInbound));
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      this->m_pContent = Convert8BitAsciiToUtf8(pInbound, &lenContent);
   }

   if (this->m_pContent)
   {
      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }

      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return;
}

// This standard constructor creates an empty FastUtf8 object with a 
// content buffer capacity specified in bytes.  The metadata has no flags 
// set upon construction.
FastUtf8::Uniseries::Uniseries(size_t nBytes)
{
   if (nBytes)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));
   }
   else
   {
      this->m_pContent = nullptr;
   }

   this->m_metadata = 0;

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return;
}

// This constructor creates a new FastUtf8::Uniseries object from the content 
// buffer and metadata of an existing one, with no validation, performing 
// a deep copy of the content.
FastUtf8::Uniseries::Uniseries(const FastUtf8::Uniseries& that)
{
   int lenContent;
   size_t nBytes;

   if (that.getContent())
   {
      this->m_metadata = that.getMetadata();
      lenContent = this->m_metadata & IS_LENGTH_LIMITED ? 
                        static_cast<int> (this->m_metadata & LENGTH_MASK) : 
                        CodePointCountUtf8(that.getContent());
      nBytes = SizeOfLenUtf8(that.getContent(), lenContent);
      this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));
      LenCopyUtf8(this->m_pContent, that.getContent(), lenContent);
   }
   else
   {
      this->m_pContent = nullptr;
   }

   return;
}

// This constructor creates a FastUtf8::Uniseries object from a buffer 
// designated by the pFirst and pLast pointers.
FastUtf8::Uniseries::Uniseries(const uint8_t *pFirst, const uint8_t *pLast)
{
   bool   bIs7BitCharString;
   int    lenContent = SizeValidateWithIs7BitUtf8(
                      pFirst, pLast, &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();
   this->m_metadata = 0;

   if (!pFirst || !*pFirst || pLast <= pFirst)
   {
      this->m_pContent = nullptr;
   }
   else if (lenContent)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(
                      1 + (pLast - pFirst)));

      if (this->m_pContent)
      {
         LenCopyUtf8(this->m_pContent, pFirst, lenContent);
      }
      else
      {
         this->m_pContent = nullptr;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      lenContent = static_cast<int> (pLast - pFirst);
      this->m_pContent = LenConvert8BitAsciiToUtf8(
                     reinterpret_cast<const char *> (pFirst), lenContent);
   }

   if (this->m_pContent)
   {
      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }

      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return;
}

FastUtf8::Uniseries::Uniseries(uint8_t *pFirst, uint8_t *pLast)
{
   bool   bIs7BitCharString;
   int    lenContent = SizeValidateWithIs7BitUtf8(
                      pFirst, pLast, &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();
   this->m_metadata = 0;

   if (!pFirst || !*pFirst || pLast <= pFirst)
   {
      this->m_pContent = nullptr;
   }
   else if (lenContent)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(
                      1 + (pLast - pFirst)));

      if (this->m_pContent)
      {
         LenCopyUtf8(this->m_pContent, pFirst, lenContent);
      }
      else
      {
         this->m_pContent = nullptr;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      lenContent = static_cast<int> (pLast - pFirst);
      this->m_pContent = LenConvert8BitAsciiToUtf8(
                     reinterpret_cast<const char *> (pFirst), lenContent);
   }

   if (this->m_pContent)
   {
      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }

      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return;
}

// The destructor deallocates the content buffer.
FastUtf8::Uniseries::~Uniseries(void)
{
   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }
}

// This assignment operator replaces the content and metadata associated 
// with an existing FastUtf8::Uniseries object by validating the content in 
// the inbound buffer and making a deep copy.  If the content includes any 
// invalid code point(s), the operator treats the entire buffer as 8-bit 
// ASCII and converts it, as such, into valid UTF-8 content.
FastUtf8::Uniseries& FastUtf8::Uniseries::operator=(const uint8_t *pInbound)
{
   int lenContent;
   bool bIs7BitCharString;
   size_t nBytes = ValidateWithIs7BitUtf8(
                      pInbound, &lenContent, &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();

   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }

   this->m_metadata = 0;

   if (nBytes)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));
      CopyUtf8(m_pContent, pInbound);

      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      this->m_pContent = Convert8BitAsciiToUtf8(
                      reinterpret_cast<const char *> (pInbound), &lenContent); 
   }

   if (this->m_pContent)
   {
      this->m_metadata &= ~static_cast<uint64_t> (LENGTH_MASK);
      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (this->m_metadata)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return *this;
}

FastUtf8::Uniseries& FastUtf8::Uniseries::operator=(const char *pInbound)
{
   int lenContent;
   bool bIs7BitCharString;
   size_t nBytes = ValidateWithIs7BitUtf8(
                      reinterpret_cast<const uint8_t *> (pInbound), &lenContent, 
                      &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();

   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }

   this->m_metadata = 0;

   if (nBytes)
   {
      this->m_pContent = reinterpret_cast<uint8_t *> (std::malloc(1 + nBytes));
      CopyUtf8(m_pContent, reinterpret_cast<const uint8_t *> (pInbound));

      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      this->m_pContent = Convert8BitAsciiToUtf8(pInbound, &lenContent); 
   }

   if (this->m_pContent)
   {
      this->m_metadata &= ~static_cast<uint64_t> (LENGTH_MASK);
      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (this->m_metadata)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return *this;
}

// This assignment operator replaces the content and metadata associated 
// with the current Uniseries object with the content buffer and metadata 
// of another existing one, with no validation, performing a deep copy like 
// the constructor.
FastUtf8::Uniseries& FastUtf8::Uniseries::operator=(
                      const FastUtf8::Uniseries& that)
{
   int lenContent;
   size_t nBytes;

   lenContent = CodePointCountUtf8(that.getContent());
   nBytes = SizeOfLenUtf8(that.getContent(), lenContent);

   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }

   this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));
   LenCopyUtf8(this->m_pContent, that.getContent(), lenContent);
   this->m_metadata = that.getMetadata();
   return *this;
}

FastUtf8::Uniseries& FastUtf8::Uniseries::operator=(
                      const FastUtf8::Uniseries *pThat)
{
   int lenContent;
   size_t nBytes;

   lenContent = m_metadata & IS_LENGTH_LIMITED ? 
                     static_cast<int> (this->m_metadata & LENGTH_MASK) : 
                     CodePointCountUtf8(pThat->getContent());
   nBytes = SizeOfLenUtf8(pThat->getContent(), lenContent);

   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }

   this->m_pContent = static_cast<uint8_t *> (std::malloc(1 + nBytes));
   LenCopyUtf8(this->m_pContent, pThat->getContent(), lenContent);
   this->m_metadata = pThat->getMetadata();
   return *this;
}

// This method constructs a new object from an existing one, making a deep 
// copy of a portion of its content specified by the iFirst and iLast 
// parameters.
//
// If iLast is less than iFirst, the returned object will comprise an empty 
// series.  If both values are negative, indexing is done based on the end of 
// the content; i.e., by counting backward from the end of the content to get 
// the content beginning iFirst code points from the end, and ending at the 
// code point corresponding to the iLast index relative to the end.  A 
// negative iFirst value and zero iLast value fetches the last portion of the 
// content, beginning -iFirst code points from the end.
FastUtf8::Uniseries FastUtf8::Uniseries::slice(int iFirst, int iLast) const
{
   int lenContent;

   if (!this->m_pContent)
   {
      return Uniseries(const_cast<char *> (""));
   }

   if (this->m_metadata & IS_7BIT_CHAR_STRING)
   {
      lenContent = this->m_metadata & IS_LENGTH_LIMITED ? 
                      static_cast<int> (this->m_metadata & LENGTH_MASK) : 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<char *> (this->m_pContent)));

      if (!iLast)
      {
         iLast = lenContent;
      }

      FastUtf8::Uniseries theSlice(LenSliceAscii(
                      reinterpret_cast<char *> (this->m_pContent), iFirst, 
                      iLast, lenContent), /* bWrapBuffer = */ true);
      theSlice.m_metadata = this->m_metadata & FLAGS_MASK;
      theSlice.m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (iLast - iFirst);
      return theSlice;
   }
   else
   {
      lenContent = this->m_metadata & IS_LENGTH_LIMITED ? 
                      static_cast<int> (this->m_metadata & LENGTH_MASK) : 
                      CodePointCountUtf8(this->m_pContent);

      if (!iLast)
      {
         iLast = lenContent;
      }

      FastUtf8::Uniseries theSlice(LenSliceUtf8(
                      this->m_pContent, iFirst, iLast, lenContent), 
                      /* bWrapBuffer = */ true);
      theSlice.m_metadata = this->m_metadata & FLAGS_MASK;
      theSlice.m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (iLast - iFirst);
      return theSlice;
   }
}

// This method constructs a new object from an existing one, making a deep 
// copy of a portion of its content specified by the pFirst and pLast 
// parameters.  DOES NO POINTER VALIDATION.
FastUtf8::Uniseries FastUtf8::Uniseries::slice(
                      const uint8_t *pFirst, const uint8_t *pLast) const
{
   if (!this->m_pContent || !pFirst || !pLast || pLast <= pFirst)
   {
      return Uniseries(const_cast<char *> (""));
   }

   FastUtf8::Uniseries theSlice(pFirst, pLast);
   theSlice.m_metadata = this->m_metadata & LENGTH_MASK;
   theSlice.m_metadata = this->m_metadata | FLAGS_MASK;
   return theSlice;
}

// This method constructs a new object from an existing one, making a deep 
// copy of a portion of its content specified by the pFirst and pLast 
// parameters.  DOES NO POINTER VALIDATION OTHER THAN NULL CHECKING.
FastUtf8::Uniseries& FastUtf8::Uniseries::fromSlice(
                      const uint8_t *pFirst, const uint8_t *pLast)
{
   int    lenContent;
   bool   bIs7BitCharString;

   if (!pFirst || !pLast || pLast <= pFirst)
   {
      this->m_pContent = nullptr;
      this->m_metadata = 0;
      return *this;
   }

   lenContent = SizeValidateWithIs7BitUtf8(
                      pFirst, pLast, &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();

   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }

   this->m_metadata = 0;

   if (lenContent)
   {
      this->m_pContent = static_cast<uint8_t *> (std::malloc(
                      1 + (pLast - pFirst)));

      if (this->m_pContent)
      {
         LenCopyUtf8(this->m_pContent, pFirst, lenContent);
      }
      else
      {
         this->m_pContent = nullptr;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      lenContent = static_cast<int> (pLast - pFirst);
      this->m_pContent = LenConvert8BitAsciiToUtf8(
                     reinterpret_cast<const char *> (pFirst), lenContent);
   }

   if (this->m_pContent)
   {
      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }

      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return *this;
}

FastUtf8::Uniseries& FastUtf8::Uniseries::fromSlice(
                      const char *pFirst, const char *pLast)
{
   int    lenContent;
   bool   bIs7BitCharString;

   if (!pFirst || !pLast || pLast <= pFirst)
   {
      this->m_pContent = nullptr;
      this->m_metadata = 0;
      return *this;
   }

   lenContent = SizeValidateWithIs7BitUtf8(
                      reinterpret_cast<const uint8_t *> (pFirst), 
                      reinterpret_cast<const uint8_t *> (pLast), 
                      &bIs7BitCharString);

   FastUtf8::Initializer::setupCaseMappingOnce();

   if (this->m_pContent && !(this->m_metadata & IS_DEALLOCATED_EXTERNALLY))
   {
      std::free(this->m_pContent);
      this->m_pContent = nullptr;
   }

   this->m_metadata = 0;

   if (lenContent)
   {
      this->m_pContent = reinterpret_cast<uint8_t *> (std::malloc(
                      1 + (pLast - pFirst)));

      if (this->m_pContent)
      {
         LenCopyUtf8(this->m_pContent, reinterpret_cast<const uint8_t *> (pFirst), 
                       lenContent);
      }
      else
      {
         this->m_pContent = nullptr;
      }
   }
   else
   {
      // Found invalid content: treat it as 8-bit ASCII and convert it.
      lenContent = static_cast<int> (pLast - pFirst);
      this->m_pContent = LenConvert8BitAsciiToUtf8(pFirst, lenContent);
   }

   if (this->m_pContent)
   {
      if (bIs7BitCharString)
      {
         this->m_metadata |= IS_7BIT_CHAR_STRING;
      }

      this->m_metadata |= static_cast<uint64_t> (lenContent & LENGTH_MASK);
   }

   if (FastUtf8::bCaseInsensitive)
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }

   if (FastUtf8::bLengthLimited)
   {
      this->m_metadata |= IS_LENGTH_LIMITED;
   }

   return *this;
}

// The concatenation operators reallocate the content buffer and perform a 
// deep copy of the additional content.  The metadata is left as close as 
// possible to the original metadata without falsifying it.  Any length 
// limit is adjusted to accommodate the added content.
FastUtf8::Uniseries& FastUtf8::Uniseries::operator+=(uint8_t *pInbound)
{
   uint8_t *pContent;
   int     lenContent;
   int     lenAddedContent;
   size_t  nBytes;
   bool    bIsInboundAscii;
   size_t  nAddedBytes = ValidateWithIs7BitUtf8(
                      pInbound, &lenAddedContent, &bIsInboundAscii);
   if (!nAddedBytes)
   {
      return *this;
   }

   if (this->m_metadata & IS_7BIT_CHAR_STRING)
   {
      if (bIsInboundAscii)
      {
         lenContent = static_cast<int> (std::strlen(
                      reinterpret_cast<char *> (this->m_pContent)));

         if (this->m_metadata & IS_LENGTH_LIMITED && 
             lenContent > static_cast<int> (this->m_metadata & LENGTH_MASK))
         {
            lenContent = static_cast<int> (this->m_metadata & LENGTH_MASK);
         }

         pContent = static_cast<uint8_t *> (std::realloc(
                      reinterpret_cast<char *> (this->m_pContent), 
                      1 + lenContent + lenAddedContent));

         if (pContent)
         {
            this->m_pContent = reinterpret_cast<uint8_t *> (strncat(
                      reinterpret_cast<char *> (pContent), 
                      reinterpret_cast<char *> (pInbound), 
                      static_cast<size_t> (lenAddedContent)));
         }
      }
      else
      {
         this->m_metadata &= ~static_cast<uint64_t> (IS_7BIT_CHAR_STRING);
      }
   }

   if (!(this->m_metadata & IS_7BIT_CHAR_STRING))
   {
      // If lenContent hasn't been set within the above conditions, it does 
      // get set here, regardless of what compiler warnings may say.
      lenContent = CodePointCountUtf8(this->m_pContent);

      if (this->m_metadata & IS_LENGTH_LIMITED && 
          lenContent > static_cast<int> (this->m_metadata & LENGTH_MASK))
      {
         lenContent = static_cast<int> (this->m_metadata & LENGTH_MASK);
      }

      nBytes = SizeOfLenUtf8(this->m_pContent, lenContent);
      pContent = static_cast<uint8_t *> (std::realloc(
                      reinterpret_cast<char *> (this->m_pContent), 
                      1 + nBytes + nAddedBytes));

      if (pContent)
      {
         this->m_pContent = LenConcatenateUtf8(
                      pContent, 1 + nBytes + nAddedBytes, pInbound,
                      lenContent, lenAddedContent);
      }
   }

   this->m_metadata &= ~static_cast<uint64_t> (LENGTH_MASK);
   this->m_metadata |= 
       LENGTH_MASK & (static_cast<uint64_t> (lenContent + lenAddedContent));
   return *this;
}

FastUtf8::Uniseries& FastUtf8::Uniseries::operator+=(char *pInbound)
{
   uint8_t *pContent;
   int     lenContent;
   int     lenAddedContent;
   size_t  nBytes;
   bool    bIsInboundAscii;
   size_t  nAddedBytes = ValidateWithIs7BitUtf8(
                      reinterpret_cast<uint8_t *> (pInbound), 
                      &lenAddedContent, &bIsInboundAscii);
   if (!nAddedBytes)
   {
      return *this;
   }

   if (this->m_metadata & IS_7BIT_CHAR_STRING)
   {
      if (bIsInboundAscii)
      {
         lenContent = static_cast<int> (std::strlen(
                      reinterpret_cast<char *> (this->m_pContent)));

         if (this->m_metadata & IS_LENGTH_LIMITED && 
             lenContent > static_cast<int> (this->m_metadata & LENGTH_MASK))
         {
            lenContent = static_cast<int> (this->m_metadata & LENGTH_MASK);
         }

         pContent = reinterpret_cast<uint8_t *> (std::realloc(
                      reinterpret_cast<char *> (this->m_pContent), 
                      1 + lenContent + lenAddedContent));

         if (pContent)
         {
            this->m_pContent = reinterpret_cast<uint8_t *> (strncat(
                      reinterpret_cast<char *> (pContent), 
                      pInbound, 
                      static_cast<size_t> (lenAddedContent)));
         }
      }
      else
      {
         this->m_metadata &= ~static_cast<uint64_t> (IS_7BIT_CHAR_STRING);
      }
   }

   if (!(this->m_metadata & IS_7BIT_CHAR_STRING))
   {
      // If lenContent hasn't been set within the above conditions, it does 
      // get set here, regardless of what compiler warnings may say.
      lenContent = CodePointCountUtf8(this->m_pContent);

      if (this->m_metadata & IS_LENGTH_LIMITED && 
          lenContent > static_cast<int> (this->m_metadata & LENGTH_MASK))
      {
         lenContent = static_cast<int> (this->m_metadata & LENGTH_MASK);
      }

      nBytes = SizeOfLenUtf8(this->m_pContent, lenContent);
      pContent = static_cast<uint8_t *> (std::realloc(
                      reinterpret_cast<char *> (this->m_pContent), 
                      1 + nBytes + nAddedBytes));

      if (pContent)
      {
         this->m_pContent = LenConcatenateUtf8(
                      pContent, 1 + nBytes + nAddedBytes, 
                      reinterpret_cast<uint8_t *> (pInbound),
                      lenContent, lenAddedContent);
      }
  }

   this->m_metadata &= ~static_cast<uint64_t> (LENGTH_MASK);
   this->m_metadata |= 
       LENGTH_MASK & (static_cast<uint64_t> (lenContent + lenAddedContent));
   return *this;
}

FastUtf8::Uniseries& FastUtf8::Uniseries::operator+=(
                      const FastUtf8::Uniseries& that)
{
   uint8_t *pContent;
   int     lenContent;
   int     lenAddedContent;
   size_t  nBytes;
   size_t  nAddedBytes;

   if (this->m_metadata & IS_7BIT_CHAR_STRING)
   {
      if (that.getMetadata() & IS_7BIT_CHAR_STRING)
      {
         lenContent = static_cast<int> (std::strlen(
                      reinterpret_cast<char *> (this->m_pContent)));
         lenAddedContent = static_cast<int> (std::strlen(
                      reinterpret_cast<char *> (that.getContent())));

         if (this->m_metadata & IS_LENGTH_LIMITED && 
             lenContent > static_cast<int> (this->m_metadata & LENGTH_MASK))
         {
            lenContent = static_cast<int> (this->m_metadata & LENGTH_MASK);
         }

         if (that.getMetadata() & IS_LENGTH_LIMITED && 
             lenAddedContent > static_cast<int> 
                      (that.getMetadata() & LENGTH_MASK))
         {
            lenAddedContent = static_cast<int> 
                      (that.getMetadata() & LENGTH_MASK);
         }

         pContent = static_cast<uint8_t *> (std::realloc(
                      reinterpret_cast<char *> (this->m_pContent), 
                      1 + lenContent + lenAddedContent));

         if (pContent)
         {
            this->m_pContent = reinterpret_cast<uint8_t *> (strncat(
                      reinterpret_cast<char *> (pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<size_t> (lenAddedContent)));
         }
      }
      else
      {
         this->m_metadata &= ~static_cast<uint64_t> (IS_7BIT_CHAR_STRING);
      }
   }

   if (!(this->m_metadata & IS_7BIT_CHAR_STRING))
   {
      // If lenContent and lenAddedContent haven't been set within the above 
      // conditions, they do get set here, regardless of what compiler 
      // warnings may say.
      lenContent = CodePointCountUtf8(this->m_pContent);
      lenAddedContent = CodePointCountUtf8(that.getContent());

      if (this->m_metadata & IS_LENGTH_LIMITED && 
          lenContent > static_cast<int> (this->m_metadata & LENGTH_MASK))
      {
         lenContent = static_cast<int> (this->m_metadata & LENGTH_MASK);
      }

      if (that.getMetadata() & IS_LENGTH_LIMITED && 
          lenAddedContent > static_cast<int> 
                   (that.getMetadata() & LENGTH_MASK))
      {
         lenAddedContent = static_cast<int> 
                   (that.getMetadata() & LENGTH_MASK);
      }

      nBytes = SizeOfLenUtf8(this->m_pContent, lenContent);
      nAddedBytes = SizeOfLenUtf8(that.getContent(), lenAddedContent);
      pContent = static_cast<uint8_t *> (std::realloc(
                   this->m_pContent, 1 + nBytes + nAddedBytes));

      if (pContent)
      {
         this->m_pContent = LenConcatenateUtf8(
                   pContent, 1 + nBytes + nAddedBytes, 
                   that.getContent(), lenContent, lenAddedContent);
      }
   }

   this->m_metadata &= ~static_cast<uint64_t> (LENGTH_MASK);
   this->m_metadata |= 
       LENGTH_MASK & (static_cast<uint64_t> (lenContent + lenAddedContent));
   return *this;
}

FastUtf8::Uniseries FastUtf8::Uniseries::operator+(const FastUtf8::Uniseries& that)
{
   uint8_t *pContent;
   int     lenContent;
   int     lenAddedContent;
   size_t  nBytes;
   size_t  nAddedBytes;
   FastUtf8::Uniseries theSeries(this->m_pContent);

   if (theSeries.m_metadata & IS_7BIT_CHAR_STRING)
   {
      if (that.getMetadata() & IS_7BIT_CHAR_STRING)
      {
         lenContent = static_cast<int> (std::strlen(
                      reinterpret_cast<char *> (theSeries.m_pContent)));
         lenAddedContent = static_cast<int> (std::strlen(
                      reinterpret_cast<char *> (that.getContent())));

         if (theSeries.m_metadata & IS_LENGTH_LIMITED && 
             lenContent > static_cast<int> (theSeries.m_metadata & 
                LENGTH_MASK))
         {
            lenContent = static_cast<int> (theSeries.m_metadata & LENGTH_MASK);
         }

         if (that.getMetadata() & IS_LENGTH_LIMITED && 
             lenAddedContent > static_cast<int> 
                      (that.getMetadata() & LENGTH_MASK))
         {
            lenAddedContent = static_cast<int> 
                      (that.getMetadata() & LENGTH_MASK);
         }

         pContent = static_cast<uint8_t *> (std::realloc(
                      reinterpret_cast<char *> (theSeries.m_pContent), 
                      1 + lenContent + lenAddedContent));

         if (pContent)
         {
            theSeries.m_pContent = reinterpret_cast<uint8_t *> (strncat(
                      reinterpret_cast<char *> (pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<size_t> (lenAddedContent)));
         }
      }
      else
      {
         theSeries.m_metadata &= ~static_cast<uint64_t> (IS_7BIT_CHAR_STRING);
      }
   }

   if (!(theSeries.m_metadata & IS_7BIT_CHAR_STRING))
   {
      // If lenContent and lenAddedContent haven't been set within the above 
      // conditions, they do get set here, regardless of what compiler 
      // warnings may say.
      lenContent = CodePointCountUtf8(theSeries.m_pContent);
      lenAddedContent = CodePointCountUtf8(that.getContent());

      if (theSeries.m_metadata & IS_LENGTH_LIMITED && 
          lenContent > static_cast<int> (theSeries.m_metadata & LENGTH_MASK))
      {
         lenContent = static_cast<int> (theSeries.m_metadata & LENGTH_MASK);
      }

      if (that.getMetadata() & IS_LENGTH_LIMITED && 
          lenAddedContent > static_cast<int> 
                   (that.getMetadata() & LENGTH_MASK))
      {
         lenAddedContent = static_cast<int> 
                   (that.getMetadata() & LENGTH_MASK);
      }

      nBytes = SizeOfLenUtf8(theSeries.m_pContent, lenContent);
      nAddedBytes = SizeOfLenUtf8(that.getContent(), lenAddedContent);
      pContent = static_cast<uint8_t *> (std::realloc(
                   theSeries.m_pContent, 1 + nBytes + nAddedBytes));

      if (pContent)
      {
         theSeries.m_pContent = LenConcatenateUtf8(
                   pContent, 1 + nBytes + nAddedBytes, 
                   that.getContent(), lenContent, lenAddedContent);
      }
   }

   theSeries.m_metadata &= ~static_cast<uint64_t> (LENGTH_MASK);
   theSeries.m_metadata |= 
       LENGTH_MASK & (static_cast<uint64_t> (lenContent + lenAddedContent));
   return theSeries;
}

// The std::unique_ptr<FastUtf8::Uniseries> pSeparate() method constructs an 
// object from a portion of the existing object's content.  The portion is 
// derived based on a search for a token.  The new object encompasses the 
// content "ahead of" a found token.  The existing object is modified to 
// encompass any remaining content "after" the token.  
//
// If the search can find no token, the method effectively moves the content 
// to the new object.  The method optionally trims white space from the 
// new object's content.  MODIFIES THE OBJECT'S CONTENT BY REPLACING FOUND 
// TOKENS WITH NULLS.
std::unique_ptr<FastUtf8::Uniseries> FastUtf8::Uniseries::pSeparate(
                         uint8_t *puzTokenSet, bool bTrim)
{
   uint8_t                              *puzSliceContent = nullptr;
   std::unique_ptr<FastUtf8::Uniseries> pSlice = nullptr;
   int                                  lenSlice = 0;

   if (this->m_pContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(
                         reinterpret_cast<char **> (&this->m_pContent), 
                         reinterpret_cast<char *> (puzTokenSet))));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = TrimUtf8(SeparateUtf8(
                      &this->m_pContent, puzTokenSet));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (&this->m_pContent), 
                      reinterpret_cast<char *> (puzTokenSet)));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = SeparateUtf8(&this->m_pContent, puzTokenSet);
         }
      }
   }

   if (puzSliceContent)
   {
	  pSlice = std::make_unique<FastUtf8::Uniseries>(puzSliceContent, 
                      /* bWrapBuffer = */ true);
      pSlice->m_metadata = this->m_metadata = this->m_metadata & FLAGS_MASK;

      if (lenSlice)
      {
         pSlice->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (lenSlice);
      }
      else if (pSlice->m_pContent)
      {
         pSlice->m_metadata |= 
                LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                      pSlice->m_pContent));
      }

      if (this->m_pContent)
      {
         this->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                         this->m_pContent));
      }

      this->m_metadata |= IS_DEALLOCATED_EXTERNALLY;
   }

   return pSlice;
}

std::unique_ptr<FastUtf8::Uniseries> FastUtf8::Uniseries::pSeparate(
                         char *pszTokenSet, bool bTrim)
{
   uint8_t                              *puzSliceContent = nullptr;
   std::unique_ptr<FastUtf8::Uniseries> pSlice = nullptr;
   int                                  lenSlice = 0;

   if (this->m_pContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(
                         reinterpret_cast<char **> (&this->m_pContent), 
                         pszTokenSet)));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = TrimUtf8(SeparateUtf8(
                      &this->m_pContent, 
                      reinterpret_cast<uint8_t *> (pszTokenSet)));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (&this->m_pContent), 
                      pszTokenSet));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = SeparateUtf8(&this->m_pContent, 
                      reinterpret_cast<uint8_t *> (pszTokenSet));
         }
      }
   }

   if (puzSliceContent)
   {
	  pSlice = std::make_unique<FastUtf8::Uniseries>(puzSliceContent, 
                      /* bWrapBuffer = */ true);
      pSlice->m_metadata = this->m_metadata = this->m_metadata & FLAGS_MASK;

      if (lenSlice)
      {
         pSlice->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (lenSlice);
      }
      else if (pSlice->m_pContent)
      {
         pSlice->m_metadata |= 
                LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                      pSlice->m_pContent));
      }

      if (this->m_pContent)
      {
         this->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                         this->m_pContent));
      }

      this->m_metadata |= IS_DEALLOCATED_EXTERNALLY;
   }

   return pSlice;
}

std::unique_ptr<FastUtf8::Uniseries> FastUtf8::Uniseries::pSeparate(
                      char cToken, bool bTrim)
{
   uint8_t                              *puzSliceContent = nullptr;
   std::unique_ptr<FastUtf8::Uniseries> pSlice = nullptr;
   int                                  lenSlice = 0;
   char                                 pszToken[2] = { cToken, '\0' };

   if (this->m_pContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(
                         reinterpret_cast<char **> (&this->m_pContent), 
                         pszToken)));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = TrimUtf8(SeparateUtf8(
                      &this->m_pContent, 
                      reinterpret_cast<uint8_t *> (pszToken)));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (&this->m_pContent), 
                      pszToken));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = SeparateUtf8(&this->m_pContent, 
                      reinterpret_cast<uint8_t *> (pszToken));
         }
      }
   }

   if (puzSliceContent)
   {
	  pSlice = std::make_unique<FastUtf8::Uniseries>(puzSliceContent, 
                      /* bWrapBuffer = */ true);
      pSlice->m_metadata = this->m_metadata = this->m_metadata & FLAGS_MASK;

      if (lenSlice)
      {
         pSlice->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (lenSlice);
      }
      else if (pSlice->m_pContent)
      {
         pSlice->m_metadata |= 
                LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                      pSlice->m_pContent));
      }

      if (this->m_pContent)
      {
         this->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                         this->m_pContent));
      }

      this->m_metadata |= IS_DEALLOCATED_EXTERNALLY;
   }

   return pSlice;
}

std::unique_ptr<FastUtf8::Uniseries> FastUtf8::Uniseries::pSeparate(
                         FastUtf8::Uniseries& usTokenSet, bool bTrim)
{
   uint8_t                              *puzSliceContent = nullptr;
   std::unique_ptr<FastUtf8::Uniseries> pSlice = nullptr;
   int                                  lenSlice = 0;

   if (this->m_pContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(
                         reinterpret_cast<char **> (&this->m_pContent), 
                         reinterpret_cast<char *> (usTokenSet.getContent()))));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = TrimUtf8(SeparateUtf8(
                      &this->m_pContent, usTokenSet.getContent()));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            puzSliceContent = reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (&this->m_pContent), 
                      reinterpret_cast<char *> (usTokenSet.getContent())));
            lenSlice = static_cast<int> (this->m_pContent - puzSliceContent);
         }
         else
         {
            puzSliceContent = SeparateUtf8(
                      &this->m_pContent, usTokenSet.getContent());
         }
      }
   }

   if (puzSliceContent)
   {
	  pSlice = std::make_unique<FastUtf8::Uniseries>(puzSliceContent, 
                      /* bWrapBuffer = */ true);
      pSlice->m_metadata = this->m_metadata = this->m_metadata & FLAGS_MASK;

      if (lenSlice)
      {
         pSlice->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (lenSlice);
      }
      else if (pSlice->m_pContent)
      {
         pSlice->m_metadata |= 
                LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                      pSlice->m_pContent));
      }

      if (this->m_pContent)
      {
         this->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                         this->m_pContent));
      }

      this->m_metadata |= IS_DEALLOCATED_EXTERNALLY;
   }

   return pSlice;
} 

// The pointer-driven separate() overloads can begin a token search from an  
// address within the "this" content.  The address is specified via the first 
// parameter.  The caller is responsible for ensuring that address is within 
// the content.
//
// The address returned is the address of the first portion of delimited 
// content.  The "this" content subsequently refers to any portion of the 
// orginal content that remains, beyond the token.  MODIFIES THE OBJECT'S 
// CONTENT BY REPLACING FOUND TOKENS WITH NULLS.
uint8_t * FastUtf8::Uniseries::pSeparate(
                      uint8_t **ppContent, const uint8_t *puzTokenSet, 
                      bool bTrim)
{
   if (ppContent && *ppContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(
                         reinterpret_cast<char **> (ppContent), 
                         reinterpret_cast<const char *> (puzTokenSet))));
         }
         else
         {
            return TrimUtf8(SeparateUtf8(ppContent, puzTokenSet));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (ppContent), 
                      reinterpret_cast<const char *> (puzTokenSet)));
         }
         else
         {
            return SeparateUtf8(ppContent, puzTokenSet);
         }
      }
   }

   return nullptr;
}

uint8_t * FastUtf8::Uniseries::pSeparate(
                      uint8_t **ppContent, const char *pszTokenSet, bool bTrim)
{
   if (ppContent && *ppContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(reinterpret_cast<char **> (ppContent), 
                         pszTokenSet)));
         }
         else
         {
            return TrimUtf8(SeparateUtf8(ppContent, 
                      reinterpret_cast<const uint8_t *> (pszTokenSet)));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (ppContent), 
                      pszTokenSet));
         }
         else
         {
            return SeparateUtf8(ppContent, 
                      reinterpret_cast<const uint8_t *> (pszTokenSet));
         }
      }
   }

   return nullptr;
}

uint8_t * FastUtf8::Uniseries::pSeparate(
                      uint8_t **ppContent, char cToken, bool bTrim)
{
   const char szToken[2] = { cToken, '\0' };

   if (ppContent && *ppContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (TrimAscii(
                      SeparateAscii(
                         reinterpret_cast<char **> (ppContent), 
                         szToken)));
         }
         else
         {
            return TrimUtf8(SeparateUtf8(ppContent, 
                      reinterpret_cast<const uint8_t *> (szToken)));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (SeparateAscii(
                      reinterpret_cast<char **> (ppContent), 
                      szToken));
         }
         else
         {
            return SeparateUtf8(ppContent, 
                      reinterpret_cast<const uint8_t *> (szToken));
         }
      }
   }

   return nullptr;
}

uint8_t * FastUtf8::Uniseries::pSeparate(
                      uint8_t **ppContent, 
                      const FastUtf8::Uniseries& usTokenSet, bool bTrim)
{
   if (ppContent && *ppContent)
   {
      if (bTrim)
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (TrimAscii(
               SeparateAscii(
                  reinterpret_cast<char **> (ppContent), 
                  reinterpret_cast<const char *> (usTokenSet.getContent()))));
         }
         else
         {
            return TrimUtf8(SeparateUtf8(ppContent, usTokenSet.getContent()));
         }
      }
      else
      {
         if (this->m_metadata & IS_7BIT_CHAR_STRING)
         {
            return reinterpret_cast<uint8_t *> (SeparateAscii(
               reinterpret_cast<char **> (ppContent), 
               reinterpret_cast<const char *> (usTokenSet.getContent())));
         }
         else
         {
            return SeparateUtf8(ppContent, usTokenSet.getContent());
         }
      }
   }

   return nullptr;
}

// The pFindToken() content comparison method searches the "this" content for 
// any token within a set of tokens.  The token search begins from an address, 
// within the content, specified via the first parameter.  The caller is 
// responsible for ensuring that the address is  within the content.  If a 
// token is found, the method returns a pointer to the code point immediately 
// prior to it.  Otherwise, the method returns nullptr.
uint8_t * FastUtf8::Uniseries::pFindToken(
      const uint8_t *puzContent,      // Content in which to search for tokens
      const uint8_t *puzTokenSet)     // Set of tokens
         const noexcept
{
   if (!puzContent)
   {
      switch (this->m_metadata & FLAGS_MASK)
      {
         case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
            return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (puzTokenSet), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (puzTokenSet)))));
         case IS_7BIT_CHAR_STRING:
            return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (puzTokenSet)));
         case IS_LENGTH_LIMITED:
            return TokenLenFindUtf8(this->m_pContent, puzTokenSet, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(puzTokenSet));
         default:
            return TokenFindUtf8(this->m_pContent, puzTokenSet);
      }
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (puzContent), 
                      reinterpret_cast<const char *> (puzTokenSet), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (puzContent))), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (puzTokenSet)))));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (puzContent), 
                      reinterpret_cast<const char *> (puzTokenSet)));
      case IS_LENGTH_LIMITED:
         return TokenLenFindUtf8(puzContent, puzTokenSet, 
                      CodePointCountUtf8(puzContent), 
                      CodePointCountUtf8(puzTokenSet));
      default:
         return TokenFindUtf8(puzContent, puzTokenSet);
   }
}

uint8_t * FastUtf8::Uniseries::pFindToken(
      const char *pszContent,      // Content in which to search for tokens
      const char *pszTokenSet)     // Set of tokens
         const noexcept
{
   if (!pszContent)
   {
      switch (this->m_metadata & FLAGS_MASK)
      {
         case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
            return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      pszTokenSet, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pszTokenSet))));
         case IS_7BIT_CHAR_STRING:
            return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      pszTokenSet));
         case IS_LENGTH_LIMITED:
            return TokenLenFindUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pszTokenSet), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pszTokenSet)));
         default:
            return TokenFindUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pszTokenSet));
      }
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(pszContent, 
                      pszTokenSet, static_cast<int> (std::strlen(pszContent)), 
                      static_cast<int> (std::strlen(pszTokenSet))));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(pszContent, 
                       pszTokenSet));
      case IS_LENGTH_LIMITED:
          return TokenLenFindUtf8(
                      reinterpret_cast<const uint8_t *> (pszContent), 
                      reinterpret_cast<const uint8_t *> (pszTokenSet), 
                      static_cast<int> (std::strlen(pszContent)), 
                      static_cast<int> (std::strlen(pszTokenSet)));
      default:
         return TokenFindUtf8(reinterpret_cast<const uint8_t *> (pszContent), 
                      reinterpret_cast<const uint8_t *> (pszTokenSet));
   }
}

uint8_t * FastUtf8::Uniseries::pFindToken(
      const uint8_t *puzContent,      // Content in which to search for tokens
      const Uniseries& sTokenSet)     // Set of tokens
         const noexcept
{
   if (!puzContent)
   {
      switch (this->m_metadata & FLAGS_MASK)
      {
         case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
            return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (sTokenSet.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sTokenSet.getLength()));
         case IS_7BIT_CHAR_STRING:
            return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> 
                         (sTokenSet.getContent())));
         case IS_LENGTH_LIMITED:
            return TokenLenFindUtf8(this->m_pContent, sTokenSet.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sTokenSet.getLength());
         default:
            return TokenFindUtf8(this->m_pContent, sTokenSet.getContent());
      }
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (puzContent), 
                      reinterpret_cast<const char *> (sTokenSet.getContent()), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (puzContent))), 
                      sTokenSet.getLength()));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (puzContent), 
                      reinterpret_cast<const char *> 
                         (sTokenSet.getContent())));
      case IS_LENGTH_LIMITED:
         return TokenLenFindUtf8(puzContent, sTokenSet.getContent(), 
                      CodePointCountUtf8(puzContent), sTokenSet.getLength());
      default:
         return TokenFindUtf8(puzContent, sTokenSet.getContent());
   }
}

uint8_t * FastUtf8::Uniseries::pFindToken(
      const char *pszContent,      // Content in which to search for tokens
      const Uniseries& sTokenSet)  // Set of tokens
         const noexcept
{
   if (!pszContent)
   {
      switch (this->m_metadata & FLAGS_MASK)
      {
         case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
            return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (sTokenSet.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sTokenSet.getLength()));
         case IS_7BIT_CHAR_STRING:
            return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> 
                         (sTokenSet.getContent())));
         case IS_LENGTH_LIMITED:
            return TokenLenFindUtf8(this->m_pContent, sTokenSet.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sTokenSet.getLength());
         default:
            return TokenFindUtf8(this->m_pContent, sTokenSet.getContent());
      }
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(pszContent, 
                      reinterpret_cast<const char *> (sTokenSet.getContent()), 
                      static_cast<int> (std::strlen(pszContent)), 
                      sTokenSet.getLength()));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(pszContent, 
                      reinterpret_cast<const char *> (sTokenSet.getContent())));
      case IS_LENGTH_LIMITED:
         return TokenLenFindUtf8(
                      reinterpret_cast<const uint8_t *> (pszContent), 
                      sTokenSet.getContent(), 
                      static_cast<int> (std::strlen(pszContent)), 
                      sTokenSet.getLength());
      default:
         return TokenFindUtf8(
                      reinterpret_cast<const uint8_t *> (pszContent), 
                      sTokenSet.getContent());
   }
}

// Single-parameter overloads of pFindToken() begin their search at the top of 
// the "this" content.
uint8_t * FastUtf8::Uniseries::pFindToken(
      const uint8_t *puzTokenSet)     // Set of tokens
         const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (puzTokenSet), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (puzTokenSet)))));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (puzTokenSet)));
      case IS_LENGTH_LIMITED:
         return TokenLenFindUtf8(this->m_pContent, puzTokenSet, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(puzTokenSet));
      default:
         return TokenFindUtf8(this->m_pContent, puzTokenSet);
   }
}

uint8_t * FastUtf8::Uniseries::pFindToken(
      const char *pszTokenSet)     // Set of tokens
         const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      pszTokenSet, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pszTokenSet))));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      pszTokenSet));
      case IS_LENGTH_LIMITED:
         return TokenLenFindUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pszTokenSet), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pszTokenSet)));
      default:
         return TokenFindUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pszTokenSet));
   }
}

uint8_t * FastUtf8::Uniseries::pFindToken(
      const Uniseries& sTokenSet)     // Set of tokens
         const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (TokenLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (sTokenSet.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sTokenSet.getLength()));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (TokenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> 
                         (sTokenSet.getContent())));
      case IS_LENGTH_LIMITED:
         return TokenLenFindUtf8(this->m_pContent, sTokenSet.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK),
                      sTokenSet.getLength());
      default:
         return TokenFindUtf8(this->m_pContent, sTokenSet.getContent());
   }
}

// The methods within the iterator class apply to individual code points 
// within a Uniseries object's content buffer.
FastUtf8::Uniseries::Iterator::Iterator(
               uint8_t *pSeries, uint8_t *pSeriesBase, uint8_t *pSeriesLimit,
               FastUtf8::Uniseries& series) 
    : pContentBase(pSeriesBase), 
      pContentLimit(pSeriesLimit), 
      pContentCurrent(pSeries),
      theSeries(series)
{
}

// The * dereference operator returns a uint32_t value representing a UTF-8 
// code point.  The code point's substantive byte(s) occupy the value's least 
// significant byte(s).  The value can be used for comparison with other code 
// points represented similarly.
uint32_t FastUtf8::Uniseries::Iterator::operator*() const
{
   uint32_t nCodePoint;

   // Have we got half-ASCII code points?  Map to lowercase and compare.
   if (*pContentCurrent <= HALF_ASCII_LIMIT)
   {
      nCodePoint = static_cast<uint32_t> (*pContentCurrent);
   }
   else if (*pContentCurrent > TWOFER_LIMIT)
   {
      if (*pContentCurrent > THREESOME_LIMIT)
      {
         nCodePoint = static_cast<uint32_t> (*pContentCurrent << 24) | 
                      static_cast<uint32_t> (*(1 + pContentCurrent) << 16) | 
                      static_cast<uint32_t> (*(2 + pContentCurrent) << 8) | 
                      static_cast<uint32_t> (*(3 + pContentCurrent));
      }
      else
      {
         nCodePoint = static_cast<uint32_t> (*pContentCurrent << 16) | 
                      static_cast<uint32_t> (*(1 + pContentCurrent) << 8) | 
                      static_cast<uint32_t> (*(2 + pContentCurrent));
      }
   }
   else if (*pContentCurrent > SINGLETON_LIMIT)
   {
      nCodePoint = static_cast<uint32_t> ((*pContentCurrent & 0x1F) << 8) | 
                   static_cast<uint32_t> (*(1 + pContentCurrent));
   }
   else
   {
      nCodePoint = 0;
   }

   return nCodePoint;
}

// The -> dereference operator is intended to point to a UTF-8 code point in 
// memory regardless of its alignment.
uint8_t * FastUtf8::Uniseries::Iterator::operator->() const
{
   return pContentCurrent;
}

// The prefix and postfix increment operators each advance the iterator 
// by a code point.
FastUtf8::Uniseries::Iterator& FastUtf8::Uniseries::Iterator::operator++()
{
   if (pContentCurrent < pContentLimit)
   {
      CodePointAdvanceUtf8((const uint8_t **) &pContentCurrent);
   }

   return *this;
}

FastUtf8::Uniseries::Iterator FastUtf8::Uniseries::Iterator::operator++(int)
{
   Iterator itr = *this;

   if (pContentCurrent < pContentLimit)
   {
      CodePointAdvanceUtf8((const uint8_t **) &pContentCurrent);
   }

   return itr;
}

// The prefix and postfix decrement operators each backtrack the iterator 
// by a code point.
FastUtf8::Uniseries::Iterator& FastUtf8::Uniseries::Iterator::operator--()
{
   if (pContentCurrent > pContentBase)
   {
      CodePointBacktrackUtf8((const uint8_t **) &pContentCurrent, 
                      (const uint8_t *) pContentBase);
   }

   return *this;
}

FastUtf8::Uniseries::Iterator FastUtf8::Uniseries::Iterator::operator--(int)
{
   Iterator itr = *this;

   if (pContentCurrent > pContentBase)
   {
      CodePointBacktrackUtf8((const uint8_t **) &pContentCurrent, 
                      (const uint8_t *) pContentBase);
   }

   return itr;
}

// These are equality operators for individual code points in content.
bool FastUtf8::Uniseries::Iterator::operator==(const Iterator& thatItr)
{
   if (theSeries.m_metadata & IS_CASE_INSENSITIVE)
   {
      return CodePointCaseCompareUtf8(
                      this->pContentCurrent, thatItr.pContentCurrent);
   }
   else
   {
      return CodePointCompareUtf8(
                      this->pContentCurrent, thatItr.pContentCurrent);
   }
}

bool FastUtf8::Uniseries::Iterator::operator!=(const Iterator& thatItr)
{
   if (theSeries.m_metadata & IS_CASE_INSENSITIVE)
   {
      return !CodePointCaseCompareUtf8(
                      this->pContentCurrent, thatItr.pContentCurrent);
   }
   else
   {
      return !CodePointCompareUtf8(
                      this->pContentCurrent, thatItr.pContentCurrent);
   }
}

// The remaining methods in the FastUtf8::Uniseries class apply to content as a 
// whole.
FastUtf8::Uniseries::Iterator FastUtf8::Uniseries::begin()
{
   return Iterator(m_pContent, m_pContent, 
                      m_pContent + SizeOfUtf8(m_pContent), *this);
}

// The 'end' iterator points one past the last element
FastUtf8::Uniseries::Iterator FastUtf8::Uniseries::end()
{
   size_t sizeContent;

   if (m_metadata & IS_LENGTH_LIMITED)
   {
      sizeContent = SizeOfLenUtf8(m_pContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      sizeContent = SizeOfUtf8(m_pContent);
   }

   return Iterator(m_pContent + sizeContent, m_pContent, 
                      m_pContent + sizeContent, *this);
}

// These are basic getter / setter methods for whole content and its 
// metadata.  This first one gets a pointer to the object's content buffer.
uint8_t * FastUtf8::Uniseries::getContent(void) const noexcept
{
   return this->m_pContent;
}

uint64_t FastUtf8::Uniseries::getMetadata(void) const noexcept
{
   return this->m_metadata;
}

// This method makes a deep copy of the whole content.  The developer is 
// responsible for ensuring that the buffer receiving the content is 
// sufficient.
void FastUtf8::Uniseries::getContent(uint8_t *pOutbound)
{
   if (this->m_metadata & IS_LENGTH_LIMITED)
   {
      LenCopyUtf8(pOutbound, this->m_pContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      CopyUtf8(pOutbound, this->m_pContent);
   }

   return;
}

void FastUtf8::Uniseries::getContent(char *pOutbound)
{
   if (!this->m_pContent)
   {
      pOutbound = nullptr;
   }
   else if (this->m_metadata & IS_LENGTH_LIMITED)
   {
      LenCopyUtf8(reinterpret_cast<uint8_t *> (pOutbound), 
                      this->m_pContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      CopyUtf8(reinterpret_cast<uint8_t *> (pOutbound), 
                      this->m_pContent);
   }

   return;
}

// The IS_CASE_INSENSITIVE flag can be set via either of the next methods.
void FastUtf8::Uniseries::setCaseSensitivity(
                             bool bCaseSensitive) noexcept
{
   if (bCaseSensitive)
   {
      this->m_metadata &= ~(static_cast<uint64_t> (IS_CASE_INSENSITIVE));
   }
   else
   {
      this->m_metadata |= IS_CASE_INSENSITIVE;
   }
}

void FastUtf8::Uniseries::setCaseInsensitive(void) noexcept
{
   this->m_metadata |= IS_CASE_INSENSITIVE;
}

void FastUtf8::Uniseries::clearCaseInsensitive(void) noexcept
{
   this->m_metadata &= ~(static_cast<uint64_t> (IS_CASE_INSENSITIVE));
}

// Setting a nonzero length limit also sets the IS_LENGTH_LIMITED flag.
void FastUtf8::Uniseries::setLengthLimit(int lenContent) noexcept
{
   if (lenContent)
   {
      this->m_metadata &= ~(static_cast<uint64_t> (LENGTH_MASK));
      this->m_metadata |= (IS_LENGTH_LIMITED | (lenContent & LENGTH_MASK));
   }
  else
   {
      this->m_metadata &= ~(static_cast<uint64_t> (IS_LENGTH_LIMITED));
   }
}

void FastUtf8::Uniseries::setLengthLimited(void) noexcept
{
   this->m_metadata |= IS_LENGTH_LIMITED;
}

void FastUtf8::Uniseries::clearLengthLimited(void) noexcept
{
   this->m_metadata &= ~(static_cast<uint64_t> (IS_LENGTH_LIMITED));
}

// This method returns the object's current content length as a count of 
// its code points.
int FastUtf8::Uniseries::getLength(const uint8_t *pInbound) noexcept
{
   return CodePointCountUtf8(pInbound);
}

// This method returns the length of the inbound content.
int FastUtf8::Uniseries::getLength(const char *pInbound) noexcept
{
   return CodePointCountUtf8(reinterpret_cast<const uint8_t *> (pInbound));
}

int FastUtf8::Uniseries::getLength(void) const noexcept
{
   return static_cast<int> ((this->m_metadata & LENGTH_MASK) ? 
                      static_cast<int> (this->m_metadata & LENGTH_MASK) : 
                      CodePointCountUtf8(this->m_pContent));
}

int FastUtf8::Uniseries::getLength(const FastUtf8::Uniseries& that) 
                      noexcept
{
   return static_cast<int> ((that.getMetadata() & LENGTH_MASK) ? 
                      static_cast<int> (that.getMetadata() & LENGTH_MASK) : 
                      CodePointCountUtf8(that.getContent()));
}

// Given a byte count, this method returns the corresponding count of code 
// points between the beginning of the content and the last complete code 
// point encompassing the given number of bytes.
int FastUtf8::Uniseries::getLength(const size_t sizeContent) noexcept
{
   return LenSizeOfUtf8(this->m_pContent, sizeContent);
}

// This method returns the content's size in bytes.
size_t FastUtf8::Uniseries::getSize(const uint8_t *pInbound) noexcept
{
   return SizeOfUtf8(pInbound); 
}

size_t FastUtf8::Uniseries::getSize(const char *pInbound) noexcept
{
   return SizeOfUtf8(reinterpret_cast<const uint8_t *> (pInbound)); 
}

size_t FastUtf8::Uniseries::getSize(void) noexcept
{
   if (m_metadata & IS_LENGTH_LIMITED)
   {
      return SizeOfLenUtf8(
                      this->m_pContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      return SizeOfUtf8(this->m_pContent); 
   }
}

size_t FastUtf8::Uniseries::getSize(const FastUtf8::Uniseries& that) noexcept
{
   if (that.getMetadata() & IS_LENGTH_LIMITED)
   {
      return SizeOfLenUtf8(
                      that.getContent(), 
                      static_cast<int> (that.getMetadata() & LENGTH_MASK));
   }
   else
   {
      return SizeOfUtf8(that.getContent()); 
   }
}

// This method returns the number of bytes needed to store the content 
// after case folding.  This size may be larger or smaller than the 
// unfolded size.
size_t FastUtf8::Uniseries::getSizeFolded(const uint8_t *pInbound) noexcept
{
   return SizeOfFoldedUtf8(pInbound); 
}

size_t FastUtf8::Uniseries::getSizeFolded(const char *pInbound) noexcept
{
   return SizeOfFoldedUtf8(reinterpret_cast<const uint8_t *> (pInbound)); 
}

size_t FastUtf8::Uniseries::getSizeFolded(void) const noexcept
{
   if (this->m_metadata & IS_LENGTH_LIMITED)
   {
      return SizeOfFoldedLenUtf8(
                      this->m_pContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      return SizeOfFoldedUtf8(this->m_pContent); 
   }
}

size_t FastUtf8::Uniseries::getSizeFolded(const FastUtf8::Uniseries& that) 
                      noexcept
{
   if (that.getMetadata() & IS_LENGTH_LIMITED)
   {
      return SizeOfFoldedLenUtf8(
                      that.getContent(), 
                      static_cast<int> (that.getMetadata() & LENGTH_MASK));
   }
   else
   {
      return SizeOfFoldedUtf8(that.getContent()); 
   }
}

// This method creates a Uniseries by making a deep copy of the whole inbound 
// content after case folding.  The size of folded content may be greater or 
// less than the original content's size, in bytes.  To predetermine the 
// needed size, invoke the above method.
FastUtf8::Uniseries FastUtf8::Uniseries::getFolded(
                      uint8_t *pOutbound, size_t sizeOutbound)
{
   FastUtf8::Uniseries series = Uniseries(sizeOutbound);

   series.m_pContent = ToFoldedUtf8(
                      series.m_pContent, pOutbound, sizeOutbound);
   series.m_metadata = static_cast<uint64_t> (CodePointCountUtf8(
                      series.m_pContent) & LENGTH_MASK);
   return series;
}

FastUtf8::Uniseries FastUtf8::Uniseries::getFolded(
                      char *pOutbound, size_t sizeOutbound) 
{
   FastUtf8::Uniseries series = Uniseries(sizeOutbound);

   series.m_pContent = ToFoldedUtf8(
                      series.m_pContent, 
                      reinterpret_cast<uint8_t *> (pOutbound), 
                      sizeOutbound);
   series.m_metadata = static_cast<uint64_t> (CodePointCountUtf8(
                      series.m_pContent) & LENGTH_MASK);
   return series;
}

FastUtf8::Uniseries FastUtf8::Uniseries::getFolded(void) const
{
   size_t              sizeSeries = this->getSizeFolded();
   FastUtf8::Uniseries series = Uniseries(sizeSeries);

   ToFoldedUtf8(series.m_pContent, this->m_pContent,sizeSeries);
   series.m_metadata = this->m_metadata;
   return series;
}

FastUtf8::Uniseries FastUtf8::Uniseries::getFolded(
                      const FastUtf8::Uniseries& that)
{
   size_t              sizeSeries = that.getSizeFolded();
   FastUtf8::Uniseries series = Uniseries(sizeSeries);

   ToFoldedUtf8(series.m_pContent, that.getContent(), sizeSeries);
   series.m_metadata = that.getMetadata();
   return series;
}

FastUtf8::Uniseries FastUtf8::Uniseries::getFolded(
                      const FastUtf8::Uniseries& that, size_t sizeOutbound)
{
   FastUtf8::Uniseries series = Uniseries(sizeOutbound);

   series.m_pContent = ToFoldedUtf8(
                      series.m_pContent, that.getContent(), sizeOutbound);
   series.m_metadata = that.getMetadata();
   return series;
}

// Is the inbound content all 7-bit ASCII characters?  If so, returns true.
// Otherwise returns false.
bool FastUtf8::Uniseries::is7Bit(const uint8_t *pInbound) noexcept
{
   return Is7BitUtf8(const_cast<uint8_t *> (pInbound)); 
}

bool FastUtf8::Uniseries::is7Bit(const char *pInbound) noexcept
{
   return Is7BitUtf8(reinterpret_cast<uint8_t *> (
                      const_cast<char *> (pInbound)));
}

// Is the current content all 7-bit ASCII characters?  If so, this method 
// sets the IS_7BIT_CHAR_STRING flag and returns true.  Otherwise, this 
// method clears that flag and returns false.
bool FastUtf8::Uniseries::is7Bit(void) noexcept
{
   if (this->m_pContent)
   {
     if (this->m_metadata & IS_LENGTH_LIMITED)
     {
        if (IsLen7BitUtf8(
                     this->m_pContent, 
                     static_cast<int> (this->m_metadata & LENGTH_MASK)))
        {
           this->m_metadata |= IS_7BIT_CHAR_STRING;
           return true;
        }
        else
        {
           this->m_metadata &= ~(static_cast<uint64_t> (IS_7BIT_CHAR_STRING));
        }
     }
     else
     {
        if (Is7BitUtf8(this->m_pContent))
        {
           this->m_metadata |= IS_7BIT_CHAR_STRING;
           return true;
        }
        else
        {
           this->m_metadata &= ~(static_cast<uint64_t> (IS_7BIT_CHAR_STRING));
        }
     }
   }

   return false;
}

bool FastUtf8::Uniseries::is7Bit(const FastUtf8::Uniseries& that) 
                             noexcept
{
   if (that.getContent())
   {
      if (that.getMetadata() & IS_LENGTH_LIMITED)
      {
         if (IsLen7BitUtf8(
                      that.getContent(), 
                      static_cast<int> (that.getMetadata() & LENGTH_MASK)))
         {
            return true;
         }
      }
      else
      {
         if (Is7BitUtf8(that.getContent()))
         {
            return true;
         }
      }
   }

   return false;
}

// These are equality operators for whole content comparison.
bool FastUtf8::Uniseries::operator==(
                      const FastUtf8::Uniseries& that) const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (strncasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      that.getLength()) 
                         == 0);
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return (strcasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent())) 
                         == 0);
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (strncmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      that.getLength()) 
                         == 0);
      case IS_7BIT_CHAR_STRING:
         return (strcmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent())) 
                         == 0);
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return LenCaseCompareUtf8(
                      this->m_pContent, that.getContent(), that.getLength());
      case IS_CASE_INSENSITIVE:
         return CaseCompareUtf8(
                      this->m_pContent, that.getContent());
      case IS_LENGTH_LIMITED:
         return LenCompareUtf8(
                      this->m_pContent, that.getContent(), that.getLength());
      default:
         return CompareUtf8(
                      this->m_pContent, that.getContent());
   }
}

bool FastUtf8::Uniseries::operator!=(
                      const FastUtf8::Uniseries& that) const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (strncasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      that.getLength()) 
                         != 0);
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return (strcasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent())) 
                         != 0);
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (strncmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      that.getLength()) 
                         != 0);
      case IS_7BIT_CHAR_STRING:
         return (strcmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent())) 
                         != 0);
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return !LenCaseCompareUtf8(
                      this->m_pContent, that.getContent(), that.getLength());
      case IS_CASE_INSENSITIVE:
         return !CaseCompareUtf8(
                      this->m_pContent, that.getContent());
      case IS_LENGTH_LIMITED:
         return !LenCompareUtf8(
                      this->m_pContent, that.getContent(), that.getLength());
      default:
         return !CompareUtf8(
                      this->m_pContent, that.getContent());
   }
}

// This method is identical to the equality operator but strictly case-
// insensitive; it does not check the IS_CASE_INSENSITIVE flag.
bool FastUtf8::Uniseries::caseCompare(const uint8_t *pInbound) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (strncasecmp(
                      reinterpret_cast<char *> (this->m_pContent),
                      reinterpret_cast<char *> (
                         const_cast<uint8_t *> (pInbound)), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK)) 
                             == 0);
      case IS_7BIT_CHAR_STRING:
         return (strcasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (
                         const_cast<uint8_t *> (pInbound))) 
                             == 0);
      case IS_LENGTH_LIMITED:
         return LenCaseCompareUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return CaseCompareUtf8(
                      this->m_pContent, pInbound);
   }
}

bool FastUtf8::Uniseries::caseCompare(const char *pInbound) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (strncasecmp(
                      reinterpret_cast<char *> (this->m_pContent), pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK)) 
                             == 0);
      case IS_7BIT_CHAR_STRING:
         return (strcasecmp(reinterpret_cast<char *> (this->m_pContent), 
                      pInbound) 
                             == 0);
      case IS_LENGTH_LIMITED:
         return LenCaseCompareUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return CaseCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound));
   }
}

bool FastUtf8::Uniseries::caseCompare(
                             const FastUtf8::Uniseries& that) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (strncasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      that.getLength()) 
                             == 0);
      case IS_7BIT_CHAR_STRING:
         return (strcasecmp(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent())) 
                             == 0);
      case IS_LENGTH_LIMITED:
         return LenCaseCompareUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return CaseCompareUtf8(
                      this->m_pContent, that.getContent());
   }
}

// This partial content comparison method returns true if the "this" 
// object's content includes the inbound content, and false otherwise.
bool FastUtf8::Uniseries::contains(const uint8_t *pInbound) const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                          reinterpret_cast<const char *> (pInbound))),
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound),
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pInbound))),
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != std::strstr(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound)));
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pInbound),
                      /* ppLast = */ nullptr));
      case IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindUtf8(
                      this->m_pContent, pInbound, /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenFindUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pInbound),
                      /* ppLast = */ nullptr));
      default:
         return (nullptr != FindUtf8(this->m_pContent, pInbound,
                      /* ppLast = */ nullptr));
   }
}

bool FastUtf8::Uniseries::contains(const char *pInbound) const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pInbound)), 
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound,
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pInbound)), 
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != std::strstr(
                      reinterpret_cast<char *> (this->m_pContent), pInbound));
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pInbound)),
                      /* ppLast = */ nullptr));
      case IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound),
                      /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pInbound)),
                      /* ppLast = */ nullptr));
      default:
         return (nullptr != FindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound),
                      /* ppLast = */ nullptr));
   }
}

bool FastUtf8::Uniseries::contains(const char cInbound) const noexcept
{
   char szInbound[2] = { cInbound, '\0' };

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), szInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1,  /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), szInbound,
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), szInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1,  /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != std::strstr(
                      reinterpret_cast<char *> (this->m_pContent), szInbound));
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1,  /* ppLast = */ nullptr));
      case IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound),
                      /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1,  /* ppLast = */ nullptr));
      default:
         return (nullptr != FindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound),
                      /* ppLast = */ nullptr));
   }
}

// This partial content comparison method returns true if the "this" 
// object's content includes the "that" object's content, and false 
// otherwise.
bool FastUtf8::Uniseries::contains(
                             const FastUtf8::Uniseries& that) const noexcept
{
   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()),
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != std::strstr(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent())));
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), /* ppLast = */ nullptr));
      case IS_CASE_INSENSITIVE:
         return (nullptr != CaseFindUtf8(
                      this->m_pContent, that.getContent(),
                      /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenFindUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), /* ppLast = */ nullptr));
      default:
         return (nullptr != FindUtf8(
                      this->m_pContent, that.getContent(),
                      /* ppLast = */ nullptr));
   }
}

// This partial content comparison method returns an index of the inbound 
// content within the "this" object's content -- that is, a count of the 
// code points between the beginning of "this" content and any first 
// match -- or a negative return value (-1) in case that content is not 
// found.
int FastUtf8::Uniseries::find(const uint8_t *pInbound) const noexcept
{
   char *pszStrstrResult;

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pInbound))));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return IndexCaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pInbound))));
      case IS_7BIT_CHAR_STRING:
      {
         pszStrstrResult = std::strstr(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound));
         
         if (pszStrstrResult == nullptr)
         {
            return -1;
         }
         else
         {
            return static_cast<int> (pszStrstrResult - 
                      reinterpret_cast<const char *> (this->m_pContent));
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pInbound));
      case IS_CASE_INSENSITIVE:
         return IndexCaseFindUtf8(
                      this->m_pContent, pInbound);
      case IS_LENGTH_LIMITED:
         return IndexLenFindUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pInbound));
      default:
         return IndexFindUtf8(
                      this->m_pContent, pInbound);
   }
}

int FastUtf8::Uniseries::find(const char *pInbound) const noexcept
{
   char *pszStrstrResult;

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pInbound)));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return IndexCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound);
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pInbound)));
      case IS_7BIT_CHAR_STRING:
      {
         pszStrstrResult = std::strstr(
                      reinterpret_cast<char *> (this->m_pContent), pInbound);
         
         if (pszStrstrResult == nullptr)
         {
            return -1;
         }
         else
         {
            return static_cast<int> 
               (pszStrstrResult - reinterpret_cast<char *> (this->m_pContent));
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pInbound)));
      case IS_CASE_INSENSITIVE:
         return IndexCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound));
      case IS_LENGTH_LIMITED:
         return IndexLenFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pInbound)));
      default:
         return IndexFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound));
   }
}

int FastUtf8::Uniseries::find(const char cInbound) const noexcept
{
   char szInbound[2] = { cInbound, '\0' };
   char *pszStrstrResult;

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), szInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1);
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return IndexCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), szInbound);
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), szInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1);
      case IS_7BIT_CHAR_STRING:
      {
         pszStrstrResult = std::strstr(
                      reinterpret_cast<char *> (this->m_pContent), szInbound);
         
         if (pszStrstrResult == nullptr)
         {
            return -1;
         }
         else
         {
            return static_cast<int> 
               (pszStrstrResult - reinterpret_cast<char *> (this->m_pContent));
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1);
      case IS_CASE_INSENSITIVE:
         return IndexCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound));
      case IS_LENGTH_LIMITED:
         return IndexLenFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1);
      default:
         return IndexFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<uint8_t *> (szInbound));
   }
}

// This partial content comparison method returns an index of the "that" 
// object's content within the "this" object's content -- that is, a count 
// of the code points between the beginning of "this" content and any first 
// match -- or a negative return value (-1) in case that content is not 
// found.
int FastUtf8::Uniseries::find(
                             const FastUtf8::Uniseries& that) const noexcept
{
   char *pszStrstrResult;

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength());
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return IndexCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength());
      case IS_7BIT_CHAR_STRING:
      {
         pszStrstrResult = std::strstr(reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()));
         
         if (pszStrstrResult == nullptr)
         {
            return -1;
         }
         else
         {
            return static_cast<int> 
               (pszStrstrResult - reinterpret_cast<char *> (this->m_pContent));
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength());
      case IS_CASE_INSENSITIVE:
         return IndexCaseFindUtf8(
                      this->m_pContent, that.getContent());
      case IS_LENGTH_LIMITED:
         return IndexLenFindUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength());
      default:
         return IndexFindUtf8(this->m_pContent, that.getContent());
   }
}

// This partial content comparison method returns a pointer to the inbound 
// search content within the "this" object's content -- that is, a raw pointer 
// to any first match in "this" -- or a nullptr return value in case that 
// content is not found.  The optional pFirst parameter refers to a location, 
// in the "this" content, to begin seeking a match.  The method updates pFirst 
// and pLast to return the boundaries of any first matching content within 
// "this".
uint8_t * FastUtf8::Uniseries::pFind(
      const uint8_t *pSearchContent,   // Needle
      const uint8_t *pFirst,           // Beginning location
      uint8_t       **ppLast) const    // Returned location where match ends
{
   const uint8_t *pContent;
   uint8_t       *pMatch;
   size_t        sizeMatch;

   if (pFirst)
   {
      pContent = pFirst;
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pSearchContent))), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pSearchContent))), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
      {
         pMatch = reinterpret_cast<uint8_t *> (const_cast<char *> (std::strstr(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent))));
         if (pMatch)
         {
            sizeMatch = std::strlen(
                      reinterpret_cast<const char *> (pSearchContent));
            if (ppLast)
            {
               *ppLast = const_cast<uint8_t *> (pContent) + sizeMatch;
            }

            return pMatch;
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(
                      pContent, pSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pSearchContent), 
                      ppLast);
      case IS_CASE_INSENSITIVE:
         return CaseFindUtf8(pContent, pSearchContent, ppLast);
      case IS_LENGTH_LIMITED:
         return LenFindUtf8(
                      pContent, pSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pSearchContent), ppLast);
      default:
         return FindUtf8(pContent, pSearchContent, ppLast);
   }
}

uint8_t * FastUtf8::Uniseries::pFind(
      const char    *pSearchContent,   // Needle
      const char    *pFirst,           // Beginning location
      uint8_t       **ppLast) const    // Returned location where match ends
{
   const uint8_t *pContent;
   uint8_t       *pMatch;
   size_t        sizeMatch;

   if (pFirst)
   {
      pContent = reinterpret_cast<const uint8_t *> (pFirst);
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      pSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pSearchContent)), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      pSearchContent, 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      pSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pSearchContent)), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
      {
         pMatch = reinterpret_cast<uint8_t *> (const_cast<char*> (std::strstr(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent))));
         if (pMatch)
         {
            sizeMatch = std::strlen(
                      reinterpret_cast<const char *> (pSearchContent));
            if (ppLast)
            {
               *ppLast = const_cast<uint8_t *> (pContent) + sizeMatch;
            }

            return pMatch;
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(
                      pContent, 
                      reinterpret_cast<const uint8_t *> (pSearchContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pSearchContent)), 
                      ppLast);
      case IS_CASE_INSENSITIVE:
         return CaseFindUtf8(
                      pContent, 
                      reinterpret_cast<const uint8_t *> (pSearchContent), 
                      ppLast);
      case IS_LENGTH_LIMITED:
         return LenFindUtf8(
                      pContent, 
                      reinterpret_cast<const uint8_t *> (pSearchContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pSearchContent)), 
                      ppLast);
      default:
         return FindUtf8(
                      pContent, 
                      reinterpret_cast<const uint8_t *> (pSearchContent), 
                      ppLast);
   }
}

uint8_t * FastUtf8::Uniseries::pFind(
      const char    cSearchContent,   // Needle
      const uint8_t *pFirst,          // Beginning location
      uint8_t       **ppLast) const   // Returned location where match ends
{
   const uint8_t *pContent;
   uint8_t       *pMatch;
   char          szSearchContent[2] = { cSearchContent, '\0' };

   if (pFirst)
   {
      pContent = pFirst;
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), szSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1, 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), szSearchContent, 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenFindAscii(
                      reinterpret_cast<const char *> (pContent), szSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1, 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
	  {
         pMatch = reinterpret_cast<uint8_t *> (const_cast<char *> ((std::strstr(
                      reinterpret_cast<const char *> (pContent), szSearchContent))));
         if (pMatch)
         {
            if (ppLast)
            {
               *ppLast = const_cast<uint8_t *> (pContent) + 1;
            }

            return pMatch;
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(
                      pContent, 
                      reinterpret_cast<uint8_t *> (szSearchContent),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1, ppLast);
      case IS_CASE_INSENSITIVE:
         return CaseFindUtf8(
                      pContent, reinterpret_cast<uint8_t *> (szSearchContent), 
                      ppLast);
      case IS_LENGTH_LIMITED:
         return LenFindUtf8(
                      pContent, reinterpret_cast<uint8_t *> (szSearchContent),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      /* lenContent = */ 1, ppLast);
      default:
         return FindUtf8(
                      pContent, reinterpret_cast<uint8_t *> (szSearchContent), 
                      ppLast);
   }
}

// This partial content comparison method returns a pointer to any first 
// occurrence of the "that" object's content within the "this" object's 
// content, or nullptr in case that content is not found.
uint8_t * FastUtf8::Uniseries::pFind(
      const FastUtf8::Uniseries& that,   // Uniseries containing needle
      const uint8_t *pFirst,             // Updated beginning location
      uint8_t       **ppLast) const      // Returned location where match ends
{
   const uint8_t *pContent;
   uint8_t       *pMatch;
   size_t        sizeMatch;

   if (pFirst)
   {
      pContent = pFirst;
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & FLAGS_MASK)
   {
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_CASE_INSENSITIVE:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (that.getContent()), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
      {
         pMatch = reinterpret_cast<uint8_t *> (const_cast<char *> (std::strstr(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (that.getContent()))));
         if (pMatch)
         {
            sizeMatch = std::strlen(
                      reinterpret_cast<const char *> (that.getContent()));
            if (ppLast)
            {
               *ppLast = const_cast<uint8_t *> (pContent) + sizeMatch;
            }

            return pMatch;
         }
      }
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(
                      pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), ppLast);
      case IS_CASE_INSENSITIVE:
         return CaseFindUtf8(
                      pContent, that.getContent(), ppLast);
      case IS_LENGTH_LIMITED:
         return LenFindUtf8(
                      pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), ppLast);
      default:
         return FindUtf8(
                      pContent, that.getContent(), ppLast);
   }
}

// This case-insensitive partial content comparison method returns an index 
// (offset code point count) of the inbound content within "this" object's 
// content, or -1 if the inbound content is not found.  It disregards the 
// IS_CASE_INSENSITIVE flag.
bool FastUtf8::Uniseries::caseContains(const uint8_t *pInbound) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pInbound))), 
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound),
                      /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pInbound), /* ppLast = */ nullptr));
      default:
         return (nullptr != CaseFindUtf8(this->m_pContent, pInbound,
                      /* ppLast = */ nullptr));
   }
}

bool FastUtf8::Uniseries::caseContains(const char *pInbound) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound,
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pInbound)), 
                      /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound,
                      /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound),
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pInbound)), 
                      /* ppLast = */ nullptr));
      default:
         return (nullptr != CaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound), 
                      /* ppLast = */ nullptr));
   }
}

// This case-insensitive partial content comparison method returns an index 
// (offset code point count) of the "that" content within the "this" 
// content, or -1 if that content is not found.  It disregards the 
// IS_CASE_INSENSITIVE flag.
bool FastUtf8::Uniseries::caseContains(
                             const FastUtf8::Uniseries& that) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), /* ppLast = */ nullptr));
      case IS_7BIT_CHAR_STRING:
         return (nullptr != CaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()),
                      /* ppLast = */ nullptr));
      case IS_LENGTH_LIMITED:
         return (nullptr != LenCaseFindUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), /* ppLast = */ nullptr));
      default:
         return (nullptr != CaseFindUtf8(
                      this->m_pContent, that.getContent(), 
                      /* ppLast = */ nullptr));
   }
}

// This case-insensitive partial content comparison method returns an index 
// (offset code point count) of the inbound content within "this" object's 
// content, or -1 if the inbound content is not found.  It disregards the 
// IS_CASE_INSENSITIVE flag.
int FastUtf8::Uniseries::caseFind(const uint8_t *pInbound) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pInbound))));
      case IS_7BIT_CHAR_STRING:
         return IndexCaseFindAscii(
                      reinterpret_cast<const char *> (this->m_pContent), 
                      reinterpret_cast<const char *> (pInbound));
      case IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pInbound));
      default:
         return IndexCaseFindUtf8(this->m_pContent, pInbound);
   }
}

int FastUtf8::Uniseries::caseFind(const char *pInbound) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pInbound)));
      case IS_7BIT_CHAR_STRING:
         return IndexCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), pInbound);
      case IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(reinterpret_cast<const uint8_t *> (pInbound)));
      default:
         return IndexCaseFindUtf8(
                      this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pInbound));
   }
}

// This case-insensitive partial content comparison method returns an index 
// (offset code point count) of the "that" content within the "this" 
// content, or -1 if that content is not found.  It disregards the 
// IS_CASE_INSENSITIVE flag.
int FastUtf8::Uniseries::caseFind(
                             const FastUtf8::Uniseries& that) const noexcept
{
   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return IndexLenCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength());
      case IS_7BIT_CHAR_STRING:
         return IndexCaseFindAscii(
                      reinterpret_cast<char *> (this->m_pContent), 
                      reinterpret_cast<char *> (that.getContent()));
      case IS_LENGTH_LIMITED:
         return IndexLenCaseFindUtf8(
                      this->m_pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength());
      default:
         return IndexCaseFindUtf8(
                      this->m_pContent, that.getContent());
   }
}

// This case-insensitive partial content comparison method returns a pointer  
// to any first occurrence of the inbound content within "this" object's 
// content, or nullptr if the inbound content is not found.  It disregards the 
// IS_CASE_INSENSITIVE flag.
uint8_t * FastUtf8::Uniseries::casepFind(
      const uint8_t *pSearchContent,  // Needle
      const uint8_t *pFirst,          // Beginning location
      uint8_t       **ppLast) const   // Returned location where match ends
{
   const uint8_t *pContent;

   if (pFirst)
   {
      pContent = pFirst;
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(
                         reinterpret_cast<const char *> (pSearchContent))), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (pSearchContent), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(
                      pContent, pSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pSearchContent), ppLast);
      default:
         return CaseFindUtf8(pContent, pSearchContent, ppLast);
   }
}

uint8_t * FastUtf8::Uniseries::casepFind(
      const char *pSearchContent,      // Needle
      const char *pFirst,              // Beginning location
      uint8_t    **ppLast) const       // Returned location where match ends
{
   const uint8_t *pContent;

   if (pFirst)
   {
      pContent = reinterpret_cast<const uint8_t *> (pFirst);
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      pSearchContent, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      static_cast<int> (std::strlen(pSearchContent)), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      pSearchContent, 
                      reinterpret_cast<char **> (ppLast)));
      case IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(pContent, 
                      reinterpret_cast<const uint8_t *> (pSearchContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pSearchContent)), 
                      ppLast);
      default:
         return CaseFindUtf8(pContent, 
                      reinterpret_cast<const uint8_t *> (pSearchContent), 
                      ppLast);
   }
}

// This case-insensitive partial content comparison method returns a pointer 
// to the "that" content within the "this" content, or nullptr if that content 
// is not found.  It disregards the IS_CASE_INSENSITIVE flag.
uint8_t * FastUtf8::Uniseries::casepFind(
      const FastUtf8::Uniseries& that,   // Object containing needle
      const uint8_t *pFirst,             // Beginning location
      uint8_t **ppLast) const            // Returned location where match ends
{
   const uint8_t *pContent;

   if (pFirst)
   {
      pContent = pFirst;
   }
   else
   {
      pContent = this->m_pContent;
   }

   switch (this->m_metadata & (IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED))
   {
      case IS_7BIT_CHAR_STRING | IS_LENGTH_LIMITED:
         return reinterpret_cast<uint8_t *> (LenCaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (that.getContent()), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_7BIT_CHAR_STRING:
         return reinterpret_cast<uint8_t *> (CaseFindAscii(
                      reinterpret_cast<const char *> (pContent), 
                      reinterpret_cast<const char *> (that.getContent()), 
                      reinterpret_cast<char **> (ppLast)));
      case IS_LENGTH_LIMITED:
         return LenCaseFindUtf8(
                      pContent, that.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      that.getLength(), ppLast);
      default:
         return CaseFindUtf8(
                      pContent, that.getContent(), ppLast);
   }
}

// This method provides for wildcard-based content comparison.  The "this" 
// content may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::wildCompare(const uint8_t *pTame) const noexcept
{
   switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
   {
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                      this->m_pContent, pTame, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pTame));
      case IS_CASE_INSENSITIVE:
         return WildCaseCompareUtf8(this->m_pContent, pTame);
      case IS_LENGTH_LIMITED:
         return WildLenCompareUtf8(
                      this->m_pContent, pTame, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pTame));
      default:
         return WildCompareUtf8(this->m_pContent, pTame);
   }
}

// content may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::wildCompare(const char *pTame) const noexcept
{
   switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
   {
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pTame), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pTame)));
      case IS_CASE_INSENSITIVE:
         return WildCaseCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pTame));
      case IS_LENGTH_LIMITED:
         return WildLenCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pTame), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pTame)));
      default:
         return WildCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pTame));
   }
}

// This method provides for wildcard-based content comparison.  The "this" 
// content may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::wildCompare(
                             const FastUtf8::Uniseries& tame) const noexcept
{
   switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
   {
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                   this->m_pContent, tame.getContent(), 
                   static_cast<int> (this->m_metadata & LENGTH_MASK), 
                   tame.getLength());
      case IS_CASE_INSENSITIVE:
         return WildCaseCompareUtf8(
                   this->m_pContent, tame.getContent());
      case IS_LENGTH_LIMITED:
         return WildLenCompareUtf8(
                   this->m_pContent, tame.getContent(), 
                   static_cast<int> (this->m_metadata & LENGTH_MASK), 
                   tame.getLength());
      default:
         return WildCompareUtf8(
                   this->m_pContent, tame.getContent());
   }
}

// This method provides for wildcard-based content comparison.  The passed-in 
// content is the content that may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::compareWild(const uint8_t *pWild) const noexcept
{
   switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
   {
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                      pWild, this->m_pContent, 
                      CodePointCountUtf8(pWild), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      case IS_CASE_INSENSITIVE:
         return WildCaseCompareUtf8(pWild, this->m_pContent);
      case IS_LENGTH_LIMITED:
         return WildLenCompareUtf8(
                      pWild, this->m_pContent, 
                      CodePointCountUtf8(pWild), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return WildCompareUtf8(pWild, this->m_pContent);
   }
}

// content may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::compareWild(const char *pWild) const noexcept
{
   switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
   {
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                      reinterpret_cast<const uint8_t *> (pWild), 
                      this->m_pContent, 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      case IS_CASE_INSENSITIVE:
         return WildCaseCompareUtf8(
                      reinterpret_cast<const uint8_t *> (pWild), 
                      this->m_pContent);
      case IS_LENGTH_LIMITED:
         return WildLenCompareUtf8(
                      reinterpret_cast<const uint8_t *> (pWild), 
                      this->m_pContent, 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return WildCompareUtf8(
                      reinterpret_cast<const uint8_t *> (pWild), 
                      this->m_pContent);
   }
}

// This method provides for wildcard-based content comparison.  The passed-in 
// content is the content that may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::compareWild(
                             const FastUtf8::Uniseries& wild) const noexcept
{
   switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
   {
      case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                   wild.getContent(), this->m_pContent, 
                   wild.getLength(), 
                   static_cast<int> (this->m_metadata & LENGTH_MASK));
      case IS_CASE_INSENSITIVE:
         return WildCaseCompareUtf8(
                   wild.getContent(), this->m_pContent);
      case IS_LENGTH_LIMITED:
         return WildLenCompareUtf8(
                   wild.getContent(), this->m_pContent, 
                   wild.getLength(), 
                   static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return WildCompareUtf8(
                   wild.getContent(), this->m_pContent);
   }
}

// This method provides for just case-insensitive wildcard-based content 
// comparison.  It does not check the IS_CASE_INSENSITIVE flag.  The "this" 
// content may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::wildCaseCompare(const uint8_t *pTame) const noexcept
{
   switch (this->m_metadata & IS_LENGTH_LIMITED)
   {
      case IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                      this->m_pContent, pTame, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(pTame));
      default:
         return WildCaseCompareUtf8(this->m_pContent, pTame);
   }
}

bool FastUtf8::Uniseries::wildCaseCompare(const char *pTame) const noexcept
{
   switch (this->m_metadata & IS_LENGTH_LIMITED)
   {
      case IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pTame), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pTame)));
      default:
         return WildCaseCompareUtf8(this->m_pContent, 
                      reinterpret_cast<const uint8_t *> (pTame));
   }
}

// This method provides for just case-insensitive wildcard-based content 
// comparison.  It does not check the IS_CASE_INSENSITIVE flag.  The "this" 
// content may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::wildCaseCompare(
                             const FastUtf8::Uniseries& tame) const noexcept
{
   switch (this->m_metadata & IS_LENGTH_LIMITED)
   {
      case IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                   this->m_pContent, tame.getContent(), 
                   static_cast<int> (this->m_metadata & LENGTH_MASK), 
                   tame.getLength());
      default:
         return WildCaseCompareUtf8(
                   this->m_pContent, tame.getContent());
   }
}

// This method provides for just case-insensitive wildcard-based content 
// comparison.  It does not check the IS_CASE_INSENSITIVE flag.  The passed-in 
// content is the content that may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::caseCompareWild(const uint8_t *pWild) const noexcept
{
   switch (this->m_metadata & IS_LENGTH_LIMITED)
   {
      case IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                      pWild, this->m_pContent, 
                      CodePointCountUtf8(pWild), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return WildCaseCompareUtf8(pWild, this->m_pContent);
   }
}

bool FastUtf8::Uniseries::caseCompareWild(const char *pWild) const noexcept
{
   switch (this->m_metadata & IS_LENGTH_LIMITED)
   {
      case IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                      reinterpret_cast<const uint8_t *> (pWild), 
                      this->m_pContent, 
                      CodePointCountUtf8(reinterpret_cast<const uint8_t *> (pWild)), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return WildCaseCompareUtf8(
                      reinterpret_cast<const uint8_t *> (pWild), 
                      this->m_pContent);
   }
}

// This method provides for just case-insensitive wildcard-based content 
// comparison.  It does not check the IS_CASE_INSENSITIVE flag.  The passed-in 
// content is the content that may include the '*' or '?' wildcards.
bool FastUtf8::Uniseries::caseCompareWild(
                             const FastUtf8::Uniseries& wild) const noexcept
{
   switch (this->m_metadata & IS_LENGTH_LIMITED)
   {
      case IS_LENGTH_LIMITED:
         return WildLenCaseCompareUtf8(
                   wild.getContent(), this->m_pContent, 
                   wild.getLength(), 
                   static_cast<int> (this->m_metadata & LENGTH_MASK));
      default:
         return WildCaseCompareUtf8(
                   wild.getContent(), this->m_pContent);
   }
}

// The pFindWild() partial content comparison method scans the "this" 
// content for a first match on the inbound search pattern which is 
// expected to contain wildcards.  Returns a pointer to the first matching 
// portion of the content.  The optional parameters are returned targeted 
// wildcard matching results, for example given this sentence:
//
// content: "This part is skipped, but here is some content to be matched."
// pFirst:                            "here is some content to be matched." 
// pWild:   "some*to*match"                    (location provided by caller)
//
// That is, if the caller points pFirst to "here is some content to be 
// matched" and provides pointers for ppLast and ppTarget, the method will 
// set the pointers within the content, this way:
// 
// return value: "some content to be matched."  (location of first match)
// *ppLast:                             "hed."  (location set by the method)
// *ppTarget:                       "matched."  (location set by the method)
//
// This is useful for seeking a relatively large portion of content that 
// includes a specified target portion within it.  The method provides a 
// speedy way for software to find a line, paragraph, stanza, or other 
// programmatically distinguishable piece of writing that has a particular 
// word or phrase in it, in some cases via just one call.  Additional calls, 
// as needed for example to compare a target to an item in a list, can 
// include applying the slice() method to construct a Uniseries with 
// *ppTarget and *ppLast as the slice() parameters, then applying one of 
// the Uniseries compare methods.
//
// If no match is found, or if no first wildcard is found, sets *ppFirst, 
// *ppLast, and *ppTarget to nullptr and returns nullptr.
uint8_t * FastUtf8::Uniseries::pFindWild(
   const uint8_t *pWild,        // Search pattern (with wildcards)
   uint8_t       **ppFirst,     // Updated beginning location
   uint8_t       **ppLast,      // Returned loc where match ends
   uint8_t       **ppTarget)    // Returned loc after last '*'
      const noexcept
{
   if (ppFirst)
   {
      switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(ppFirst, pWild, 
                      CodePointCountUtf8(*ppFirst),
                      CodePointCountUtf8(pWild), ppLast, ppTarget);
         case IS_CASE_INSENSITIVE:
            return WildCaseFindUtf8(ppFirst, pWild, ppLast, ppTarget);
         case IS_LENGTH_LIMITED:
            return WildLenFindUtf8(ppFirst, pWild, 
                      CodePointCountUtf8(*ppFirst),
                      CodePointCountUtf8(pWild), ppLast, ppTarget);
         default:
            return WildFindUtf8(ppFirst, pWild, ppLast, ppTarget);
      }
   }
   else
   {
      uint8_t *pContent = this->m_pContent;

      switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(&pContent, pWild, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK),
                      CodePointCountUtf8(pWild), ppLast, ppTarget);
         case IS_CASE_INSENSITIVE:
            return WildCaseFindUtf8(&pContent, pWild, ppLast, ppTarget);
         case IS_LENGTH_LIMITED:
            return WildLenFindUtf8(&pContent, pWild, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK),
                      CodePointCountUtf8(pWild), ppLast, ppTarget);
         default:
            return WildFindUtf8(&pContent, pWild, ppLast, ppTarget);
      }
   }
}

uint8_t * FastUtf8::Uniseries::pFindWild(
   const char *pWild,                 // Search pattern (with wildcards)
   uint8_t    **ppFirst,              // Updated beginning location
   uint8_t    **ppLast,               // Returned loc where match ends
   uint8_t    **ppTarget)             // Returned loc after last '*'
      const noexcept
{
   if (ppFirst)
   {
      switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(ppFirst, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      CodePointCountUtf8(*ppFirst), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      ppLast, ppTarget);
         case IS_CASE_INSENSITIVE:
            return WildCaseFindUtf8(ppFirst, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      ppLast, ppTarget);
         case IS_LENGTH_LIMITED:
            return WildLenFindUtf8(ppFirst, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      CodePointCountUtf8(*ppFirst), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      ppLast, ppTarget);
         default:
            return WildFindUtf8(ppFirst, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      ppLast, ppTarget);
      }
   }
   else
   {
      uint8_t *pContent = this->m_pContent;

      switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(&pContent, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      ppLast, ppTarget);
         case IS_CASE_INSENSITIVE:
            return WildCaseFindUtf8(&pContent, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      ppLast, ppTarget);
         case IS_LENGTH_LIMITED:
            return WildLenFindUtf8(&pContent, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      ppLast, ppTarget);
         default:
            return WildFindUtf8(&pContent, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      ppLast, ppTarget);
      }
   }
}

uint8_t * FastUtf8::Uniseries::pFindWild(
   const Uniseries& sWild,               // Search pattern (with wildcards)
   uint8_t       **ppFirst,              // Updated beginning location
   uint8_t       **ppLast,               // Returned loc where match ends
   uint8_t       **ppTarget)             // Returned loc after last '*'
      const noexcept
{
   if (ppFirst)
   {
      switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(ppFirst, sWild.getContent(), 
                      CodePointCountUtf8(*ppFirst), sWild.getLength(), 
                      ppLast, ppTarget);
         case IS_CASE_INSENSITIVE:
            return WildCaseFindUtf8(ppFirst, sWild.getContent(), 
                      ppLast, ppTarget);
         case IS_LENGTH_LIMITED:
            return WildLenFindUtf8(ppFirst, sWild.getContent(), 
                      CodePointCountUtf8(*ppFirst), sWild.getLength(), 
                      ppLast, ppTarget);
         default:
            return WildFindUtf8(ppFirst, sWild.getContent(), 
                      ppLast, ppTarget);
      }
   }
   else
   {
      uint8_t *pContent = this->m_pContent;

      switch (this->m_metadata & (IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED))
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(&pContent, sWild.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sWild.getLength(), ppLast, ppTarget);
         case IS_CASE_INSENSITIVE:
            return WildCaseFindUtf8(&pContent, sWild.getContent(), 
                      ppLast, ppTarget);
         case IS_LENGTH_LIMITED:
            return WildLenFindUtf8(&pContent, sWild.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sWild.getLength(), ppLast, ppTarget);
         default:
            return WildFindUtf8(&pContent, sWild.getContent(), 
                      ppLast, ppTarget);
      }
   }
}

// Case-insensitive implementation of the pFindWild() partial content 
// comparison method.
uint8_t * FastUtf8::Uniseries::casepFindWild(
   const uint8_t *pWild,        // Search pattern (with wildcards)
   uint8_t       **ppFirst,     // Updated beginning location
   uint8_t       **ppLast,      // Returned loc where match ends
   uint8_t       **ppTarget)    // Returned loc after last '*'
      const noexcept
{
   if (ppFirst)
   {
      switch (this->m_metadata & IS_LENGTH_LIMITED)
      {
         case IS_CASE_INSENSITIVE | IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(ppFirst, pWild, 
                      CodePointCountUtf8(*ppFirst),
                      CodePointCountUtf8(pWild), ppLast, ppTarget);
         default:
            return WildCaseFindUtf8(ppFirst, pWild, ppLast, ppTarget);
      }
   }
   else
   {
      uint8_t *pContent = this->m_pContent;

      switch (this->m_metadata & IS_LENGTH_LIMITED)
      {
         case IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(&pContent, pWild, 
                      static_cast<int> (this->m_metadata & LENGTH_MASK),
                      CodePointCountUtf8(pWild), ppLast, ppTarget);
         default:
            return WildCaseFindUtf8(&pContent, pWild, ppLast, ppTarget);
      }
   }
}

uint8_t * FastUtf8::Uniseries::casepFindWild(
   const char *pWild,                 // Search pattern (with wildcards)
   uint8_t    **ppFirst,              // Updated beginning location
   uint8_t    **ppLast,               // Returned loc where match ends
   uint8_t    **ppTarget)             // Returned loc after last '*'
      const noexcept
{
   if (ppFirst)
   {
      switch (this->m_metadata & IS_LENGTH_LIMITED)
      {
         case IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(ppFirst, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      CodePointCountUtf8(*ppFirst), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      ppLast, ppTarget);
         default:
            return WildCaseFindUtf8(ppFirst, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      ppLast, ppTarget);
      }
   }
   else
   {
      uint8_t *pContent = this->m_pContent;

      switch (this->m_metadata & IS_LENGTH_LIMITED)
      {
         case IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(&pContent, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      CodePointCountUtf8(
                         reinterpret_cast<const uint8_t *> (pWild)), 
                      ppLast, ppTarget);
         default:
            return WildCaseFindUtf8(&pContent, 
                      reinterpret_cast<const uint8_t *> (pWild), 
                      ppLast, ppTarget);
      }
   }
}

uint8_t * FastUtf8::Uniseries::casepFindWild(
   const Uniseries& sWild,               // Search pattern (with wildcards)
   uint8_t       **ppFirst,              // Updated beginning location
   uint8_t       **ppLast,               // Returned loc where match ends
   uint8_t       **ppTarget)             // Returned loc after last '*'
      const noexcept
{
   if (ppFirst)
   {
      switch (this->m_metadata & IS_LENGTH_LIMITED)
      {
         case IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(ppFirst, sWild.getContent(), 
                      CodePointCountUtf8(*ppFirst), sWild.getLength(), 
                      ppLast, ppTarget);
         default:
            return WildCaseFindUtf8(ppFirst, sWild.getContent(), 
                      ppLast, ppTarget);
      }
   }
   else
   {
      uint8_t *pContent = this->m_pContent;

      switch (this->m_metadata & IS_LENGTH_LIMITED)
      {
         case IS_LENGTH_LIMITED:
            return WildLenCaseFindUtf8(&pContent, sWild.getContent(), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK), 
                      sWild.getLength(), ppLast, ppTarget);
         default:
            return WildCaseFindUtf8(&pContent, sWild.getContent(), 
                      ppLast, ppTarget);
      }
   }
}

// The non-const and const subscript (index) operators call their 
// supporting C function, unless the entire content is ASCII text.
uint32_t FastUtf8::Uniseries::operator[](int iIndex) noexcept
{
   if (!this->m_pContent)
   {
      return 0;
   }
   else if (this->m_metadata & IS_7BIT_CHAR_STRING && 
       iIndex < static_cast<int> (this->m_metadata & LENGTH_MASK))
   {
      return this->m_pContent[iIndex];
   }
   else
   {
      return IndexUtf8(this->m_pContent, iIndex);
   }
}

const uint32_t FastUtf8::Uniseries::operator[](int iIndex) const noexcept
{
   if (!this->m_pContent)
   {
      return 0;
   }
   else if (this->m_metadata & IS_7BIT_CHAR_STRING && 
       iIndex < static_cast<int> (this->m_metadata & LENGTH_MASK))
   {
      return this->m_pContent[iIndex];
   }
   else
   {
      return IndexUtf8(this->m_pContent, iIndex);
   }
}

// This method removes outboard white space from the object's current 
// content.
void FastUtf8::Uniseries::trim(void)
{
   if (this->m_pContent)
   {
      if (this->m_metadata & IS_7BIT_CHAR_STRING)
      {
         this->m_pContent = reinterpret_cast<uint8_t *> (TrimAscii(
                         reinterpret_cast<char *> (this->m_pContent)));
         this->m_metadata &= ~(static_cast<uint64_t> (LENGTH_MASK));
         this->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (std::strlen(
                         reinterpret_cast<char *> (this->m_pContent)));
      }
      else
      {
         this->m_pContent = TrimUtf8(this->m_pContent);
         this->m_metadata &= ~(static_cast<uint64_t> (LENGTH_MASK));
         this->m_metadata |= 
                      LENGTH_MASK & static_cast<uint64_t> (CodePointCountUtf8(
                         this->m_pContent));
      }
   }

   return;
}

// This method validates the object's current UTF-8 content.  It returns 
// true if the content is valid.  If IS_LENGTH_LIMITED is set, it validates 
// as many code points as have been specified via the setLengthLimit() 
// method.  Otherwise it validates code points until it encounters a 
// terminating null and gets their count, which it returns via the iCount 
// parameter.
bool FastUtf8::Uniseries::validate(int *iCount) const noexcept
{
   if (!this->m_pContent)
   {
      return false;
   }
   else if (this->m_metadata & IS_LENGTH_LIMITED)
   {
      return LenValidateUtf8(
                   this->m_pContent, 
                   static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      return ValidateUtf8(
                   this->m_pContent, iCount);
   }
}

bool FastUtf8::Uniseries::validate(void) const noexcept
{
   int iCount;

   if (!this->m_pContent)
   {
      return false;
   }
   else
   {
      return ValidateUtf8(this->m_pContent, &iCount);
   }
}

// In case content validation fails, this method may serve as a reasonable 
// fallback.  It returns valid UTF-8 content and sets the iCount parameter 
// reflecting the content's length, as a count of its code points.
uint8_t * FastUtf8::Uniseries::convert8BitAscii(int *iCount) noexcept
{
   if (!this->m_pContent)
   {
      return nullptr;
   }
   else if (this->m_metadata & IS_LENGTH_LIMITED)
   {
      return LenConvert8BitAsciiToUtf8(
                      reinterpret_cast<char *> (this->m_pContent), 
                      static_cast<int> (this->m_metadata & LENGTH_MASK));
   }
   else
   {
      return Convert8BitAsciiToUtf8(
                      reinterpret_cast<char *> (this->m_pContent), iCount);
   }

}

uint8_t * FastUtf8::Uniseries::convert8BitAscii(void) noexcept
{
   int iCount;

   if (!this->m_pContent)
   {
      return nullptr;
   }
   else 
   {
      return Convert8BitAsciiToUtf8(
                      reinterpret_cast<char *> (this->m_pContent), &iCount);
   }
}

// The << operator sends char * output to theStream.
std::ostream& operator<<(std::ostream& theStream, 
                      const FastUtf8::Uniseries& theOutput)
{
   if (theOutput.m_pContent)
   {
      theStream << reinterpret_cast<char *> (theOutput.m_pContent);
   }

   return theStream;
}

// A counterpart >> operator would be relatively complicated.  It would have 
// to manage a resizeable input buffer to handle input of arbitrary size, and 
// it would have to ensure the needed memory management for the content 
// buffer itself (m_pContent).

// This Uniseries comparison operator applies for pointer == object.
bool operator==(const Uniseries *puSeries, const Uniseries& uSeries)
{
   if (!puSeries)
   {
      return false;
   }

   return *puSeries == uSeries;  // Invoke Uniseries operator==
}

// This Uniseries comparison operator applies for object == pointer.
bool operator==(const Uniseries& uSeries, const Uniseries *puSeries)
{
   if (!puSeries)
   {
      return false;
   }

   return uSeries == *puSeries;
}

};       // namespace FastUtf8
#endif   // __cplusplus

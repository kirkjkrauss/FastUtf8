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
#if defined(_MSC_VER)
#include <windows.h>
#include <shlwapi.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strcasestr StrStrIA
#define strdup _strdup
#endif

#include <stdlib.h>          // For memory management
#include <ctype.h>           // For isspace()

#if !defined(NULLPTR)
#define NULLPTR (0)
#endif

#if !defined(TRUE)
#define TRUE (1)
#endif

#if !defined(FALSE)
#define FALSE (0)
#endif

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
// point.  Returns TRUE if there is a further code point, or FALSE if the 
// next content is a terminating null.  PERFORMS NO UTF-8 VALIDATION OTHER
// THAN NULL CHECKING.
//
inline int CodePointAdvanceUtf8(const uint8_t **ppContent)
{
   *ppContent += (**ppContent > 0) +
       ((**ppContent > SINGLETON_LIMIT) && *(1 + *ppContent)) + 
       ((**ppContent > TWOFER_LIMIT) && *(2 + *ppContent)) + 
       ((**ppContent > THREESOME_LIMIT) && *(3 + *ppContent));
   return (bool) **ppContent;
}

// Given a pointer to a UTF-8 code point and a pointer to the beginning of 
// the series that contains it, backtracks the first pointer to any code 
// point immediately "below" it in memory.  Returns TRUE if such a previous 
// code point exists at or "above" the beginning of the series, or FALSE 
// otherwise.  This logic is all exercised in testset_targetedsearch_latin() 
// [testutf8.cpp].
//
inline int CodePointBacktrackUtf8(const uint8_t **ppContent, 
                      const uint8_t *pContentStart)
{
   if ((*ppContent - 1) >= pContentStart && 
       *(*ppContent - 1) <= HALF_ASCII_LIMIT)
   {
      *ppContent -= 1;
	  return TRUE;
   }
   else if ((*ppContent - 2) >= pContentStart && 
            *(*ppContent - 2) > SINGLETON_LIMIT)
   {
      *ppContent -= 2;
	  return TRUE;
   }
   else if ((*ppContent - 3) >= pContentStart && 
             *(*ppContent - 3) > SINGLETON_LIMIT)
   {
      *ppContent -= 3;
	  return TRUE;
   }
   else if ((*ppContent - 4) >= pContentStart)
   {
      *ppContent -= 4;
	  return TRUE;
   }

   return FALSE;
}

// Compares two UTF-8 code points.  Returns TRUE if the code points are 
// identical.  Returns FALSE otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline int CodePointCompareUtf8(const uint8_t *pContentA, 
                      const uint8_t *pContentB)
{
   if (*pContentA != *pContentB)
   {
      return FALSE;
   }
   else if (*pContentA > SINGLETON_LIMIT &&
            *(1 + pContentA) != *(1 + pContentB))
   {
      return FALSE;
   }
   else if (*pContentA > TWOFER_LIMIT &&
            *(2 + pContentA) != *(2 + pContentB))
   {
      return FALSE;
   }
   else if (*pContentA > THREESOME_LIMIT &&
            *(3 + pContentA) != *(3 + pContentB))
   {
      return FALSE;
   }

   return TRUE;
}

// Compares two UTF-8 code points.  Advances the second pointer to any next 
// UTF-8 code point.  Returns TRUE if the code points are identical.  Returns 
// FALSE otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline int CodePointAdvanceAndCompareUtf8(
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
      return FALSE;
   }
   else if (*pContentA > SINGLETON_LIMIT &&
      *(1 + pContentA) != *(1 + *ppContentB))
   {
      return FALSE;
   }
   else if (*pContentA > TWOFER_LIMIT &&
      *(2 + pContentA) != *(2 + *ppContentB))
   {
      return FALSE;
   }
   else if (*pContentA > THREESOME_LIMIT &&
      *(3 + pContentA) != *(3 + *ppContentB))
   {
      return FALSE;
   }

   return TRUE;
}

// Compares two UTF-8 code points.  Returns TRUE if the code points are 
// identical after case folding.  Returns FALSE otherwise.  PERFORMS NO 
// UTF-8 VALIDATION.
//
inline int CodePointCaseCompareUtf8(
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
// next UTF-8 code point.  Returns TRUE if the folded code points are 
// identical.  Returns FALSE otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline int CodePointAdvanceAndCaseCompareUtf8(
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
// TRUE if every code point prior to the terminating null is valid.  Returns 
// FALSE otherwise.
//
int ValidateUtf8(
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
                  return FALSE;
               }

               (*piCount)++;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return FALSE;
            }

            (*piCount)++;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return FALSE;
         }

         (*piCount)++;
         pContent += 2;
      }
      else
      {
         return FALSE;
      }
   }

   return TRUE;
}

// Validates the given content, up to the specified number of code points, 
// starting from the beginning of the content.  Returns TRUE if as many code 
// points are valid.  Returns FALSE otherwise.
//
int LenValidateUtf8(
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
                  return FALSE;
               }

               ++iContent;
               pContent += 4;
               continue;
            }

            if (*(pContent + 2) <= HALF_ASCII_LIMIT || 
                *(pContent + 2) > SINGLETON_LIMIT)
            {
               return FALSE;
            }

            ++iContent;
            pContent += 3;
            continue;
         }

         if (*(pContent + 1) <= HALF_ASCII_LIMIT || 
             *(pContent + 1) > SINGLETON_LIMIT)
         {
            return FALSE;
         }

         ++iContent;
         pContent += 2;
      }
      else
      {
         return FALSE;
      }
   }

   return TRUE;
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
            int *pbIs7BitCharString)      // Returned 7-bit ASCII flag
{
   const uint8_t *pContentOrig = pContent;

   *piCount = 0;
   *pbIs7BitCharString = TRUE;

   while (*pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         (*piCount)++;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         *pbIs7BitCharString = FALSE;

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
         *pbIs7BitCharString = FALSE;
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
            int *pbIs7BitCharString)      // Returned 7-bit ASCII flag
{
   const uint8_t *pContentOrig = pContent;
   int     iContent = 0;

   *pbIs7BitCharString = TRUE;

   while (iContent < lenContent && *pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++iContent;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         *pbIs7BitCharString = FALSE;

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
         *pbIs7BitCharString = FALSE;
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
            int *pbIs7BitCharString)       // Returned 7-bit ASCII flag
{
   int iContent = 0;

   *pbIs7BitCharString = TRUE;

   while (pContent < pLast && *pContent > 0)
   {
      if (*pContent <= HALF_ASCII_LIMIT)
      {
         ++iContent;
         ++pContent;
      }
      else if (*pContent > SINGLETON_LIMIT)
      {
         *pbIs7BitCharString = FALSE;

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
         *pbIs7BitCharString = FALSE;
         return 0;
      }
   }

   return iContent;
}

// Given null-terminated content comprising what may be an 8-bit ASCII string, 
// allocates a buffer sufficient for the equivalent UTF-8 content and places 
// that content in it.  If FREE_INVALID_CONTENT is set, deallocates the block 
// containing the 8-bit ASCII string.  Returns a pointer to the new  buffer, 
// or NULLPTR if the content comprises only 7-bit ASCII characters.
//
// The developer is responsible for ensuring that the allocated buffer for 
// UTF-8 content is deallocated via free(), once it is no longer in use.
//
uint8_t * Convert8BitAsciiToUtf8(
            const char *pContent,     // Content to convert
            int  *lenContent)         // Returned length (in code points)
{
   uint8_t       *pUtf8Base;
   uint8_t       *pUtf8 = NULLPTR;
   int          bGot8BitAscii = FALSE;
   size_t        sizeNeeded = 0;
   unsigned char *pAscii = (unsigned char *) pContent;

   while (*pAscii)
   {
      if (*pAscii > HALF_ASCII_LIMIT)
      {
         if (!bGot8BitAscii)
         {
            bGot8BitAscii = TRUE;
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
      pUtf8Base = NULLPTR;
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
// ASCII string.  Returns a pointer to the new buffer, or NULLPTR if the 
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
   uint8_t *pUtf8 = NULLPTR;
   int    bGot8BitAscii = FALSE;
   size_t  sizeAscii = 0;
   size_t  sizeNeeded = 0;
   unsigned char *pAscii = (unsigned char *) pContent;

   while (sizeAscii < sizeContent && *pAscii)
   {
      if (*pAscii > HALF_ASCII_LIMIT)
      {
         if (!bGot8BitAscii)
         {
            bGot8BitAscii = TRUE;
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
      pUtf8Base = NULLPTR;
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
      return NULLPTR;
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
// with ordinary C/C++ string routines.  Returns TRUE for a 7-bit ASCII 
// string, and FALSE otherwise.
//
int Is7BitUtf8(
            uint8_t *pContent)             // Content to evaluate
{
   do
   {
      if (*pContent > HALF_ASCII_LIMIT)
      {
         return FALSE;
      }
   } while (*pContent++);

   return TRUE;
}

// Given UTF-8 content and its length in code points, determines whether it 
// comprises entirely 7-bit "half ASCII" characters.  Returns TRUE for a 
// 7-bit ASCII string, and FALSE otherwise.
//
int IsLen7BitUtf8(
            uint8_t *pContent,             // Content to evaluate
            int lenContent)                // Code point count
{
   int iContent = 0;                       // Index for content

   do
   {
      if (*pContent > HALF_ASCII_LIMIT)
      {
         return FALSE;
      }
   } while (lenContent > ++iContent && *pContent++);

   return TRUE;
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
      return NULLPTR;
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
      return NULLPTR;
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
// concatenated content, returns NULLPTR.  Otherwise returns a pointer to the 
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
      return NULLPTR;
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
         return NULLPTR;  // Insufficient buffer size for total content.
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
// NULLPTR.  Otherwise returns a pointer to the beginning of the buffer.
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
      return NULLPTR;
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
         return NULLPTR;  // Insufficient buffer size for total content.
      }
   } while (lenAdditionalContent > ++iContent && *pAdditionalContent);

   *pContent = 0;         // Add the terminator.
   return (uint8_t *) pContentBase;   // Copy to buffer complete.
}

// Given a pointer to UTF-8 content and a pointer to one or more delimiter 
// code points, searches the content for the first occurrence of any 
// delimiter.  Replaces that code point in the content with a null terminator, 
// including enough nulls to replace the entire code point.  Returns a pointer 
// to any first delimited content, or NULLPTR if there is no content.  
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
      *ppContent = NULLPTR;
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
               *ppContent = NULLPTR;
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
// pointer to any first delimited portion of the string, or NULLPTR if the 
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
      *ppszText = NULLPTR;
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
               *ppszText = NULLPTR;
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
// Returns NULLPTR if no delimiter is found.  PERFORMS NO UTF-8 VALIDATION 
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
      return NULLPTR;
   }

   pToken = (uint8_t *) pTokenSet;

   // Bypass any initial matching tokens.
   do
   {
      if (CodePointCompareUtf8(pContent, pToken))
      {
         if (!CodePointAdvanceUtf8(&pContent))
         {
            return NULLPTR;
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
   } while (TRUE);

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
                  return NULLPTR;
               }
      
               pToken = (uint8_t *) pTokenSet;
            }
         }
      } while (TRUE);
   }

   return NULLPTR;
}

// Given a pointer to length-limited UTF-8 content and a pointer to a length-
// limited set of one or more delimiter code points, searches the content for 
// the first occurrence of any delimiter.  Bypasses any initial delimiters at 
// the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the code point immediately prior to it.  
// Returns NULLPTR if no delimiter is found.  PERFORMS NO UTF-8 VALIDATION 
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
      return NULLPTR;
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
            return NULLPTR;
         }

         ++iContent;

         if (iContent > lenContent)
         {
            return NULLPTR;
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
                  return NULLPTR;
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

   return NULLPTR;
}

// Given a pointer to a null-terminated ASCII string and a pointer to a null- 
// terminated set of one or more delimiter characters, searches the string for 
// the first occurrence of a delimiter.  Bypasses any initial delimiters at 
// the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the character immediately prior to it.  
// Returns NULLPTR if no delimiter is found.  DOES NOT HANDLE UTF-8.
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
      return NULLPTR;
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
            return NULLPTR;
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
   } while (TRUE);

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
                  return NULLPTR;
               }
    
               pContent = pContentNext;
               pToken = (char *) pTokenSet;
            }
         }
      } while (TRUE);
   }

   return NULLPTR;
}

// Given a pointer to a length-limited ASCII string and a pointer to a length- 
// limited set of one or more delimiter characters, searches the string for 
// the first occurrence of a delimiter.  Bypasses any initial delimiters at 
// the content's start.  In case a delimiter is found within the subsequent 
// content, returns a pointer to the character immediately prior to it.  
// Returns NULLPTR if no delimiter is found.  DOES NOT HANDLE UTF-8.
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
      return NULLPTR;
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
            return NULLPTR;
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
                  return NULLPTR;
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

   return NULLPTR;
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
      } while (TRUE);

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
// Returns TRUE for matching content, and FALSE otherwise.  PERFORMS NO 
// UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
// Some ASCII string comparison functions can return values that indicate 
// whether one string might be considered numerically "less than" another. 
// Though that may be useful for certain sorting arrangements, UTF-8 content 
// sorting might best be coded specifically for one locale or another.
//
int CompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB)      // ...with other content
{
   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return FALSE;
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
         return FALSE;
      }
   } while (*pContentA);

   return (*pContentB == 0);
}

// Determines whether null-terminated UTF-8 content matches, entirely, after 
// case folding.  Returns TRUE for matching content, and FALSE otherwise.
// PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
int CaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB)      // ...with other content
{
   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return FALSE;
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
         return FALSE;
      }
   } while (*pContentA);

   return (*pContentB == 0);
}

// Determines whether UTF-8 content matches, up to a given number of code 
// points or any terminating null.  Returns TRUE for matching content, and 
// FALSE otherwise.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
int LenCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent)      // Code point count
{
   int iContent;                      // Index for content

   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return FALSE;
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

   return (lenContent == iContent ? TRUE : 
           *pContentA ? TRUE : *pContentB == 0);
}

// Determines whether UTF-8 content matches, up to a given number of code 
// points or any terminating null, after case folding.  Returns TRUE for 
// matching content, and FALSE otherwise.  PERFORMS NO UTF-8 VALIDATION OTHER 
// THAN NULL CHECKING.
//
int LenCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent)      // Code point count
{
   int iContent;                      // Index for content

   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return FALSE;
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

   return (lenContent == iContent ? TRUE : 
           *pContentA ? TRUE : *pContentB == 0);
}

// Determines whether content matches, up to a specified number of bytes or 
// any terminating null.  Returns TRUE for matching content, and FALSE 
// otherwise.  PERFORMS NO POINTER VALIDATION AND NO UTF-8 VALIDATION OTHER 
// THAN NULL CHECKING.
//
int SizeCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            size_t        sizeContent)     // Content size (bytes)
{
   size_t nSize;                      // Accumulate size of content

   if (!pContentA || !pContentB)      // Got any empty input?
   {
      return FALSE;
   }

   if (!*pContentA && !*pContentB)
   {
      return TRUE;
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

   return (sizeContent == nSize ? TRUE : 
           *pContentA ? TRUE : *pContentB == 0);
}

// Determines whether content matches after case folding, up to a specified 
// number of bytes.  Returns TRUE if the given ranges begin and end at byte 
// values consistent with valid code point boundaries and if there is a case-
// insensitive match.  Returns FALSE otherwise.  PERFORMS NO FURTHER POINTER 
// VALIDATION AND NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
int SizeCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            size_t        sizeContent)     // Content size (bytes)
{
   size_t  nSize;                     // Accumulate size of content
   uint8_t *pPrevCodePointInContentA;

   // Got any empty input?
   if (!pContentA || !pContentB)
   {
      return FALSE;
   }

   if (!*pContentA && !*pContentB)
   {
      return TRUE;
   }

   // Verify that the beginning of the range isn't flagged as an intra- 
   // code-point byte.
   if (*pContentA > HALF_ASCII_LIMIT && *pContentA <= SINGLETON_LIMIT)
   {
      return FALSE;
   }

   if (*pContentB > HALF_ASCII_LIMIT && *pContentB <= SINGLETON_LIMIT)
   {
      return FALSE;
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

   return (sizeContent == nSize ? TRUE : 
           *pContentA ? TRUE : *pContentB == 0);
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- returns a pointer 
// to any first matching sequence within the larger content.  Returns NULLPTR 
// and sets *ppLast to NULLPTR if no match is found.
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
         }

         if (!CodePointCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (TRUE);

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
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- and a certain 
// number of code points in each, returns a pointer to any first matching 
// sequence within the larger content.  Returns NULLPTR and sets *ppLast to 
// NULLPTR if no match is found.
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
         }

         if (!CodePointCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (TRUE);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- returns a pointer 
// to any first matching sequence, within the larger content, after case 
// folding.  Returns NULLPTR and sets *ppLast to NULLPTR if no match is found.
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
         }

         if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (TRUE);

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
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
}

// Given a pointer to UTF-8 content and a pointer to a prospectively matching 
// portion of content -- i.e., what may be a substring -- and a certain 
// number of code points in each, returns a pointer to any first matching 
// sequence, within the larger content, after case folding.  Returns NULLPTR 
// and sets *ppLast to NULLPTR if no match is found.
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
         }

         if (!CodePointCaseCompareUtf8(pContent, pSearchContent))
         {
            break;
         }
      } while (TRUE);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- and a certain 
// number of characters in each, returns a pointer to any first matching 
// sequence within the larger string.  Returns NULLPTR and sets *ppLast to 
// NULLPTR if no match is found.
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
         }
      } while (*pszText == *pszSearchText);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pszSearchText = pszSliceInitial;
      pszText = pszTextSequence;
      iText = 1 + iTextSequence;
      ++pszText;
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- returns a 
// pointer to any first matching sequence, within the larger string, after 
// case folding.  Returns NULLPTR and sets *ppLast to NULLPTR if no match is 
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
}

// Given a pointer to an ASCII string and a pointer to a prospectively 
// matching portion of text -- i.e., what may be a substring -- and a certain 
// number of characters in each, returns a pointer to any first matching 
// sequence, within the larger string, after case folding.  Returns NULLPTR 
// and sets *ppLast to NULLPTR if no match is found.
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
         *ppLast = NULLPTR;
      }

      return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
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
               *ppLast = NULLPTR;
            }

            return NULLPTR;
         }
      } while (CM08[(unsigned char) *pszText] == 
                  CM08[(unsigned char) *pszSearchText]);

      // Continue from the character, within the larger string, after the 
      // last one checked in the outer loop.
      pszSearchText = pszSliceInitial;
      pszText = pszTextSequence;
      iText = 1 + iTextSequence;
      ++pszText;
   } while (TRUE);

   if (ppLast)
   {
      *ppLast = NULLPTR;
   }

   return NULLPTR;
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
      } while (TRUE);

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
   } while (TRUE);

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
      } while (TRUE);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (TRUE);

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
      } while (TRUE);

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
   } while (TRUE);
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
      } while (TRUE);

      // Continue from the code point, within the larger content, after the 
      // last one checked in the outer loop.
      pSearchContent = pSliceInitial;
      pContent = pContentSequence;
      iContent = 1 + iContentSequence;
      CodePointAdvanceUtf8(&pContent);
   } while (TRUE);

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
   } while (TRUE);

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
   } while (TRUE);

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
   } while (TRUE);

   return -1;
}

// Compares null-terminated UTF-8 content, matching wildcards.  Accepts '?' 
// as a single-code-point wildcard.  For each '*' wildcard, seeks out a 
// matching sequence of any code points beyond it.  Otherwise compares the 
// content a code point at a time.  PERFORMS NO UTF-8 VALIDATION.
//
int WildCompareUtf8(
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
                  return TRUE;     // "ab*" matches "ab".
               }
            }

             return FALSE;         // "abcd" doesn't match "abc".
         }
         else
         {
            return TRUE;           // "abc" matches "abc".
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
            return TRUE;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*bc" doesn't match "ab".
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
         return FALSE;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (TRUE);

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
            return TRUE;           // "ab*c*" matches "abcd".
         }

         if (!*pTame)
         {
            return FALSE;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*b*c" doesn't match "ab".
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
            return FALSE;          // "*bc" doesn't match "abcd".
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
               return FALSE;       // "*bcd" doesn't match "abc".
            }
         }

         pTame = pTameSequence;
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            return TRUE;           // "*bc" matches "abc".
         }
         else
         {
            return FALSE;          // "*bcd" doesn't match "abc".
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (TRUE);
}

// Compares UTF-8 content, up to a specified number of code points, matching 
// wildcards.  Accepts '?' as a single-code-point wildcard.  For each '*' 
// wildcard, seeks out a matching sequence of any code points beyond it.  
// Otherwise compares the content a code point at a time.  PERFORMS NO UTF-8 
// VALIDATION.
//
int WildLenCompareUtf8(
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
                  return TRUE;     // "ab*" matches "ab".
               }
            }

             return FALSE;         // "abcd" doesn't match "abc".
         }
         else
         {
            return TRUE;           // "abc" matches "abc".
         }
      }
      else if (lenWild <= iWild)
      {
         return FALSE;             // "abc" doesn't match "abcd".
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
            return TRUE;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*bc" doesn't match "ab".
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
         return FALSE;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
   } while (TRUE);

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
               return TRUE;        // "ab**c*" matches "abcd".
            }
         }

         if (lenWild <= iWild || !*pWild)
         {
            return TRUE;           // "ab*c*" matches "abcd".
         }

         if (lenTame <= iTame || !*pTame)
         {
            return FALSE;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*b*c" doesn't match "ab".
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
            return FALSE;          // "*bc" doesn't match "abcd".
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
               return FALSE;       // "*a*b" doesn't match "ac".
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
            return TRUE;           // "*bc" matches "abc".
         }

         return FALSE;             // "*bcd" doesn't match "abc".
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
      iTame++;
   } while (TRUE);
}

// Case folds and compares null-terminated UTF-8 content, matching wildcards. 
// Accepts '?' as a single-code-point wildcard.  For each '*' wildcard, seeks 
// out a matching sequence of any code points beyond it.  Otherwise compares 
// the content a code point at a time.  PERFORMS NO UTF-8 VALIDATION.
//
int WildCaseCompareUtf8(
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
                  return TRUE;     // "ab*" matches "ab".
               }
            }

            return FALSE;          // "abcd" doesn't match "abc".
         }
         else
         {
            return TRUE;           // "abc" matches "abc".
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
            return TRUE;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*bc" doesn't match "ab".
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
         return FALSE;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (TRUE);

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
            return TRUE;           // "ab*c*" matches "abcd".
         }

         if (!*pTame)
         {
            return FALSE;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*b*c" doesn't match "ab".
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
            return FALSE;          // "*bc" doesn't match "abcd".
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
               return FALSE;       // "*a*b" doesn't match "ac".
            }
         }

         pTame = pTameSequence;
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            return TRUE;           // "*bc" matches "abc".
         }
         else
         {
            return FALSE;          // "*bcd" doesn't match "abc".
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
   } while (TRUE);
}

// Case folds and compares UTF-8 content, up to a specified number of code 
// points, matching wildcards.  Accepts '?' as a single-code-point wildcard. 
// For each '*' wildcard, seeks out a matching sequence of any code points 
// beyond it.  Otherwise folds and compares the content a code point at a 
// time.  PERFORMS NO UTF-8 VALIDATION.
//
int WildLenCaseCompareUtf8(
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
                  return TRUE;     // "ab*" matches "ab".
               }
            }

             return FALSE;         // "abcd" doesn't match "abc".
         }
         else
         {
            return TRUE;           // "abc" matches "abc".
         }
      }
      else if (lenWild <= iWild)
      {
         return FALSE;             // "abc" doesn't match "abcd".
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
            return TRUE;           // "abc*" matches "abcd".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*bc" doesn't match "ab".
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
         return FALSE;             // "abc" doesn't match "abd".
      }

      // Everything's a match, so far.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
   } while (TRUE);

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
               return TRUE;        // "ab**c*" matches "abcd".
            }
         }

         if (lenWild <= iWild || !*pWild)
         {
            return TRUE;           // "ab*c*" matches "abcd".
         }

         if (lenTame <= iTame || !*pTame)
         {
            return FALSE;          // "*bcd*" doesn't match "abc".
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8(&pTame))
               {
                  return FALSE;    // "a*b*c" doesn't match "ab".
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
            return FALSE;          // "*bc" doesn't match "abcd".
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
               return FALSE;       // "*a*b" doesn't match "ac".
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
            return TRUE;           // "*bc" matches "abc".
         }

         return FALSE;             // "*bcd" doesn't match "abc".
      }

      // Everything's still a match.
      CodePointAdvanceUtf8(&pWild);
      CodePointAdvanceUtf8(&pTame);
      iWild++;
      iTame++;
   } while (TRUE);
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
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to NULLPTR and 
// returns NULLPTR.
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
         *ppLast = NULLPTR;
      }

      if (ppTarget)
      {
         *ppTarget = NULLPTR;
      }

      return NULLPTR;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = NULLPTR;

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

               if (*pSearchPattern)  // "?*?" doesn't find "a".
               {
                   pTame = pTameMatch = pWildMatch = NULLPTR;
               }
               else                // "*?" finds "a".
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
               pWildSequence = NULLPTR;
            }
         }
         else if (!*pSearchPattern)
         {
            if (ppTarget)          // "*" finds "a".
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
      } while (TRUE);

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
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      } while (TRUE);

      pTameSequence = pWildMatch = pTameMatch = NULLPTR;
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
                     if (ppLast)   // "ab*" finds "...ab".
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
      
               if (ppLast)         // "*abcd" doesn't find "...abc".
               {
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }

               *ppFirst = NULLPTR;
               return NULLPTR;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "*abc" finds "...abc".
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
                    *ppTarget = NULLPTR;
                }

                *ppFirst = NULLPTR;
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
               if (ppTarget)       // "ab*" finds "...abc".
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
                     if (pTameMatch)   // "a*bc" doesn't find "...ab".
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
            if (pTameMatch)            // "*abc" doesn't find "...abd".
            {
               pTame = pTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            goto seek_pattern;
         }
         else if (!pTameMatch)
         {
            pTameMatch = pTame;        // Got a prospective match.
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
      } while (TRUE);
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
            if (ppTarget)          // "a*c*" finds "...abcd".
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
            if (ppLast)            // "*bcd*" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't find "...ab".
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
            if (ppLast)            // "*bc" doesn't find "...abcd".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
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

                     if (ppLast)   // "a*?" finds "...abcd".
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

                  if (ppLast)      // "a*?d" finds "...abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" finds "...abcd".
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
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
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
                  if (ppLast)      // "*a*b" doesn't find "...ac".
                  {
                     *ppLast = NULLPTR;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = NULLPTR;
                  }

                  *ppFirst = NULLPTR;
                  return NULLPTR;
               }
            }

            pTame = pTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't find "...ab".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            if (ppLast)            // "*bc" finds "...abc".
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
            if (ppLast)            // "*bcd" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
   } while (TRUE);
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
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to NULLPTR and 
// returns NULLPTR.
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
         *ppLast = NULLPTR;
      }

      if (ppTarget)
      {
         *ppTarget = NULLPTR;
      }

      return NULLPTR;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = NULLPTR;

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

               if (*pSearchPattern)  // "?*?" doesn't find "a".
               {
                   pTame = pTameMatch = pWildMatch = NULLPTR;
               }
               else                // "*?" finds "a".
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
               pWildSequence = NULLPTR;
               iWildSequence = 0;
            }
         }
         else if (lenPattern <= iSearchPattern || !*pSearchPattern)
         {
            if (ppTarget)          // "*" finds "a".
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
      } while (TRUE);

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
                *ppLast = NULLPTR;
             }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      } while (TRUE);

      pTameSequence = pWildMatch = pTameMatch = NULLPTR;
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
                     if (ppLast)   // "ab*" finds "...ab".
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

               if (ppLast)         // "*abcd" doesn't find "...abc".
               {
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }

               *ppFirst = NULLPTR;
               return NULLPTR;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "*abc" finds "...abc".
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
                    *ppTarget = NULLPTR;
                }

                *ppFirst = NULLPTR;
                return pWildMatch;
            }
         }
         else if (lenPattern <= iWild)
         {
            if (pTameMatch)        // "a*c" doesn't find "...abcd".
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
               if (ppTarget)       // "ab*" finds "...abc".
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
                     if (pTameMatch)  // "a*bc" doesn't find "...ab".
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
            if (pTameMatch)        // "*abc" doesn't find "...abd".
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
            pTameMatch = pTame;    // Got a prospective match.
            iTameMatch = iTame;
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
         iWild++;
         iTame++;
      } while (TRUE);
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
               if (ppLast)         // "a*c*" finds "...abcd".
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
            if (ppTarget)          // "a*c" doesn't find "...abcd".
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
            if (ppLast)            // "*bcd*" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }
                  
            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR; 
            return NULLPTR;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't find "...ab".
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
            if (ppLast)            // "*bc" doesn't find "...abcd".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR; 
            return NULLPTR;
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

                     if (ppLast)   // "a*?" finds "...abcd".
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

                  if (ppLast)      // "a*?d" finds "...abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" finds "...abcd".
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
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
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
                  if (ppLast)      // "*a*b" doesn't find "...ac".
                  {
                     *ppLast = NULLPTR;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = NULLPTR;
                  }

                  *ppFirst = NULLPTR; 
                  return NULLPTR;
               }
            }

            pTame = pTameSequence;
            iTame = iTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't find "...ab".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      }

      // Another check for the end, at the end.
      if (lenContent <= iTame || !*pTame)
      {
         if (lenPattern <= iWild || !*pWild)
         {
            if (ppLast)            // "*bc" finds "...abc".
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
            if (ppLast)            // "*bcd" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }
                  
            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR; 
            return NULLPTR;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
      iWild++;
      iTame++;
   } while (TRUE);
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
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to NULLPTR and 
// returns NULLPTR.
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
         *ppLast = NULLPTR;
      }

      if (ppTarget)
      {
         *ppTarget = NULLPTR;
      }

      return NULLPTR;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = NULLPTR;

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

               if (*pSearchPattern)  // "?*?" doesn't find "a".
               {
                   pTame = pTameMatch = pWildMatch = NULLPTR;
               }
               else                // "*?" finds "a".
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
               pWildSequence = NULLPTR;
            }
         }
         else if (!*pSearchPattern)
         {
            if (ppTarget)          // "*" finds "a".
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
      } while (TRUE);

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
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      } while (TRUE);

      pTameSequence = pWildMatch = pTameMatch = NULLPTR;
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
                     if (ppLast)   // "ab*" finds "...ab".
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
      
               if (ppLast)         // "*abcd" doesn't find "...abc".
               {
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }

               *ppFirst = NULLPTR;
               return NULLPTR;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "*abc" finds "...abc".
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
                    *ppTarget = NULLPTR;
                }

                *ppFirst = NULLPTR;
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
               if (ppTarget)       // "ab*" finds "...abc".
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
                     if (pTameMatch)   // "a*bc" doesn't find "...ab".
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
            if (pTameMatch)            // "*abc" doesn't find "...abd".
            {
               pTame = pTameMatch;
            }

            CodePointAdvanceUtf8((const uint8_t **) &pTame);
            goto seek_pattern;
         }
         else if (!pTameMatch)
         {
            pTameMatch = pTame;        // Got a prospective match.
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
      } while (TRUE);
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
            if (ppTarget)          // "a*c*" finds "...abcd".
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
            if (ppLast)            // "*bcd*" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't find "...ab".
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
            if (ppLast)            // "*bc" doesn't find "...abcd"..
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
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

                     if (ppLast)   // "a*?" finds "...abcd".
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

                  if (ppLast)      // "a*?d" finds "...abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" finds "...abcd".
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
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
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
                  if (ppLast)      // "*a*b" doesn't find "...ac".
                  {
                     *ppLast = NULLPTR;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = NULLPTR;
                  }

                  *ppFirst = NULLPTR;
                  return NULLPTR;
               }
            }

            pTame = pTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't find "...ab".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      }

      // Another check for the end, at the end.
      if (!*pTame)
      {
         if (!*pWild)
         {
            if (ppLast)            // "*bc" finds "...abc".
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
            if (ppLast)            // "*bcd" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
   } while (TRUE);
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
// wildcard is found, sets *ppFirst, *ppLast, and *ppTarget to NULLPTR and 
// returns NULLPTR.
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
         *ppLast = NULLPTR;
      }

      if (ppTarget)
      {
         *ppTarget = NULLPTR;
      }

      return NULLPTR;
   }

   if (*pSearchPattern == '*' || *pSearchPattern == '?')
   {
      // The search pattern begins with a wildcard.  Set up for the fourth 
	  // "do" loop and skip on down there.
      iQCount = 0;
      pTameMatch = pWildMatch = pTameSequence = NULLPTR;

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

               if (*pSearchPattern)  // "?*?" doesn't find "a".
               {
                   pTame = pTameMatch = pWildMatch = NULLPTR;
               }
               else                // "*?" finds "a".
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
               pWildSequence = NULLPTR;
               iWildSequence = 0;
            }
         }
         else if (lenPattern <= iSearchPattern || !*pSearchPattern)
         {
            if (ppTarget)          // "*" finds "a".
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
      } while (TRUE);

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
                *ppLast = NULLPTR;
             }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      } while (TRUE);

      pTameSequence = pWildMatch = pTameMatch = NULLPTR;
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
                     if (ppLast)   // "ab*" finds "...ab".
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

               if (ppLast)         // "*abcd" doesn't find "...abc".
               {
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }

               *ppFirst = NULLPTR;
               return NULLPTR;
            }
            else if (pWildMatch)
            {
               if (ppLast)         // "*abc" finds "...abc".
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
                    *ppTarget = NULLPTR;
                }

                *ppFirst = NULLPTR;
                return pWildMatch;
            }
         }
         else if (lenPattern <= iWild)
         {
            if (pTameMatch)        // "ab*" finds "...abc".
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
               if (ppTarget)       // "ab*" finds "...abc".
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
                     if (pTameMatch)  // "a*bc" doesn't find "...ab".
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
            if (pTameMatch)        // "*abc" doesn't find "...abd".
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
            pTameMatch = pTame;    // Got a prospective match.
            iTameMatch = iTame;
         }

         // Everything's a match, so far.
         CodePointAdvanceUtf8((const uint8_t **) &pWild);
         CodePointAdvanceUtf8((const uint8_t **) &pTame);
         iWild++;
         iTame++;
      } while (TRUE);
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
               if (ppLast)         // "a*c*" finds "...abcd".
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
            if (ppTarget)          // "a*c*" finds "...abcd".
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
            if (ppLast)            // "*bcd*" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }
                  
            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR; 
            return NULLPTR;
         }

         // Search for the next prospective match.
         if (*pWild != '?')
         {
            while (!CodePointCaseCompareUtf8(pWild, pTame))
            {
               if (!CodePointAdvanceUtf8((const uint8_t **) &pTame))
               {
                  if (pTameMatch)      // "a*b*c" doesn't find "...ab".
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
            if (ppLast)            // "*bc" doesn't find "...abcd".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR; 
            return NULLPTR;
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

                     if (ppLast)   // "a*?" finds "...abcd".
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

                  if (ppLast)      // "a*?d" finds "...abcd".
                  {
                     *ppLast = pTameSequence;
                  }
               }
               else
               {
                  if (ppLast)      // "a*c*d" finds "...abcd".
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
                  *ppLast = NULLPTR;
               }

               if (ppTarget)
               {
                  *ppTarget = NULLPTR;
               }
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
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
                  if (ppLast)      // "*a*b" doesn't find "...ac".
                  {
                     *ppLast = NULLPTR;
                  }

                  if (ppTarget)
                  {
                     *ppTarget = NULLPTR;
                  }

                  *ppFirst = NULLPTR; 
                  return NULLPTR;
               }
            }

            pTame = pTameSequence;
            iTame = iTameSequence;
         }
		 else
         {
            if (ppLast)            // "?a*" doesn't find "...ab".
            {
               *ppLast = NULLPTR;
            }

            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR;
            return NULLPTR;
         }
      }

      // Another check for the end, at the end.
      if (lenContent <= iTame || !*pTame)
      {
         if (lenPattern <= iWild || !*pWild)
         {
            if (ppLast)            // "*bc" finds "...abc".
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
            if (ppLast)            // "*bcd" doesn't find "...abc".
            {
               *ppLast = NULLPTR;
            }
                  
            if (ppTarget)
            {
               *ppTarget = NULLPTR;
            }

            *ppFirst = NULLPTR; 
            return NULLPTR;
         }
      }

      // Everything's still a match.
      CodePointAdvanceUtf8((const uint8_t **) &pWild);
      CodePointAdvanceUtf8((const uint8_t **) &pTame);
      iWild++;
      iTame++;
   } while (TRUE);
}

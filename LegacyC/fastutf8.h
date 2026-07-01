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
#if !defined(FASTUTF8_H)
#define FASTUTF8_H

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

#include <stddef.h>

  // Counts the number of contiguous valid code points in the given null-
  // terminated content, starting from the beginning of the content.  Returns 
  // true if every code point prior to the terminating null is valid.  Returns 
  // false otherwise.
  //
  int ValidateUtf8(
            const uint8_t *pContent,       // Content to validate
            int           *piCount);       // Returned code point count

  // Validates the given content, up to the specified number of code points, 
  // starting from the beginning of the content.  Returns true if as many code 
  // point are valid.  Returns false otherwise.
  //
  int LenValidateUtf8(
            const uint8_t *pContent,       // Content to validate
            int           lenContent);     // Code point count (specified)

  // Validates the given content, up to a terminating null, starting from the 
  // beginning of the content.  Counts the number of contiguous valid code 
  // points.  Sets the bIs7BitCharString flag if every code point represents 
  // a 7-bit ASCII character.  Returns the number of bytes in the content, if 
  // the code points are valid.  Returns zero otherwise.  This function and 
  // the one that follows it are useful for constructing an object of a class 
  // that can handle ASCII character strings optimally and that also can 
  // handle UTF-8 content.
  //
  size_t ValidateWithIs7BitUtf8(
            const uint8_t *pContent,       // Content to validate
            int  *piCount,                 // Returned code point count
            int *pbIs7BitCharString);     // Returned 7-bit ASCII flag

  // Validates the given content, up to the specified number of code points, 
  // starting from the beginning of the content.  Returns the number of bytes 
  // in the content, if the code points are valid.  Returns zero otherwise.  
  // Sets the bIs7BitCharString flag if every code point represents a 7-bit 
  // ASCII character.
  //
  size_t LenValidateWithIs7BitUtf8(
            const uint8_t *pContent,       // Content to validate
            int  lenContent,               // Code point count (specified)
            int *pbIs7BitCharString);     // Returned 7-bit ASCII flag

  // Given 8-bit ASCII content, allocates a buffer for UTF-8 content and 
  // places the equivalent UTF-8 content in it.  If if FREE_INVALID_CONTENT is 
  // set, deallocates the block containing the 8-bit ASCII content.
  //
  // The developer is responsible for ensuring that the allocated buffer for 
  // UTF-8 content is deallocated via free(), once it is no longer in use.
  //
  uint8_t * Convert8BitAsciiToUtf8(
            const char *pContent,          // Content to convert
            int        *lenContent);       // Returned length (in code points)

  // Given an 8-bit ASCII string and its size in bytes, allocates a buffer 
  // sufficient for the equivalent UTF-8 content and places that content in 
  // it.  If FREE_INVALID_CONTENT is set, deallocates the block containing the 
  // 8-bit ASCII string.  Returns a pointer to the new buffer, or nullptr if 
  // the content comprises only 7-bit ASCII characters.
  //
  // The developer is responsible for ensuring that the allocated buffer for 
  // UTF-8 content is deallocated via free(), once it is no longer in use.
  //
  uint8_t * LenConvert8BitAsciiToUtf8(
            const char   *pContent,        // Content to convert
            size_t       sizeContent);     // Content size (bytes)

  // Initializes sets of mappings for case folding.  The mappings are used by 
  // the other functions here for case-insensitive string matching.
  //
  void CaseMappingSetupUtf8(
            void);

  // Given a buffer containing null-terminated UTF-8 content, anticipates the 
  // size of its folded equivalent, in bytes.  Anticipates 4 bytes for the 
  // noncharacter 0xFFFFFFFF in place of any code-point-sized content that is 
  // not valid UTF-8.  DOES NOT CHECK FOR INVALID POINTERS. 
  //
  size_t SizeOfFoldedUtf8(
            const uint8_t *pContent);      // Content to evaluate

  // Given a buffer containing UTF-8 content and a number of code points in 
  // the content, anticipates the size of the content's folded equivalent, in 
  // bytes.  Anticipates 4 bytes for the noncharacter 0xFFFFFFFF in place of 
  // any code-point-sized content that is not valid UTF-8.  DOES NOT CHECK FOR 
  // INVALID POINTERS. 
  //
  size_t SizeOfFoldedLenUtf8(
            const uint8_t *pContent,       // Content to evaluate
            int           lenContent);     // Code point count

  // Given a source buffer containing UTF-8 content, places its folded 
  // equivalent into the given destination buffer, up to the specified number 
  // of bytes.  Places the noncharacter 0xFFFFFFFF into the destination buffer 
  // in place of any code-point-sized source content that is not valid UTF-8.  
  // For Latin, Greek, and most other symbol sets that embody the uppercase 
  // and lowercase concept, acts as an iterative tolower() function for UTF-8. 
  // DOES NOT CHECK FOR BUFFER OVERFLOW, BUFFER OVERLAP, OR INVALID POINTERS. 
  //
  // Note: The folded content may occupy fewer or more bytes than the original 
  //       content.  A sufficient destination buffer can be allocated based on 
  //       an advance call to SizeOfFoldedUtf8() or to SizeOfFoldedLenUtf8().
  //
  uint8_t * ToFoldedUtf8(
            uint8_t *pDestination,         // Outbound buffer
            const uint8_t *pSource,        // Inbound buffer
            size_t  sizeDestination);      // Size of outbound buffer (bytes)

  //
  //    THE ROUTINES BELOW THIS COMMENT PERFORM NO UTF-8 VALIDATION 
  //    OTHER THAN NULL CHECKING.
  //

  // Given null-terminated UTF-8 content, returns the number of code points 
  // in it.
  //
  int CodePointCountUtf8(
            const uint8_t *pContent);      // Content to evaluate

  // Given null-terminated UTF-8 content, returns the number of bytes in it.
  //
  size_t SizeOfUtf8(
            const uint8_t *pContent);      // Content to evaluate

  // Given UTF-8 content and a count of the code points in it, returns the 
  // number of bytes in it.
  //
  size_t SizeOfLenUtf8(
            const uint8_t *pContent,       // Content to evaluate
            int     lenContent);           // Code point count

  // Given a byte range comprising UTF-8 content, returns the number of code 
  // points in the range.  Returns -1 if the range does not begin or end at 
  // byte values consistent with valid code point boundaries.  PERFORMS NO 
  // OTHER POINTER VALIDATION.  
  //
  int LenSizeOfUtf8(
            const uint8_t *pContent,       // Content to evaluate
            size_t  sizeContent);          // Size (bytes)

  // Given null-terminated UTF-8 content, determines whether it comprises 
  // entirely 7-bit "half ASCII" characters, which would make it compatible 
  // with ordinary C/C++ string routines.  Returns true for a 7-bit ASCII 
  // string, and false otherwise.
  //
  int Is7BitUtf8(
            uint8_t *pContent);            // Content to evaluate

  // Given UTF-8 content and its length in code points, determines whether it 
  // comprises entirely 7-bit "half ASCII" characters.  Returns true for a 
  // 7-bit ASCII string, and false otherwise.
  //
  int IsLen7BitUtf8(
            uint8_t *pContent,             // Content to evaluate
            int     lenContent);           // Code point count

  // Copies UTF-8 (or any) null-terminated content to the given destination 
  // buffer from the given source buffer.  DOES NOT CHECK FOR BUFFER OVERFLOW, 
  // BUFFER OVERLAP, OR INVALID POINTERS.
  //
  uint8_t * CopyUtf8(
            uint8_t *pDestination,         // Buffer
            const uint8_t *pSource);       // Content to copy

  // Copies UTF-8 content to the given destination buffer from the given 
  // source buffer, up to the specified number of code points.  DOES NOT CHECK 
  // FOR BUFFER OVERFLOW, BUFFER OVERLAP, OR INVALID POINTERS.
  //
  uint8_t * LenCopyUtf8(
            uint8_t *pDestination,         // Buffer
            const uint8_t *pSource,        // Content to copy
            int     lenContent);           // Code point count

  // Allocates a buffer and copies UTF-8 content to it from the given 
  // null-terminated source buffer.  DOES NOT CHECK FOR AN INVALID SOURCE 
  // BUFFER POINTER.
  //
  // The developer is responsible for ensuring that the allocated buffer is 
  // deallocated via free(), once it is no longer in use.
  //
  uint8_t * DuplicateUtf8(
            const uint8_t *pSource);       // Content to copy

  // Allocates a buffer and copies UTF-8 content to it from the given source 
  // buffer, up to the specified number of code points.  DOES NOT CHECK FOR AN 
  // INVALID SOURCE BUFFER POINTER.
  //
  // The developer is responsible for ensuring that the allocated buffer is 
  // deallocated via free(), once it is no longer in use.
  //
  uint8_t * LenDuplicateUtf8(
            const uint8_t *pSource,        // Content to copy
            int           lenContent);     // Code point count

  // Given a buffer partially initialized with null-terminated UTF-8 content, 
  // copies additional null-terminated UTF-8 content to it, beginning by 
  // overwriting the original content's terminating null and continuing to the 
  // additional content's terminating null.  Returns true if buffer size,  
  // specified in bytes, is sufficient to accommodate the total content, and 
  // false otherwise.
  //
  uint8_t * ConcatenateUtf8(
            uint8_t       *pContent,             // Original content
            size_t        sizeContentBuffer,     // Whole buffer size
            const uint8_t *pAdditionalContent);  // Content to add

  // Given a buffer partially initialized with UTF-8 content comprising a 
  // specified number of code points, copies additional UTF-8 content to it, 
  // beginning after that given length and continuing to the length of the 
  // additional content, also given as a specified number of code points.  
  // Returns true if the buffer size, specified in bytes, is sufficient to 
  // accommodate the total content, and false otherwise.
  //
  uint8_t * LenConcatenateUtf8(
            uint8_t       *pContent,             // Original content
            size_t        sizeContentBuffer,     // Whole buffer size
            const uint8_t *pAdditionalContent,   // Content to add
            int           lenContent,            // Original code point count
            int           lenAdditionalContent); // Added code point count

  // Given a pointer to UTF-8 content and a pointer to one or more delimiter 
  // code points, searches the content for the first occurrence of any 
  // delimiter.  Replaces that code point in the content with a null 
  // terminator, including enough nulls to replace the entire code point.  
  // Returns a pointer to any first delimited content, or nullptr if there is 
  // no content.
  //
  uint8_t * SeparateUtf8(
            uint8_t **ppContent,           // Pointer to content to modify
            const uint8_t *pTokenSet);     // Delimiter(s)

  // Given a pointer to an ASCII string and a pointer to one or more delimiter 
  // characters, searches the string for the first occurrence of a delimiter. 
  // Replaces that character in the string with a null terminator.  Returns a 
  // pointer to any first delimited portion of the string, or nullptr if the 
  // string is empty.  DOES NOT HANDLE UTF-8.
  //
  char * SeparateAscii(
            char **ppszText,               // Pointer to string to modify
            const char *pszTokenSet);      // Delimiter(s)

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
            const uint8_t *pTokenSet);     // Delimiter(s)

  // Given a pointer to null-terminated UTF-8 content and a pointer to a null- 
  // terminated set of one or more delimiter code points, searches the content 
  // for the first occurrence of any delimiter.  Bypasses any initial delimiters 
  // at the content's start.  In case a delimiter is found within the subsequent 
  // content, returns a pointer to the code point immediately prior to it.  
  // Returns nullptr if no delimiter is found.  PERFORMS NO UTF-8 VALIDATION 
  // OTHER THAN NULL CHECKING.
  //
  uint8_t * TokenLenFindUtf8(
            const uint8_t *pContent,       // Content to search
            const uint8_t *pTokenSet,      // Delimiter(s)
            int           lenContent,      // Count of code points in content
            int           lenTokenSet);    // Code points in token set

  // Returns the UTF-8 code point at the given index within the content.
  // THE PERFORMANCE IS TERRIBLE, RELATIVE TO ASCII STRING INDEXING.
  //
  uint32_t IndexUtf8(
            uint8_t *pContent,             // Content to find
            int     iIndex);               // Index at which to find it

  // Removes leading and trailing spaces from null-terminated UTF-8 content, 
  // modifying the content in place.  Returns a pointer to the beginning of 
  // the content.  In case the content occupies a heap memory block, in order 
  // to deallocate that block, the caller will need to retain the original 
  // pointer to it.
  //
  uint8_t * TrimUtf8(
            uint8_t *pContent);            // Content to trim

  // Removes leading and trailing spaces from a null-terminated ASCII string, 
  // modifying the string in place.  Returns a pointer to the beginning of the 
  // string.  In case the string occupies a heap memory block, in order to 
  // deallocate that block, the caller will need to retain the original pointer 
  // to it.
  //
  char * TrimAscii(
            char *pszText);                // String to trim

  // Returns a buffer containing the UTF-8 code points beginning at the given 
  // first index within the null-terminated content and ending at the last 
  // index.  If the last index value is less than the first index value, 
  // creates and returns an empty buffer.  If the indices are negative, 
  // indexing is based on the end of the content; i.e. counts backward from 
  // the end of the content to get the code points beginning at the first 
  // index relative to the end, and ending at the code point prior to the last 
  // index relative to the end.  Unlike the JavaScript slice() method, a 
  // negative first index (iFirst) value and zero last index (iLast) value 
  // returns the last portion of the content, beginning -(iFirst) code points 
  // from its end.
  //
  // The developer is responsible for ensuring that the allocated buffer for 
  // UTF-8 content is deallocated via free(), once it is no longer in use.
  //
  // THE PERFORMANCE IS TERRIBLE, RELATIVE TO ASCII SUBSTRING FUNCTIONALITY.
  //
  uint8_t * SliceUtf8(
            const uint8_t *pContent,       // Content to slice
            int           iFirst,          // Index at which slice begins
            int           iLast);          // Index at which slice ends

  // Similar to the above function, but for ASCII text, and much faster.
  //
  // The developer is responsible for ensuring that the allocated buffer for 
  // ASCII text is deallocated via free(), once it is no longer in use.
  //
  char * SliceAscii(
            const char *pContent,          // String to slice
            int        iFirst,             // Index at which slice begins
            int        iLast);             // Index at which slice ends

  // Returns a buffer containing the UTF-8 code points beginning at the given 
  // first index within the content and ending at the last index.  If the last 
  // index value is less than the first index value, creates and returns an 
  // empty string.  If the indices are negative, indexing is done based on the 
  // end of the content; i.e. counts backward from the end of the content to 
  // get the code points beginning at the first index relative to the end, and 
  // ending at the code point prior to the last index relative to the end.  
  // The number of code points in the content is specified via the fourth 
  // parameter.
  //
  // The developer is responsible for ensuring that the allocated buffer for 
  // UTF-8 content is deallocated via free(), once it is no longer in use.
  //
  // THE PERFORMANCE IS TERRIBLE, RELATIVE TO ASCII SUBSTRING FUNCTIONALITY.
  //
  uint8_t * LenSliceUtf8(
            const uint8_t *pContent,       // Content to slice
            int           iFirst,          // Index at which slice begins
            int           iLast,           // Index at which slice ends
            int           lenContent);     // Code point count (whole content)

  // Similar to the above function, but for ASCII text, and much faster.
  //
  // The developer is responsible for ensuring that the allocated buffer for 
  // ASCII text is deallocated via free(), once it is no longer in use.
  //
  char * LenSliceAscii(
            const char *pContent,          // String to slice
            int        iFirst,             // Index at which slice begins
            int        iLast,              // Index at which slice ends
            int        lenContent);        // Whole string length

  // Determines whether null-terminated UTF-8 content matches entirely.
  // Returns true for matching content, and false otherwise.
  //
  // Some ASCII string comparison functions can return values that indicate 
  // whether one string might be considered numerically "less than" another.  
  // Though that may be useful for certain sorting arrangements, UTF-8 content 
  // sorting might best be coded specifically for one locale or another.
  //
  int CompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB);     // ...with other content

  // Determines whether null-terminated UTF-8 content matches, entirely, after 
  // case folding.  Returns true for matching content, and false otherwise.
  //
  int CaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB);     // ...with other content

  // Determines whether UTF-8 content matches, up to a given number of code 
  // points or any terminating null.  Returns true for matching content, and 
  // false otherwise.
  //
  int LenCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent);     // Code point count

  // Determines whether UTF-8 content matches, up to a given number of code 
  // points or any terminating null, after case folding.  Returns true for 
  // matching content, and false otherwise.
  //
  int LenCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent);     // Code point count

  // Determines whether content matches, up to a specified number of bytes or 
  // any terminating null.  Returns true for matching content, and false 
  // otherwise.
  //
  int SizeCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            size_t        sizeContent);    // Content size (bytes)

  // Given a pair of byte ranges comprising UTF-8 content, determines whether 
  // the content matches after case folding.  Returns true if the ranges begin 
  // and end at byte values consistent with valid code point boundaries and if 
  // there is a case-insensitive match.  Returns false otherwise.  PERFORMS NO 
  // FURTHER POINTER VALIDATION AND NO UTF-8 VALIDATION OTHER THAN NULL 
  // CHECKING.
  //
  int SizeCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            size_t        sizeContent);    // Content size (bytes)

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- returns 
  // a pointer to any first matching sequence within the larger content. 
  // Returns nullptr if no match is found.
  //
  uint8_t * FindUtf8(
            const uint8_t *pContent,       // Haystack
            const uint8_t *pSearchContent, // Needle
            uint8_t       **ppLast);       // Returned loc where match ends

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- and a 
  // certain number of code points in each, returns a pointer to any first 
  // matching sequence within the larger content.  Returns nullptr if no 
  // match is found.
  //
  uint8_t * LenFindUtf8(
            const uint8_t *pContent,       // Haystack
            const uint8_t *pSearchContent, // Needle 
            int           lenContent,      // Code point count (haystack)
            int           lenSlice,        // Code point count (needle)
            uint8_t       **ppLast);       // Returned loc where match ends

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- returns 
  // a pointer to any first matching sequence, within the larger content, 
  // after case folding.  Returns nullptr if no match is found.
  //
  uint8_t * CaseFindUtf8(
            const uint8_t *pContent,       // Haystack
            const uint8_t *pSearchContent, // Needle 
            uint8_t       **ppLast);       // Returned loc where match ends

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- and a 
  // certain number of code points in each, returns a pointer to any first 
  // matching sequence, within the larger content, after case folding. 
  // Returns nullptr if no match is found.
  //
  uint8_t * LenCaseFindUtf8(
            const uint8_t *pContent,       // Haystack
            const uint8_t *pSearchContent, // Needle 
            int           lenContent,      // Code point count (haystack)
            int           lenSlice,        // Code point count (needle)
            uint8_t       **ppLast);       // Returned loc where match ends

  // Given a pointer to an ASCII string and a pointer to a prospectively 
  // matching portion of text -- i.e., what may be a substring -- and a 
  // certain number of characters in each, returns a pointer to any first 
  // matching sequence within the larger string.  Returns nullptr if no 
  // match is found.
  //
  char * LenFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // Character count (haystack)
            int        lenSlice,           // Character count (needle)
            char       **ppLast);          // Returned loc where match ends

  // Given a pointer to an ASCII string and a pointer to a prospectively 
  // matching portion of text -- i.e., what may be a substring -- returns a 
  // pointer to any first matching sequence, within the larger string, after 
  // case folding.  Returns nullptr if no match is found.
  //
  char * CaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle
            char       **ppLast);          // Returned loc where match ends

  // Given a pointer to an ASCII string and a pointer to a prospectively 
  // matching portion of text -- i.e., what may be a substring -- and a 
  // certain number of characters in each, returns a pointer to any first 
  // matching sequence, within the larger string, after case folding. 
  // Returns nullptr if no match is found.
  //
  char * LenCaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // String length (haystack)
            int        lenSlice,           // String length (needle)
            char       **ppLast);          // Returned loc where match ends

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- returns 
  // an index of any first matching sequence within the larger content. 
  // Returns -1 if no match is found.
  //
  int IndexFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent); // Needle

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- and a 
  // certain number of code points in each, returns an index of to any first 
  // matching sequence within the larger content.  Returns -1 if no match is 
  // found.
  //
  int IndexLenFindUtf8(
            const uint8_t *pContent,       // Haystack
            const uint8_t *pSearchContent, // Needle 
            int           lenContent,      // Code point count (haystack)
            int           lenSlice);       // Code point count (needle)

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- returns 
  // an index of any first matching sequence, within the larger content, 
  // after case folding.  Returns -1 if no match is found.
  //
  int IndexCaseFindUtf8(
            const uint8_t *pContent,        // Haystack
            const uint8_t *pSearchContent); // Needle

  // Given a pointer to UTF-8 content and a pointer to a prospectively 
  // matching portion of content -- i.e., what may be a substring -- and a 
  // certain number of code points in each, returns an index of any first 
  // matching sequence, within the larger content, after case folding. 
  // Returns -1 if no match is found.
  //
  int IndexLenCaseFindUtf8(
            const uint8_t *pContent,       // Haystack
            const uint8_t *pSearchContent, // Needle 
            int           lenContent,      // Code point count (haystack)
            int           lenSlice);       // Code point count (needle)

  // Given a pointer to an ASCII string and a pointer to a prospectively 
  // matching portion of text -- i.e., what may be a substring -- and a 
  // certain number of characters in each, returns an index of any first 
  // matching sequence within the larger string.  Returns -1 if no match 
  // is found.
  //
  int IndexLenFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // Character count (haystack)
            int        lenSlice);          // Character count (needle)

  // Given a pointer to an ASCII string and a pointer to a prospectively 
  // matching portion of text -- i.e., what may be a substring -- returns an 
  // index of any first matching sequence, within the larger string, after 
  // case folding.  Returns -1 if no match is found.
  //
  int IndexCaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText);    // Needle

  // Given a pointer to an ASCII string and a pointer to a prospectively 
  // matching portion of text -- i.e., what may be a substring -- and a 
  // certain number of characters in each, returns an index of any first 
  // matching sequence, within the larger string, after case folding. 
  // Returns -1 if no match is found.
  //
  int IndexLenCaseFindAscii(
            const char *pszText,           // Haystack
            const char *pszSearchText,     // Needle 
            int        lenText,            // String length (haystack)
            int        lenSlice);          // String length (needle)

  // C/C++ implementation of FastWildCompare(), for null-terminated content 
  // comprising UTF-8 code points.
  //
  // Compares content.  Accepts '?' as a single-code-point wildcard.  For each 
  // '*' wildcard, seeks out a matching sequence of any code points beyond it.  
  // Otherwise compares the content a code point at a time. 
  //
  int WildCompareUtf8(
            const uint8_t *pWild,          // Content (may include wildcards)
            const uint8_t *pTame);         // Content to compare (no wildcards)

  // C/C++ implementation of FastWildCompare(), for content comprising UTF-8 
  // code points.
  //
  // Compares content up to a specified number of code points.  Accepts '?' 
  // as a single-code-point wildcard.  For each '*' wildcard, seeks out a 
  // matching sequence of any code points beyond it.  Otherwise compares the 
  // content a code point at a time. 
  //
  int WildLenCompareUtf8(
            const uint8_t *pWild,          // Content (may include wildcards)
            const uint8_t *pTame,          // Content to compare (no wildcards)
            int           lenWild,         // Count of code points in content
            int           lenTame);        // Code points in prospective match

  // C/C++ implementation of FastWildCompare(), for null-terminated content 
  // comprising UTF-8 code points.
  //
  // Case folds and compares content.  Accepts '?' as a single-code-point 
  // wildcard.  For each '*' wildcard, seeks out a matching sequence of any 
  // code points beyond it.  Otherwise folds and compares the content a code 
  // point at a time. 
  //
  int WildCaseCompareUtf8(
            const uint8_t *pWild,          // Content (may include wildcards)
            const uint8_t *pTame);         // Content to compare (no wildcards)

  // C/C++ implementation of FastWildCompare(), for content comprising UTF-8 
  // code points.
  //
  // Case folds and compares content up to a specified number of code points.  
  // Accepts '?' as a single-code-point wildcard.  For each '*' wildcard, 
  // seeks out a matching sequence of any code points beyond it.  Otherwise 
  // folds and compares the content a code point at a time. 
  //
  int WildLenCaseCompareUtf8(
            const uint8_t *pWild,          // Content (may include wildcards)
            const uint8_t *pTame,          // Content to compare (no wildcards)
            int           lenWild,         // Count of code points in content
            int           lenTame);        // Code points in prospective match

  // Targeted wildcard search.
  //
  // Given UTF-8 content, and given a UTF-8 search pattern that can include 
  // '*' and '?' wildcards, searches the content for a match.  If a match is 
  // found, sets *ppLast and *ppTarget as follows:
  //
  // *ppLast will point to the location within the content where the 
  //  match ends, and
  // *ppTarget will point to the location where the last matching portion of 
  // the content begins, i.e., the content corresponding to the portion of the 
  // search pattern after the last '*' wildcard.
  //
  // Returns a pointer to the beginning of the match, corresponding to the 
  // beginning of the search pattern.
  //
  uint8_t * WildFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget);     // Returned location after last '*'

  // Targeted wildcard search (length limited).
  // See comments for WildFindUtf8().  The caller provides length limits for 
  // the content and for the search pattern via the two integer parameters.
  //
  uint8_t * WildLenFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       int           lenContent,      // Count of code points in content
       int           lenPattern,      // Code points in search pattern
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget);     // Returned location after last '*'

  // Targeted wildcard search (case-insensitive).
  // See comments for WildFindUtf8().  The wildcarded content comparison is 
  // performed with case folding, for a case-insensitive match.
  //
  uint8_t * WildCaseFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget);     // Returned location after last '*'

  // Targeted wildcard search (length limited and case-insensitive).
  // See comments for WildFindUtf8().  The caller provides length limits for 
  // the content and for the search pattern via the two integer parameters.  
  // The comparison is performed with case folding, for a case-insensitive match.
  //
  uint8_t * WildLenCaseFindUtf8(
       uint8_t       **ppFirst,       // Updated location of content to search
       const uint8_t *pSearchPattern, // Specifier that may include wildcards
       int           lenContent,      // Count of code points in content
       int           lenPattern,      // Code points in search pattern
       uint8_t       **ppLast,        // Returned location where match ends
       uint8_t       **ppTarget);     // Returned location after last '*'

#endif  // FASTUTF8_H
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
#if !defined(FASTUTF8_H)
#define FASTUTF8_H

#if defined(__cplusplus)
#include <memory>
#include <cstdint>
#include <cstddef>

using std::uint8_t;
using std::uint32_t;
using std::size_t;

// The FastUtf8 namespace provides a simple, understandable C++ interface for 
// efficiently working with both internationalized / symbolic content and 
// ASCII text.  For C++ builds, the C-style *Utf8() functions are declared in 
// this header file along with the code in the namespace.  Following the C 
// function declarations, three C++ classes are declared:
//
//  A FastUtf8::Uniseries class instantiated per UTF-8 content sequence
//     A content sequence can contain up to 4GB of code points and may 
//     represent, e.g., an ASCII string, a Devanāgarī shirorekha, an emoji 
//     combo, a Hangul Mundan, an Urdu bayt, etc.
//  A FastUtf8::Initializer class instantiated once per session, and 
//  A FastUtf8::Uniseries::Iterator class for walking through content.
//
// The FastUtf8 functionality is based on the C code.  The C code, in order 
// to keep it compatible with legacy C projects, cannot be part of the 
// FastUtf8 namespace.  In the corresponding source file (fastutf8.cpp), as in 
// this header file, the C code is placed ahead of the C++ code.
//
extern "C"
{

#else   // !__cplusplus
#include <stdint.h>
#include <stddef.h>
#endif  // !__cplusplus

  // Counts the number of contiguous valid code points in the given null-
  // terminated content, starting from the beginning of the content.  Returns 
  // true if every code point prior to the terminating null is valid.  Returns 
  // false otherwise.
  //
  bool ValidateUtf8(
            const uint8_t *pContent,       // Content to validate
            int           *piCount);       // Returned code point count

  // Validates the given content, up to the specified number of code points, 
  // starting from the beginning of the content.  Returns true if as many code 
  // point are valid.  Returns false otherwise.
  //
  bool LenValidateUtf8(
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
            bool *pbIs7BitCharString);     // Returned 7-bit ASCII flag

  // Validates the given content, up to the specified number of code points, 
  // starting from the beginning of the content.  Returns the number of bytes 
  // in the content, if the code points are valid.  Returns zero otherwise.  
  // Sets the bIs7BitCharString flag if every code point represents a 7-bit 
  // ASCII character.
  //
  size_t LenValidateWithIs7BitUtf8(
            const uint8_t *pContent,       // Content to validate
            int  lenContent,               // Code point count (specified)
            bool *pbIs7BitCharString);     // Returned 7-bit ASCII flag

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
            uint8_t       *pDestination,     // Outbound buffer
            const uint8_t *pSource,          // Inbound buffer
            size_t         sizeDestination); // Size of outbound buffer (bytes)

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
            int           lenContent);     // Code point count

  // Given a byte range comprising UTF-8 content, returns the number of code 
  // points in the range.  Returns -1 if the range does not begin or end at 
  // byte values consistent with valid code point boundaries.  PERFORMS NO 
  // OTHER POINTER VALIDATION.  
  //
  int LenSizeOfUtf8(
            const uint8_t *pContent,       // Content to evaluate
            size_t        sizeContent);    // Size (bytes)

  // Given null-terminated UTF-8 content, determines whether it comprises 
  // entirely 7-bit "half ASCII" characters, which would make it compatible 
  // with ordinary C/C++ string routines.  Returns true for a 7-bit ASCII 
  // string, and false otherwise.
  //
  bool Is7BitUtf8(
            uint8_t *pContent);            // Content to evaluate

  // Given UTF-8 content and its length in code points, determines whether it 
  // comprises entirely 7-bit "half ASCII" characters.  Returns true for a 
  // 7-bit ASCII string, and false otherwise.
  //
  bool IsLen7BitUtf8(
            uint8_t *pContent,             // Content to evaluate
            int     lenContent);           // Code point count

  // Copies UTF-8 (or any) null-terminated content to the given destination 
  // buffer from the given source buffer.  DOES NOT CHECK FOR BUFFER OVERFLOW, 
  // BUFFER OVERLAP, OR INVALID POINTERS.
  //
  uint8_t * CopyUtf8(
            uint8_t       *pDestination,   // Buffer
            const uint8_t *pSource);       // Content to copy

  // Copies UTF-8 content to the given destination buffer from the given 
  // source buffer, up to the specified number of code points.  DOES NOT CHECK 
  // FOR BUFFER OVERFLOW, BUFFER OVERLAP, OR INVALID POINTERS.
  //
  uint8_t * LenCopyUtf8(
            uint8_t       *pDestination,   // Buffer
            const uint8_t *pSource,        // Content to copy
            int           lenContent);     // Code point count

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
            uint8_t       **ppContent,     // Pointer to content to modify
            const uint8_t *pTokenSet);     // Delimiter(s)

  // Given a pointer to an ASCII string and a pointer to one or more delimiter 
  // characters, searches the string for the first occurrence of a delimiter. 
  // Replaces that character in the string with a null terminator.  Returns a 
  // pointer to any first delimited portion of the string, or nullptr if the 
  // string is empty.  DOES NOT HANDLE UTF-8.
  //
  char * SeparateAscii(
            char       **ppszText,         // Pointer to string to modify
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
  bool CompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB);     // ...with other content

  // Determines whether null-terminated UTF-8 content matches, entirely, after 
  // case folding.  Returns true for matching content, and false otherwise.
  //
  bool CaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB);     // ...with other content

  // Determines whether UTF-8 content matches, up to a given number of code 
  // points or any terminating null.  Returns true for matching content, and 
  // false otherwise.
  //
  bool LenCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent);     // Code point count

  // Determines whether UTF-8 content matches, up to a given number of code 
  // points or any terminating null, after case folding.  Returns true for 
  // matching content, and false otherwise.
  //
  bool LenCaseCompareUtf8(
            const uint8_t *pContentA,      // Content to compare...
            const uint8_t *pContentB,      // ...with other content
            int           lenContent);     // Code point count

  // Determines whether content matches, up to a specified number of bytes or 
  // any terminating null.  Returns true for matching content, and false 
  // otherwise.
  //
  bool SizeCompareUtf8(
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
  bool SizeCaseCompareUtf8(
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
  bool WildCompareUtf8(
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
  bool WildLenCompareUtf8(
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
  bool WildCaseCompareUtf8(
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
  bool WildLenCaseCompareUtf8(
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
#if defined(__cplusplus)
}   // extern "C"


//  A FastUtf8::Uniseries class instantiated per UTF-8 content sequence
//     A content sequence can contain up to 4GB of code points and may 
//     represent, e.g., an ASCII string, a Devanāgarī shirorekha, an emoji 
//     combo, a Hangul Mundan, an Urdu bayt, etc.
//  A FastUtf8::Initializer class instantiated once per session, and 
//  A FastUtf8::Uniseries::Iterator class for walking through content
//
namespace FastUtf8
{
   // These flags affect the default behavior of Uniseries objects.  By 
   // default, these objects are constructed such that operator= will perform 
   // a case-sensitive comparison and so that every routine that walks through 
   // content will stop at a length limit rather than rely on finding a 
   // terminating null.
   inline bool bCaseInsensitive = false;
   inline bool bLengthLimited = true;

   // Changing these namespace-level flags does not affect the behavior of 
   // existing Uniseries objects.

   // The bCaseInsensitive flag can be set via either of the next methods.
   inline void setCaseSensitivity(bool bCase)
   {
      bCaseInsensitive = bCase;
   }

   inline void setCaseInsensitive(void)
   {
      bCaseInsensitive = true;
   }

   // Resets (switches off) default case insensitivity for new Uniseries 
   // objects.
   inline void clearCaseInsensitive(void)
   {
      bCaseInsensitive = false;
   }

   // The bLengthLimited flag is set by default at startup.
   inline void setLengthLimited(void)
   {
      bLengthLimited = true;
   }

   // Clearing the flag will typically benefit FastUtf8 performance.  But it's 
   // an optimization that may pose risks for Uniseries objects that store 
   // user-provided content.  For info about those risks, see...
   // https://en.wikipedia.org/wiki/Null-terminated_string#Limitations
   // ... and the related "Computer security" material on Wikipedia.
   inline void clearLengthLimited(void)
   {
      bLengthLimited = false;
   }

// FastUtf8::Initializer
//
// When an executable built from this code is loaded, the mappings used for 
// case folding reside in its Data segment and occupy over 4 megabytes.  
// The mappings get initialized one time per session.
//
// Because the FastUtf8::Uniseries methods rely on those case mappings, the 
// constructor for that class includes a line of code that ensures that 
// they've been set up, but just once, by constructing a static instance of 
// the FastUtf8::Initializer class.  That way, case mapping setup happens 
// as the FastUtf8 object is constructed on first use.
//
  class Initializer
  {
   // Disallow duplicates of this object.
public:
   Initializer(const Initializer&) = delete;
   Initializer operator=(const Initializer&) = delete;

   // The static object is wrapped in a function as a local static variable, 
   // so the constructor gets invoked just once.
   static FastUtf8::Initializer& setupCaseMappingOnce()
   {
      static FastUtf8::Initializer theInstance;
      return theInstance;
   }

   // Disallow direct instantiation.
private:
   Initializer()
   {
      CaseMappingSetupUtf8();  // Set up mappings for case folding.
   }
  };

// FastUtf8::Uniseries
//
// An object of this class stores the following data:
//
//  A content buffer
//  An item of metadata
//
// The metadata item comprises a small set of flags and a length field.  It 
// fits into a single address on a 64-bit system, for speedy access.  Other 
// than the slight complexity involving that optimization, the methods within 
// this class allow for easily comprehensible management of UTF-8 content and 
// enable these operations:
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
// functions CodePointAdvanceUtf8() and CodePointBacktrackUtf8(), found just 
// in fastutf8.cpp.  Individual code point comparison via operator==() 
// involves call to CodePointCompareUtf8(), also just in fastutf8.cpp.  All 
// other FastUtf8 functionality is performed by C-style functions either 
// declared in here (and also defined in this fastutf8.cpp) or available in 
// the standard C library.
//
  class Uniseries
  {
private:
   static constexpr uint64_t IS_7BIT_CHAR_STRING = 1ULL << 0x3F;
   static constexpr uint64_t IS_CASE_INSENSITIVE = 1ULL << 0x3E;
   static constexpr uint64_t IS_LENGTH_LIMITED = 1ULL << 0x3D;
   static constexpr uint64_t IS_DEALLOCATED_EXTERNALLY = 1ULL << 0x3C;
   static constexpr uint64_t FLAGS_MASK = IS_7BIT_CHAR_STRING | 
                                          IS_CASE_INSENSITIVE | 
                                          IS_LENGTH_LIMITED | 
                                          IS_DEALLOCATED_EXTERNALLY;
   static constexpr uint64_t LENGTH_MASK = ~(uint64_t) FLAGS_MASK;

   uint8_t                   *m_pContent;  // Comprises UTF-8 code points.
   uint64_t                  m_metadata;   // Comprises flags and length.

public:
   // This constructor validates the content in the inbound buffer and makes 
   // a deep copy by invoking C-style functions implemented in fastutf8.cpp.  
   // If the content includes any invalid code point(s), the constructor 
   // treats the entire buffer as 8-bit ASCII and converts it, as such, into 
   // valid UTF-8 content.
   //
   // The C-style memory management is compatible with the underlying C-style 
   // functions, which rely on the standard C library.
   //
   Uniseries(uint8_t *pInbound, bool bWrapBuffer = false);
   Uniseries(char *pInbound, bool bWrapBuffer = false);

   // This standard constructor creates an empty FastUtf8 object with a 
   // content buffer capacity specified in bytes.  The metadata has no flags 
   // set upon construction.
   Uniseries(size_t nBytes = 0);

   // This constructor creates a FastUtf8::Uniseries object from the content 
   // buffer and metadata of an existing one, with no validation, performing a 
   // deep copy like the above constructor.
   Uniseries(const Uniseries& that);

   // This constructor creates a FastUtf8::Uniseries object from a buffer 
   // designated by the pFirst and pLast pointers.
   Uniseries(const uint8_t *pFirst, const uint8_t *pLast);
   Uniseries(uint8_t *pFirst, uint8_t *pLast);

   // The destructor deallocates the content buffer.
   ~Uniseries(void);

   // This assignment operator replaces the content and metadata associated 
   // with an existing FastUtf8::Uniseries object by validating the content in 
   // the inbound buffer and making a deep copy.  If the content includes any 
   // invalid code point(s), the operator treats the entire buffer as 8-bit 
   // ASCII and converts it, as such, into valid UTF-8 content.  Usage may 
   // generate a g++ compiler warning about converting a string constant to an 
   // array type, though the inbound content is declared as const; the 
   // -Wno-write-strings option disables these warnings.
   Uniseries& operator=(const uint8_t *pInbound);
   Uniseries& operator=(const char *pInbound);

   // This assignment operator replaces the content and metadata associated 
   // with the current Uniseries object with the content buffer and metadata 
   // of another existing one, with no validation, performing a deep copy like 
   // the constructor.
   Uniseries& operator=(const Uniseries& that);
   Uniseries& operator=(const Uniseries *pThat);

   // This slice() method constructs a new Uniseries object from an existing 
   // one, making a deep copy of a portion of its content specified by the 
   // iFirst and iLast parameters.
   //
   // If the last index value is less than the first index value, the returned 
   // object will comprise an empty string.  If the indices are negative, 
   // indexing is done based on the end of the content; i.e., by counting 
   // backward from the end of the content to get the code points beginning at 
   // the first index relative to the end, and ending at the code point prior 
   // to the last index relative to the end.  A negative first index (iFirst) 
   // value and zero last index (iLast) value fetches the last portion of the 
   // content, beginning -(iFirst) code points from its end.
   Uniseries slice(const int iFirst, const int iLast = 0) const;

   // This slice() method constructs a new Uniseries object from an existing 
   // one, making a deep copy of a portion of its content specified by the 
   // pFirst and pLast parameters.  DOES NO POINTER VALIDATION.
   Uniseries slice(const uint8_t *pFirst, const uint8_t *pLast) const;

   // This slice() method constructs a new Uniseries object from a range of 
   // UTF-8 content.  DOES NO POINTER VALIDATION OTHER THAN NULL CHECKING.
   Uniseries& fromSlice(const uint8_t *pFirst, const uint8_t *pLast);
   Uniseries& fromSlice(const char *pFirst, const char *pLast);

   // The concatenation operators reallocate the content buffer and perform a 
   // deep copy of the additional content.  The metadata is left as close as 
   // possible to the original metadata without falsifying it.  Any length 
   // limit is adjusted to accommodate the added content.
   Uniseries& operator+=(uint8_t *pInbound);
   Uniseries& operator+=(char *pInbound);
   Uniseries& operator+=(const Uniseries& that);
   Uniseries operator+(const Uniseries& that);

   // The std::unique_ptr<FastUtf8::Uniseries> pSeparate() method constructs 
   // an object from a portion of the  existing object's content.  The portion 
   // is derived based on a search for a token.  The new object encompasses 
   // the content "ahead of" a found token.  The existing object is modified 
   // to encompass any remaining content "after" the token.  
   //
   // If the search can find no token, the method effectively moves the 
   // content to the new object.  The method optionally trims white space from 
   // the new object's content.
   std::unique_ptr<FastUtf8::Uniseries> pSeparate(
                      uint8_t *puzTokenSet, bool bTrim = false);
   std::unique_ptr<FastUtf8::Uniseries> pSeparate(
                      char *pszTokenSet, bool bTrim = false);
   std::unique_ptr<FastUtf8::Uniseries> pSeparate(
                      char cToken, bool bTrim = false);
   std::unique_ptr<FastUtf8::Uniseries> pSeparate(
                      Uniseries& tokenSet, bool bTrim = false);

   // Pointer-driven pSeparate() overloads begins the token search from an  
   // address within the "this" content.  The address is specified via the 
   // first parameter.  The caller is responsible for ensuring that it is  
   // within the content.
   //
   // The address returned is the address of the first portion of delimited 
   // content.  The "this" content subsequently refers to any portion of the 
   // orginal content that remains, beyond the token.  MODIFIES THE OBJECT'S 
   // CONTENT BY INSERTING NULLS IN PLACE OF TOKENS.
   uint8_t * pSeparate(uint8_t **ppContent, 
                      const uint8_t *puzTokenSet, bool bTrim = false);
   uint8_t * pSeparate(uint8_t **ppContent, 
                      const char *pszTokenSet, bool bTrim = false);
   uint8_t * pSeparate(uint8_t **ppContent, 
                      const char cToken, bool bTrim = false);
   uint8_t * pSeparate(uint8_t **ppContent, 
                      const Uniseries& tokenSet, bool bTrim = false);

   // The pFindToken() content comparison method searches the "this" content 
   // for any token within a set of tokens.  The token search begins from an 
   // address, within the content, specified via the first parameter.  The 
   // caller is responsible for ensuring that the address is  within the 
   // content.  If a token is found, the method returns a pointer to the code 
   // point immediately prior to it.  Otherwise, the method returns nullptr.
   uint8_t * pFindToken(
      const uint8_t *pContent,      // Content in which to search for tokens
      const uint8_t *pTokenSet)     // Set of tokens
         const noexcept;
   uint8_t * pFindToken(
      const char *pContent, 
      const char *pTokenSet) const noexcept;
   uint8_t * pFindToken(
      const uint8_t *pContent, 
      const Uniseries& sTokenSet) const noexcept;
   uint8_t * pFindToken(
      const char *pContent, 
      const Uniseries& sTokenSet) const noexcept;

   // Single-parameter overloads of pFindToken() begin their search at the top 
   // of the "this" content.
   uint8_t * pFindToken(
      const uint8_t *pTokenSet)     // Set of tokens
         const noexcept;
   uint8_t * pFindToken(
      const char *pTokenSet) const noexcept;
   uint8_t * pFindToken(
      const Uniseries& sTokenSet) const noexcept;

   // FastUtf8::Uniseries::Iterator
   //
   // The methods within the iterator class apply to individual code points in 
   // the content buffer.
   //
   class Iterator
   {
    public:
      // These iterator concepts indicate indexable UTF-8 code points.
      using iterator_category = std::random_access_iterator_tag;
      using value_type = uint8_t;
      using difference_type = std::ptrdiff_t;
      using pointer = uint8_t *;
      using reference = uint32_t;  // Can store any UTF-8 code point.

      // The iterator constructor provides for a buffer comprising a range of 
	  // contiguous code points.
      Iterator(uint8_t *pSeries, uint8_t *pSeriesBase, 
                      uint8_t *pSeriesLimit, Uniseries& series);

      // Basic dereference operators are provided.
      reference operator*() const;
      pointer operator->() const;

      // The prefix and postfix increment operators each advance the iterator 
	  // by a code point.
      Iterator& operator++();
      Iterator operator++(int);

      // The prefix and postfix decrement operators each backtrack the 
	  // iterator by a code point.
      Iterator& operator--();
      Iterator operator--(int);

      // These are equality operators for individual code points in content.
      bool operator==(const Iterator& thatItr);
      bool operator!=(const Iterator& thatItr);

    private:
      uint8_t *pContentBase;
      uint8_t *pContentLimit;
      uint8_t *pContentCurrent;
      FastUtf8::Uniseries& theSeries;
   };

   // The remaining methods in the FastUtf8::Uniseries class apply to content 
   // as a whole.
   Iterator begin();

   // The end() iterator points one past the last element.
   Iterator end();

   // These are basic getter / setter methods for whole content and its 
   // metadata.  This first one gets a pointer to the object's content buffer.
   uint8_t * getContent(void) const noexcept;
   uint64_t getMetadata(void) const noexcept;

   // The getContent() method makes a deep copy of the whole content.  The 
   // developer is responsible for ensuring that the buffer receiving the 
   // content is sufficient.
   void getContent(uint8_t *pOutbound);
   void getContent(char *pOutbound);

   // The IS_CASE_INSENSITIVE flag can be set via either of the next methods.
   void setCaseSensitivity(bool bCaseSensitive) noexcept;
   void setCaseInsensitive(void) noexcept;
   void clearCaseInsensitive(void) noexcept;

   // Setting a nonzero length limit also sets the IS_LENGTH_LIMITED flag.
   void setLengthLimit(int lenContent) noexcept;
   void setLengthLimited(void) noexcept;
   void clearLengthLimited(void) noexcept;

   // Given a byte count, this method returns the corresponding count of code 
   // points between the beginning of the content and the last complete code 
   // point encompassing the given number of bytes.
   int getLength(const size_t sizeContent) noexcept;

   // This method returns the object's current content length as a count of 
   // its code points.
   int getLength(void) const noexcept;

   // The getLength() method returns the length of the inbound content.
   static int getLength(const uint8_t *pInbound) noexcept;
   static int getLength(const char *pInbound) noexcept;
   static int getLength(const Uniseries& that) noexcept;

   // This getSize() method returns the content's size in bytes.
   size_t getSize(void) noexcept;

   // These getSize() implementations return the size of the inbound content, 
   // in bytes.
   static size_t getSize(const uint8_t *pInbound) noexcept;
   static size_t getSize(const char *pInbound) noexcept;
   static size_t getSize(const Uniseries& that) noexcept;

   // The getSizeFolded() method returns the number of bytes needed to store 
   // the content after case folding.  This size may be larger or smaller than 
   // the unfolded size.
   static size_t getSizeFolded(const uint8_t *pInbound) noexcept;
   static size_t getSizeFolded(const char *pInbound) noexcept;
   size_t getSizeFolded(void) const noexcept;
   static size_t getSizeFolded(const Uniseries& that) noexcept;

   // The getFolded() method makes a deep copy of the whole content after case 
   // folding.  The developer is responsible for ensuring that the buffer 
   // receiving the content is sufficient.  The size needed for folded content 
   // may be greater or less than the original content's size, in bytes.  To 
   // predetermine the needed size, invoke the above method.
   static Uniseries getFolded(uint8_t *pOutbound, size_t sizeOutbound);
   static Uniseries getFolded(char *pOutbound, size_t sizeOutbound);
   Uniseries getFolded(void) const;
   static Uniseries getFolded(const Uniseries& that);
   static Uniseries getFolded(const Uniseries& that, size_t sizeOutbound);

   // Is the current content all 7-bit ASCII characters?  If so, the is7Bit() 
   // method sets the IS_7BIT_CHAR_STRING flag and returns true.  Otherwise, 
   // the method clears that flag and returns false.
   static bool is7Bit(const uint8_t *pInbound) noexcept;
   static bool is7Bit(const char *pInbound) noexcept;
   bool is7Bit(void) noexcept;
   static bool is7Bit(const Uniseries& that) noexcept;

   // These are equality operators for whole content comparison.
   bool operator==(const Uniseries& that) const noexcept;
   bool operator!=(const Uniseries& that) const noexcept;

   // The caseCompare() method is identical to the equality operator but 
   // strictly case-insensitive; it does not check the IS_CASE_INSENSITIVE 
   // flag.
   bool caseCompare(const uint8_t *pInbound) const noexcept;
   bool caseCompare(const char *pInbound) const noexcept;
   bool caseCompare(const Uniseries& that) const noexcept;

   // The contains() partial content comparison method returns true if the 
   // "this" object's content contains the inbound content, and false 
   // otherwise.
   bool contains(const uint8_t *pInbound) const noexcept;
   bool contains(const char *pInbound) const noexcept;
   bool contains(const char cInbound) const noexcept;

   // This partial content comparison method returns true if the "this" 
   // object's content contains the "that" object's content, and false 
   // otherwise.
   bool contains(const FastUtf8::Uniseries& that) const noexcept;

   // The find() partial content comparison method returns an index of the 
   // inbound content within the "this" object's content -- that is, a count 
   // of the code points between the beginning of "this" content and any first 
   // match -- or a negative return value (-1) in case that content is not 
   // found.
   int find(const uint8_t *pInbound) const noexcept;
   int find(const char *pInbound) const noexcept;
   int find(const char cInbound) const noexcept;

   // This partial content comparison method returns an index of the "that" 
   // object's content within the "this" object's content -- that is, a count 
   // of the code points between the beginning of "this" content and any first 
   // match -- or a negative return value (-1) in case that content is not 
   // found.
   int find(const FastUtf8::Uniseries& that) const noexcept;

   // The pFind() partial content comparison method returns a pointer to the 
   // inbound search content within the "this" object's content -- that is, a 
   // raw pointer to the beginning of any first match within "this" -- or a 
   // nullptr return value in case that content is not found.  The optional 
   // pFirst parameter refers to a location, in the "this" content, to begin 
   // seeking a match.  The method updates the optional *ppLast parameter to 
   // return the ending location of any first match within "this".
   uint8_t * pFind(
      const uint8_t *pSearchContent,      // Needle
      const uint8_t *pFirst = nullptr,    // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends
   uint8_t * pFind(
      const char *pSearchContent,         // Needle
      const char *pFirst = nullptr,       // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends
   uint8_t * pFind(
      const char cInbound,                // Single ASCII character needle
      const uint8_t *pFirst = nullptr,    // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends

   // This pFind() partial content comparison method returns a pointer to any 
   // first occurrence of the "that" object's content within the "this" 
   // object's content, or nullptr in case that content is not found.
   uint8_t * pFind(
      const FastUtf8::Uniseries& that,    // Object containing needle
      const uint8_t *pFirst = nullptr,    // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends

   // This caseContains() case-insensitive partial content comparison method 
   // returns true if the inbound content is contained within  the "this" 
   // object's content, or false if the inbound content is not found.  
   // It disregards the IS_CASE_INSENSITIVE flag.
   bool caseContains(const uint8_t *pInbound) const noexcept;
   bool caseContains(const char *pInbound) const noexcept;

   // This case-insensitive partial content comparison method returns true if 
   // the "that" content is contained within the "this" content, or false if 
   // that content is not found.  It disregards the IS_CASE_INSENSITIVE flag.
   bool caseContains(const FastUtf8::Uniseries& that) const noexcept;

   // The caseFind() case-insensitive partial content comparison method 
   // returns an index (offset code point count) of the inbound content within 
   // the "this" object's content, or -1 if the inbound content is not found. 
   // It disregards the IS_CASE_INSENSITIVE flag.
   int caseFind(const uint8_t *pInbound) const noexcept;
   int caseFind(const char *pInbound) const noexcept;

   // This case-insensitive partial content comparison method returns an index 
   // (offset code point count) of the "that" content within the "this" 
   // content, or -1 if that content is not found.  It disregards the 
   // IS_CASE_INSENSITIVE flag.
   int caseFind(const FastUtf8::Uniseries& that) const noexcept;

   // The casepFind() case-insensitive partial content comparison method 
   // returns a pointer to any first occurrence of the inbound content within 
   // "this" object's content, or nullptr if the inbound content is not found. 
   // The method updates *ppLast to return the ending location of any first 
   // match within "this".  It disregards the IS_CASE_INSENSITIVE flag.
   uint8_t * casepFind(
      const uint8_t *pSearchContent,      // Needle
      const uint8_t *pFirst = nullptr,    // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends
   uint8_t * casepFind(
      const char *pSearchContent,         // Needle
      const char *pFirst = nullptr,    // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends

   // This casepFind() case-insensitive partial content comparison method 
   // returns a pointer to the "that" content within the "this" content, or 
   // nullptr if that content is not found.  It disregards the 
   // IS_CASE_INSENSITIVE flag.  Optional parameters include a pointer to a 
   // code point at which to begin the search and a returned pointer to the 
   // last code point of the match, if any, in the found content.
   uint8_t * casepFind(
      const FastUtf8::Uniseries& that,    // Object containing needle
      const uint8_t *pFirst = nullptr,    // Beginning location
      uint8_t **ppLast = nullptr) const;  // Returned location where match ends

   // The wildCompare() method provides for wildcard-based content comparison. 
   // The "this" content is the content that may include the '*' or '?' 
   // wildcards.
   bool wildCompare(const uint8_t *pTame) const noexcept;
   bool wildCompare(const char *pTame) const noexcept;
   bool wildCompare(const FastUtf8::Uniseries& tame) const noexcept;

   // The wildCaseCompare() method provides for strictly case-insensitive 
   // wildcard-based content comparison.  It does not check the 
   // IS_CASE_INSENSITIVE flag.  The "this" content is the content that may 
   // include the '*' or '?' wildcards.
   bool wildCaseCompare(const uint8_t *pTame) const noexcept;
   bool wildCaseCompare(const char *pTame) const noexcept;
   bool wildCaseCompare(const FastUtf8::Uniseries& tame) const noexcept;

   // The compareWild() method provides for wildcard-based content comparison. 
   // The passed-in content is the content that may include the '*' or '?' 
   // wildcards.
   bool compareWild(const uint8_t *pWild) const noexcept;
   bool compareWild(const char *pWild) const noexcept;
   bool compareWild(const FastUtf8::Uniseries& wild) const noexcept;

   // The caseCompareWild() method provides for strictly case-insensitive 
   // wildcard-based content comparison.  It does not check the 
   // IS_CASE_INSENSITIVE flag.  The passed-in content is the content that may 
   // include the '*' or '?' wildcards.
   bool caseCompareWild(const uint8_t *pWild) const noexcept;
   bool caseCompareWild(const char *pWild) const noexcept;
   bool caseCompareWild(const FastUtf8::Uniseries& wild) const noexcept;

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
   // If no match is found, or if there is no first wildcard, sets *ppFirst, 
   // *ppLast, and *ppTarget to nullptr and returns nullptr.
   //
   uint8_t * pFindWild(
      const uint8_t *pWild,                 // Search pattern (with wildcards)
      uint8_t       **ppFirst = nullptr,    // Updated beginning location
      uint8_t       **ppLast = nullptr,     // Returned loc where match ends
      uint8_t       **ppTarget = nullptr)   // Returned loc after last '*'
         const noexcept;
   uint8_t * pFindWild(
      const char *pWild, 
      uint8_t       **ppFirst = nullptr, 
      uint8_t       **ppLast = nullptr, 
      uint8_t       **ppTarget = nullptr) const noexcept;
   uint8_t * pFindWild(
      const Uniseries& sWild, 
      uint8_t       **ppFirst = nullptr, 
      uint8_t       **ppLast = nullptr, 
      uint8_t       **ppTarget = nullptr) const noexcept;

   // Case-insensitive implementation of the pFindWild() partial content 
   // comparison method.
   uint8_t * casepFindWild(
      const uint8_t *pWild,                 // Search pattern (with wildcards)
      uint8_t       **ppFirst = nullptr,    // Updated beginning location
      uint8_t       **ppLast = nullptr,     // Returned loc where match ends
      uint8_t       **ppTarget = nullptr)   // Returned loc after last '*'
         const noexcept;
   uint8_t * casepFindWild(
      const char *pWild, 
      uint8_t       **ppFirst = nullptr, 
      uint8_t       **ppLast = nullptr, 
      uint8_t       **ppTarget = nullptr) const noexcept;
   uint8_t * casepFindWild(
      const Uniseries& sWild, 
      uint8_t       **ppFirst = nullptr, 
      uint8_t       **ppLast = nullptr, 
      uint8_t       **ppTarget = nullptr) const noexcept;

   // The non-const and const subscript (index) operators call their 
   // supporting C function, unless the entire content is ASCII text.
   uint32_t operator[](int iIndex) noexcept;
   const uint32_t operator[](int iIndex) const noexcept;

   // The trim() method removes outboard white space from the object's current 
   // content by replacing the code points with nulls.
   void trim(void);

   // The validate() method returns true if all of the "this" object's content 
   // is valid UTF-8.  If IS_LENGTH_LIMITED is set, it validates as many code 
   // points as have been specified via the setLengthLimit() method.  
   // Otherwise it validates code points until it encounters a terminating 
   // null and gets their count, which it returns via the iCount parameter.
   bool validate(int *iCount) const noexcept;
   bool validate(void) const noexcept;

   // In case content validation fails, the convert8BitAscii() method may 
   // serve as a reasonable fallback.  It returns valid UTF-8 content and sets 
   // the iCount parameter reflecting the content's length, as a count of its 
   // code points.
   uint8_t * convert8BitAscii(int *iCount) noexcept;
   uint8_t * convert8BitAscii(void) noexcept;

   // The output stream operator<< can be used to send a Uniseries to the 
   // console.
   friend std::ostream& operator<<(std::ostream& theStream, 
                      const FastUtf8::Uniseries& theOutput);
  };    // class Uniseries

  // A counterpart >> operator would be relatively complicated.  It would have 
  // to manage a resizeable input buffer to handle input of arbitrary size, 
  // and it would have to ensure the needed memory management for the content 
  // buffer itself (m_pContent).

  // This Uniseries comparison operator applies for pointer == object.
  bool operator==(const Uniseries* puSeries, const Uniseries& uSeries);

  // This Uniseries comparison operator applies for object == pointer.
  bool operator==(const Uniseries& uSeries, const Uniseries* puSeries);
};      // namespace FastUtf8
#endif  // __cplusplus
#endif  // FASTUTF8_H
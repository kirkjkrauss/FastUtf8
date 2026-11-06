// C++ demo for UTF-8-ready selective targeted wildcard search routines.
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
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <filesystem>
#include "../fastutf8.h"

// Loads UTF-8 content from the given file on disk.
//
uint8_t * LoadContent(const char *pszContentFileName)
{
   uint8_t          *pBuffer = nullptr;
   std::streamsize  sizeContent = 0;

   // Open the file and position the read pointer to its end, to get its size.
   std::ifstream stream(pszContentFileName, std::ios::binary | std::ios::ate);

   if (stream.is_open())
   {
      sizeContent = stream.tellg();       // Get the file size.
      stream.seekg(0, std::ios::beg);     // Reset the read pointer.

     // Allocate a buffer sufficient for the whole file.
     pBuffer = new(std::nothrow) uint8_t[1 + sizeContent];

      if (pBuffer)                        // Got an allocated buffer?
      {
         // Read the file into the buffer.
         if (stream.read(reinterpret_cast<char *> (pBuffer), sizeContent))
         {
            pBuffer[sizeContent] = '\0';  // Add a terminating null.
         }
         else
         {
            delete[] pBuffer;
            pBuffer = nullptr;
         }
      }
   }

   return pBuffer;
}

// This demo loads a sample file comprising US surnames, birth dates, and 
// birth places (US states), organized by generation, in XML format.  Within 
// entries corresponding to the silent generation, it selects all case-
// insensitive matches on a specific state (New Jersey) and outputs the 
// corresponding surnames and birth dates.
//
void XMLDemoAscii(void)
{
   FastUtf8::Uniseries sSurname;         // Name to be extracted from XML.
   uint8_t *puzSurname, *puzSurnameEnd;  // Content range comprising the name.
   FastUtf8::Uniseries sDOB;             // Date of birth to be extracted.
   uint8_t *puzDOB, *puzDOBEnd;          // Range comprising the date of birth.
   uint8_t *puzMatch;                    // First match on the overall pattern.
   uint8_t *pBuffer;                     // Block containing data from file.

   // Load a sample XML file that contains mixed-case ASCII text.
   pBuffer = LoadContent("TwentiethCenturyUS.xml");

   if (!pBuffer)
   {
      std::cout << "Could not load TwentiethCenturyUS.xml" << std::endl;
      return;
   }

   // When the buffer is wrapped in a Uniseries object, it gets deallocated 
   // when the object is destroyed, unless bDeallocOnDestruct is false.
   FastUtf8::Uniseries sXML = FastUtf8::Uniseries(pBuffer, 
                  /* bWrapBuffer = */ true, /* bDeallocOnDestruct = */ false);

   // Prepare a lowercased state identifier for New Jersey.
   uint8_t puzState[3] = "nj";

   // Begin the selective targeted wildcard search at the top of the content.
   uint8_t *pTopOfXML = sXML.getLength() ? sXML.getContent() : nullptr;

   if (!pTopOfXML)
   {
      std::cout << "No content found in TwentiethCenturyUS.xml" << std::endl;
      delete[] pBuffer;
      return;
   }

   // Prepare a wildcarded search pattern to find silent generation authors 
   // whose birth state, as provided within the XML dataset, can be filled in.
   FastUtf8::Uniseries sBeginPattern(
    "<generation>*<silent>*<entry>*<surname>*</surname>*<dob>*</dob>*<state>");
   FastUtf8::Uniseries sEndPattern(
    "</state>*</entry>*</silent>*</generation>");

   // Set the nesting level at which to seek matches within the loaded XML 
   // content.  Because the zeroth nesting level is outside of any tags 
   // designated in the search pattern, iLevel corresponds to a count of '*' 
   // wildcards, starting with one, between each end of the overall pattern 
   // and the central portion to match to the array entries to be populated.  
   // The first (number 1) '*' wildcard is between "<generation>" and 
   // "<silent>".
   const int iLevel = 3;

   // The count of '*' wildcards restarts within the central portion of the 
   // search pattern.  There, the zeroth '*' wildcard gets matched to the 
   // zeroth array element.  We choose ordinals for matches on the 3rd and 5th 
   // '*' wildcards (zero-based) within the overall pattern.
   const int iSurnameMatch = 3 - iLevel;  // Choose the '*' within surname tags
   const int iDOBMatch = 5 - iLevel;      // Choose the '*' within dob tags

   // Prepare an empty result set, to be ordered by surname.
   const std::size_t iOrderBy = static_cast<std::size_t> (iSurnameMatch);
   FastUtf8::MatchComparator fComparator(iOrderBy);
   FastUtf8::ResultSet fResultSet(fComparator);

   // Set up the search pattern so the authors' birth state matches "nj".
   FastUtf8::Uniseries sPattern = sBeginPattern;
   sPattern += puzState;
   sPattern += sEndPattern;

   // Retrieve matches on the '*' wildcards in the search pattern.
   puzMatch = sXML.casepSelectWild(&fResultSet, sPattern, iLevel, pTopOfXML);

   if (!puzMatch)
   {
      std::cout << "No match found for pattern containing state " << puzState;
      std::cout << " in TwentiethCenturyUS.xml" << std::endl;
      delete[] pBuffer;
      return;
   }

   std::cout << "Searched TwentiethCenturyUS.xml for state " << puzState;

   for (const FastUtf8::MatchVector& fVector : fResultSet)
   {
      puzSurname = fVector.matches[iSurnameMatch].pBegin;
      puzSurnameEnd = fVector.matches[iSurnameMatch].pEnd;
      puzDOB = fVector.matches[iDOBMatch].pBegin;
      puzDOBEnd = fVector.matches[iDOBMatch].pEnd;

      // Construct a Uniseries from the content between the pointers.
      if (puzSurname)
      {
         sSurname.fromSlice(puzSurname, puzSurnameEnd);

         if (puzDOB)
         {
            sDOB.fromSlice(puzDOB, puzDOBEnd);
         }
      }
      else
      {
         sSurname = "No match found for ";
         sSurname += puzState;
         break;
      }

      std::cout << std::endl << "   Finding: " << sSurname;

      if (sDOB.getLength())
      {
         std::cout << " \t  DOB:" << sDOB;
      }

      std::cout << std::endl;
   }

   std::cout << std::endl;
   delete[] pBuffer;
   return;
}

// This demo loads a sample file comprising mixed-case Russian surnames, birth 
// dates, and birth places (районы or districts), organized by generation, in 
// XML format.  Within entries corresponding to the silent generation, it 
// selects all case-insensitive matches on a specific район (Московская) and 
// outputs the corresponding surnames and birth dates.
//
void XMLDemoUtf8(void)
{
   FastUtf8::Uniseries sSurname;         // Name to be extracted from XML.
   uint8_t *puzSurname, *puzSurnameEnd;  // Content range comprising the name.
   FastUtf8::Uniseries sDOB;             // Date of birth to be extracted.
   uint8_t *puzDOB, *puzDOBEnd;          // Range comprising the date of birth.
   uint8_t *puzMatch;                    // First match on the overall pattern.
   uint8_t *pBuffer;                     // Block containing data from file.

   // Load a sample XML file that contains mixed-case Russian text.
   pBuffer = LoadContent("TwentiethCenturyRu.xml");

   if (!pBuffer)
   {
      std::cout << "Could not load TwentiethCenturyRu.xml" << std::endl;
      return;
   }

   // When the buffer is wrapped in a Uniseries object, it gets deallocated 
   // when the object is destroyed, unless bDeallocOnDestruct is false.
   FastUtf8::Uniseries sXML = FastUtf8::Uniseries(pBuffer, 
                  /* bWrapBuffer = */ true, /* bDeallocOnDestruct = */ false);

   // The Moscow district is lowercased here but mixed case in the XML file.
   uint8_t puzRaion[40] = "московская";

   // Begin the selective targeted wildcard search at the top of the content.
   uint8_t *pTopOfXML = sXML.getLength() ? sXML.getContent() : nullptr;

   if (!pTopOfXML)
   {
      std::cout << "No content found in TwentiethCenturyRu.xml" << std::endl;
      delete[] pBuffer;
      return;
   }

   // Prepare a wildcarded search pattern to find молчаливое generation 
   // authors whose birth район can be filled in.
   FastUtf8::Uniseries sBeginPattern(
    "<поколение>*<молчаливое>*<запись>*<фамилия>*</фамилия>*<рождения>*</рождения>*<район>");
   FastUtf8::Uniseries sEndPattern(
    "</район>*</запись>*</молчаливое>*</поколение>");

   // Set the nesting level at which to seek matches within the loaded XML 
   // content.  The zeroth nesting level is outside of any tags.  Accordingly, 
   // the first (number 1) '*' wildcard is between "<поколение>" and 
   // "<молчаливое>".
   const int iLevel = 3;

   // The count of '*' wildcards restarts within the central portion of the 
   // search pattern.  There, the zeroth '*' wildcard gets matched to the 
   // zeroth array element.  We choose ordinals for matches on the 3rd and 5th 
   // '*' wildcards (zero-based) within the overall pattern.
   const int iSurnameMatch = 3 - iLevel;  // Choose the '*' within surname tags
   const int iDOBMatch = 5 - iLevel;      // Choose the '*' within dob tags

   // Prepare an empty result set, to be ordered by surname.
   const std::size_t iOrderBy = static_cast<std::size_t> (iSurnameMatch);
   FastUtf8::MatchComparator fComparator(iOrderBy);
   FastUtf8::ResultSet fResultSet(fComparator);

   // Set the authors' birth район to matche "московская".
   FastUtf8::Uniseries sPattern = sBeginPattern;
   sPattern += puzRaion;
   sPattern += sEndPattern;

   // Retrieve matches on the '*' wildcards in the search pattern.
   puzMatch = sXML.casepSelectWild(&fResultSet, sPattern, iLevel, pTopOfXML);

   if (!puzMatch)
   {
      std::cout << "No match found for pattern containing район " << puzRaion;
      std::cout << " in TwentiethCenturyRu.xml" << std::endl;
      delete[] pBuffer;
      return;
   }

   std::cout << "Searched TwentiethCenturyRu.xml for район " << puzRaion;

   for (const FastUtf8::MatchVector& fVector : fResultSet)
   {
      puzSurname = fVector.matches[iSurnameMatch].pBegin;
      puzSurnameEnd = fVector.matches[iSurnameMatch].pEnd;
      puzDOB = fVector.matches[iDOBMatch].pBegin;
      puzDOBEnd = fVector.matches[iDOBMatch].pEnd;

      // Construct a Uniseries from the content between the pointers.
      if (puzSurname)
      {
         sSurname.fromSlice(puzSurname, puzSurnameEnd);

         if (puzDOB)
         {
            sDOB.fromSlice(puzDOB, puzDOBEnd);
         }
      }
      else
      {
         sSurname = "No match found for ";
         sSurname += puzRaion;
         break;
      }

      std::cout << std::endl << "   Находка: " << sSurname;

      if (sDOB.getLength())
      {
         std::cout << " \t  рождения:" << sDOB;
      }

      std::cout << std::endl;
   }

   std::cout << std::endl;
   delete[] pBuffer;
   return;
}

// This demo loads a sample file comprising US surnames, birth dates, and 
// birth places (US states), organized by generation, in JSON format.  
// Within entries corresponding to the silent generation, it selects all 
// case-insensitive matches on a specific state (New Jersey) and outputs 
// the corresponding surnames and birth dates.
//
void JsonDemoAscii(void)
{
   FastUtf8::Uniseries sSurname;         // Name to be extracted from XML.
   uint8_t *puzSurname, *puzSurnameEnd;  // Content range comprising the name.
   FastUtf8::Uniseries sDOB;             // Date of birth to be extracted.
   uint8_t *puzDOB, *puzDOBEnd;          // Range comprising the date of birth.
   uint8_t *puzMatch;                    // First match on the overall pattern.
   uint8_t *pBuffer;                     // Block containing data from file.

   // Load a sample JSON file that contains mixed-case ASCII text.
   pBuffer = LoadContent("TwentiethCenturyUS.json");

   if (!pBuffer)
   {
      std::cout << "Could not load TwentiethCenturyUS.json" << std::endl;
      return;
   }

   // When the buffer is wrapped in a Uniseries object, by default it gets 
   // deallocated when the object is destroyed.
   FastUtf8::Uniseries sJson = FastUtf8::Uniseries(pBuffer, 
                                                   /* bWrapBuffer = */ true);

   // Prepare a lowercased state identifier for New Jersey.
   uint8_t puzState[3] = "nj";

   // Begin the selective targeted wildcard search at the top of the content.
   uint8_t *pTopOfJson = sJson.getLength() ? sJson.getContent() : nullptr;

   if (!pTopOfJson)
   {
      std::cout << "No content found in TwentiethCenturyUS.json" << std::endl;
      return;
   }

   // Prepare a wildcarded search pattern to find silent generation authors 
   // whose birth state, as provided within the JSON dataset, matches "nj".
   FastUtf8::Uniseries sBeginPattern(
    "{*\"generation\": {*\"silent\": {*\"entry\": [*{*\"surname\": \"*\",*\"dob\": \"*\",*\"state\": \"");
   FastUtf8::Uniseries sEndPattern(
    "\"*}*]*}*}*}");

   // Set the nesting level at which to seek matches within the loaded JSON 
   // content.  Because the zeroth nesting level is outside of any tags 
   // designated in the search pattern, iLevel corresponds to a count of '*' 
   // wildcards, starting with one, between each end of the overall pattern 
   // and the central portion to match to the array entries to be populated.  
   // The first (number 1) '*' wildcard is between the first opening brace 
   // and "generation".
   const int iLevel = 5;

   // The count of '*' wildcards restarts within the central portion of the 
   // search pattern.  There, the zeroth '*' wildcard gets matched to the 
   // zeroth array element.  We choose ordinals for matches on the 5th and 7th 
   // '*' wildcards (zero-based) within the overall pattern.
   const int iSurnameMatch = 5 - iLevel;  // Choose the '*' after surname
   const int iDOBMatch = 7 - iLevel;      // Choose the '*' after dob

   // Prepare an empty result set, to be ordered by surname.
   const std::size_t iOrderBy = static_cast<std::size_t> (iSurnameMatch);
   FastUtf8::MatchComparator fComparator(iOrderBy);
   FastUtf8::ResultSet fResultSet(fComparator);

   FastUtf8::Uniseries sPattern = sBeginPattern;
   sPattern += puzState;
   sPattern += sEndPattern;

   // Retrieve matches on the '*' wildcards in the search pattern.
   puzMatch = sJson.casepSelectWild(&fResultSet, sPattern, iLevel, pTopOfJson);

   if (!puzMatch)
   {
      std::cout << "No match found for pattern containing state " << puzState;
      std::cout << " in TwentiethCenturyUS.json" << std::endl;
      return;
   }

   std::cout << "Searched TwentiethCenturyUS.json for state " << puzState;

   for (const FastUtf8::MatchVector& fVector : fResultSet)
   {
      puzSurname = fVector.matches[iSurnameMatch].pBegin;
      puzSurnameEnd = fVector.matches[iSurnameMatch].pEnd;
      puzDOB = fVector.matches[iDOBMatch].pBegin;
      puzDOBEnd = fVector.matches[iDOBMatch].pEnd;

      // Construct a Uniseries from the content between the pointers.
      if (puzSurname)
      {
         sSurname.fromSlice(puzSurname, puzSurnameEnd);

         if (puzDOB)
         {
            sDOB.fromSlice(puzDOB, puzDOBEnd);
         }
      }
      else
      {
         sSurname = "No match found for ";
         sSurname += puzState;
         break;
      }

      std::cout << std::endl << "   Finding: " << sSurname;

      if (sDOB.getLength())
      {
         std::cout << " \t  DOB:" << sDOB;
      }

      std::cout << std::endl;
   }

   std::cout << std::endl;
   return;
}

// This demo loads a sample file comprising mixed-case Russian surnames, birth 
// dates, and birth places (районы or districts), organized by generation, in 
// JSON format.  Within entries corresponding to the silent generation, it 
// selects all case-insensitive matches on a specific район (Московская) and 
// outputs the corresponding surnames and birth dates.
//
void JsonDemoUtf8(void)
{
   FastUtf8::Uniseries sSurname;         // Name to be extracted from XML.
   uint8_t *puzSurname, *puzSurnameEnd;  // Content range comprising the name.
   FastUtf8::Uniseries sDOB;             // Date of birth to be extracted.
   uint8_t *puzDOB, *puzDOBEnd;          // Range comprising the date of birth.
   uint8_t *puzMatch;                    // First match on the overall pattern.
   uint8_t *pBuffer;                     // Block containing data from file.

   // Load a sample JSON file that contains mixed-case Russian text.
   pBuffer = LoadContent("TwentiethCenturyRu.json");

   if (!pBuffer)
   {
      std::cout << "Could not load TwentiethCenturyRu.json" << std::endl;
      return;
   }

   // When the buffer is wrapped in a Uniseries object, by default it gets 
   // deallocated when the object is destroyed.
   FastUtf8::Uniseries sJson = FastUtf8::Uniseries(pBuffer, 
                                                   /* bWrapBuffer = */ true);

   // The Moscow district is lowercased here but mixed case in the JSON file.
   uint8_t puzRaion[40] = "московская";

   // Begin the selective targeted wildcard search at the top of the content.
   uint8_t *pTopOfJson = sJson.getLength() ? sJson.getContent() : nullptr;

   if (!pTopOfJson)
   {
      std::cout << "No content found in TwentiethCenturyRu.json" << std::endl;
      return;
   }

   // Prepare a wildcarded search pattern to find silent generation authors 
   // whose birth район, as provided within the JSON dataset, matches 
   // "московская".
   FastUtf8::Uniseries sBeginPattern(
    "{*\"поколение\": {*\"молчаливое\": {*\"запись\": [*{*\"фамилия\": \"*\",*\"рождения\": \"*\",*\"район\": \"");
   FastUtf8::Uniseries sEndPattern(
    "\"*}*]*}*}*}");

   // Set the nesting level at which to seek matches within the loaded JSON 
   // content.  Because the zeroth nesting level is outside of any tags 
   // designated in the search pattern, iLevel corresponds to a count of '*' 
   // wildcards, starting with one, between each end of the overall pattern 
   // and the central portion to match to the array entries to be populated.  
   // The first (number 1) '*' wildcard is between the first opening brace 
   // and "поколение".
   const int iLevel = 5;

   // The count of '*' wildcards restarts within the central portion of the 
   // search pattern.  There, the zeroth '*' wildcard gets matched to the 
   // zeroth array element.  We choose ordinals for matches on the 5th and 7th 
   // '*' wildcards (zero-based) within the overall pattern.
   const int iSurnameMatch = 5 - iLevel;  // Choose the '*' after surname
   const int iDOBMatch = 7 - iLevel;      // Choose the '*' after dob

   // Prepare an empty result set, to be ordered by surname.
   const std::size_t iOrderBy = static_cast<std::size_t> (iSurnameMatch);
   FastUtf8::MatchComparator fComparator(iOrderBy);
   FastUtf8::ResultSet fResultSet(fComparator);

   FastUtf8::Uniseries sPattern = sBeginPattern;
   sPattern += puzRaion;
   sPattern += sEndPattern;

   // Retrieve matches on the '*' wildcards in the search pattern.
   puzMatch = sJson.casepSelectWild(&fResultSet, sPattern, iLevel, pTopOfJson);

   if (!puzMatch)
   {
      std::cout << "No match found for pattern containing район " << puzRaion;
      std::cout << " in TwentiethCenturyRu.json" << std::endl;
      return;
   }

   std::cout << "Searched TwentiethCenturyRu.json for район " << puzRaion;

   for (const FastUtf8::MatchVector& fVector : fResultSet)
   {
      puzSurname = fVector.matches[iSurnameMatch].pBegin;
      puzSurnameEnd = fVector.matches[iSurnameMatch].pEnd;
      puzDOB = fVector.matches[iDOBMatch].pBegin;
      puzDOBEnd = fVector.matches[iDOBMatch].pEnd;

      // Construct a Uniseries from the content between the pointers.
      if (puzSurname)
      {
         sSurname.fromSlice(puzSurname, puzSurnameEnd);

         if (puzDOB)
         {
            sDOB.fromSlice(puzDOB, puzDOBEnd);
         }
      }
      else
      {
         sSurname = "No match found for ";
         sSurname += puzRaion;
         break;
      }

      std::cout << std::endl << "   Находка: " << sSurname;

      if (sDOB.getLength())
      {
         std::cout << " \t  рождения:" << sDOB;
      }

      std::cout << std::endl;
   }

   std::cout << std::endl;
   return;
}

// Entry point for JSON and XML demos.
//
int main(int argc, char **argv)
{
   int  iRepetitions;
   int  iPart = 0;
   bool bComparePerformance = false;

#if defined(_WIN32)
   SetConsoleOutputCP(CP_UTF8);
   SetConsoleCP(CP_UTF8);
#endif

   // Get performance comparison results against standard library ASCII 
   // string functions?
   if (argc > 1 && (argv[1][0] == 'w' || 
                    argv[1][0] == 'p' || 
                    argv[1][0] == 'c'))
   {
      bComparePerformance = true;

      // How many times to repeat tests, for function time accumulation?
      if (argc > 2)
      {
         iRepetitions = atoi(argv[2]);
      }
      else
      {
         iRepetitions = 1000000;
      }
   }   
   else
   {
      bComparePerformance = false;
      iRepetitions = 0;
   }

   std::cout << "Demo of hierarchical content selection via wildcards" << std::endl;

   // Find content in ASCII XML data.
   ++iPart;
   std::cout << std::endl << "Part " << iPart << std::endl;
   std::cout << "XML demo (ASCII)" << std::endl << std::endl;
   XMLDemoAscii();

   // Find content in internationalized XML data.
   ++iPart;
   std::cout << std::endl << "Part " << iPart << std::endl;
   std::cout << "XML demo (UTF-8)" << std::endl << std::endl;
   XMLDemoUtf8();

   // Find content in ASCII JSON data.
   ++iPart;
   std::cout << std::endl << "Part " << iPart << std::endl;
   std::cout << "JSON demo (ASCII)" << std::endl << std::endl;
   JsonDemoAscii();

   // Find content in internationalized JSON data.
   ++iPart;
   std::cout << std::endl << "Part " << iPart << std::endl;
   std::cout << "JSON demo (UTF-8)" << std::endl << std::endl;
   JsonDemoUtf8();

   if (bComparePerformance)
   {
      std::cout << "Performance demo" << 
         std::endl << std::endl;
   }
   else
   {
      std::cout << "End of demo: hierarchical content selection via wildcards" << 
         std::endl << std::endl;
   }

   return 0;
}
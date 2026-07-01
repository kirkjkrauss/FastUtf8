// UTF-8-ready C/C++ routines for matching wildcards.
//
// Copyright 2025 Kirk J Krauss.  This is a Derivative Work based on 
// material that is copyright 2025 Kirk J Krauss and available at
//
//     https://developforperformance.com/MatchingWildcards_AnImprovedAlgorithmForBigData.html
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
#include <stddef.h>              // for size_t

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
/* NOT VALIDATED HERE    0x7F */ // 0nnnnnnn  (an entire 1-byte code point)
#define SINGLETON_LIMIT  0xBF    // 10nnnnnn  (an intra-code-point byte)
#define TWOFER_LIMIT     0xDF    // 110nnnnn  (first of a 2-byte code point)
#define THREESOME_LIMIT  0xEF    // 1110nnnn  (first of a 3-byte code point)

// Given a pointer to a UTF-8 code point, advances it to any next UTF-8 code 
// point.  Returns true if there is a further code point, or false if the 
// next content is a terminating null.  PERFORMS NO UTF-8 VALIDATION OTHER
// THAN NULL CHECKING.
//
inline bool CodePointAdvance(char **ppContent)
{
	*ppContent += (*(unsigned char *) *ppContent > 0) +
	    ((*(unsigned char *) *ppContent > SINGLETON_LIMIT) && 
	        *(1 + *ppContent)) +
	    ((*(unsigned char *) *ppContent > TWOFER_LIMIT) && 
	        *(2 + *ppContent)) +
		((*(unsigned char *) *ppContent > THREESOME_LIMIT) && 
	        *(3 + *ppContent));
	return (bool) **ppContent;
}


// Compares two UTF-8 code points.  Returns true if the code points are 
// identical.  Returns false otherwise.  PERFORMS NO UTF-8 VALIDATION.
//
inline bool CodePointCompare(char *pContentA, char *pContentB)
{
	if (*pContentA != *pContentB)
	{
		return false;
	}
	else if (*(unsigned char *) pContentA > SINGLETON_LIMIT &&
	         *(1 + pContentA) != *(1 + pContentB))
	{
		return false;
	}
	else if (*(unsigned char *) pContentA > TWOFER_LIMIT &&
	         *(2 + pContentA) != *(2 + pContentB))
	{
		return false;
	}
	else if (*(unsigned char *) pContentA > THREESOME_LIMIT &&
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
inline bool CodePointAdvanceAndCompare(char *pContentA, char **ppContentB)
{
	*ppContentB += (*(unsigned char *) *ppContentB > 0) +
		((*(unsigned char *) *ppContentB > SINGLETON_LIMIT) && 
		    *(1 + *ppContentB)) +
		((*(unsigned char *) *ppContentB > TWOFER_LIMIT) && 
		    *(2 + *ppContentB)) +
		((*(unsigned char *) *ppContentB > THREESOME_LIMIT) && 
		    *(3 + *ppContentB));

	if (*pContentA != **ppContentB)
	{
		return false;
	}
	else if (*(unsigned char *) pContentA > SINGLETON_LIMIT &&
		*(1 + pContentA) != *(1 + *ppContentB))
	{
		return false;
	}
	else if (*(unsigned char *) pContentA > TWOFER_LIMIT &&
		*(2 + pContentA) != *(2 + *ppContentB))
	{
		return false;
	}
	else if (*(unsigned char *) pContentA > THREESOME_LIMIT &&
		*(3 + pContentA) != *(3 + *ppContentB))
	{
		return false;
	}

	return true;
}


// Given a null-terminated UTF-8 string, returns the number of code points 
// in it.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.
//
size_t CodePointCount(char* pContent)
{
	char *pCodePointInContent = pContent;
	size_t iCount = *(unsigned char *) pCodePointInContent > 0;

	while (CodePointAdvance(&pCodePointInContent))
	{
		iCount++;
	}

	return iCount;
}


// C++ implementation of FastWildCompare(), for null-terminated strings 
// comprising valid UTF-8 code points.  PERFORMS NO UTF-8 VALIDATION.
//
// Compares two strings.  Accepts '?' as a single-code-point wildcard.  For 
// each '*' wildcard, seeks out a matching sequence of any code points beyond 
// it.  Otherwise compares the strings a code point at a time. 
//
bool FastWildCompareUtf8(char *pWild, char *pTame)
{
	char *pWildSequence;  // Points to prospective wild string match after '*'
	char *pTameSequence;  // Points to prospective tame string match

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
						return true;   // "ab" matches "ab*".
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
			while (CodePointAdvance(&pWild) && *pWild == '*')
			{
				continue;
			}

			if (!*pWild)
			{
				return true;           // "abc*" matches "abcd".
			}

			// Search for the next prospective match.
			if (*pWild !='?')
			{
				while (!CodePointCompare(pWild, pTame))
				{
					if (!CodePointAdvance(&pTame))
					{
						return false;  // "a*bc" doesn't match "ab".
					}
				}
			}

			// Keep fallback positions for retry in case of incomplete match.
			pWildSequence = pWild;
			pTameSequence = pTame;
			break;
		}
		else if (!CodePointCompare(pWild, pTame) && *pWild != '?')
		{
			return false;              // "abc" doesn't match "abd".
		}

		// Everything's a match, so far.
		CodePointAdvance(&pWild);
		CodePointAdvance(&pTame);
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
				while (!CodePointCompare(pWild, pTame))
				{
					if (!CodePointAdvance(&pTame))
					{
						return false;  // "a*b*c" doesn't match "ab".
					}
				}
			}

			// Keep the new fallback positions.
			pWildSequence = pWild;
			pTameSequence = pTame;
		}
		else if (!CodePointCompare(pWild, pTame) && *pWild != '?')
		{
			// The equivalent portion of the upper loop is really simple.
			if (!*pTame)
			{
				return false;          // "*bcd" doesn't match "abc".
			}

			// A fine time for questions.
			while (*pWildSequence == '?')
			{
				++pWildSequence;
				++pTameSequence;
			}

			// Fall back, but never so far again.
			pWild = pWildSequence;

			while (!CodePointAdvanceAndCompare(pWild, &pTameSequence))
			{
				if (!*pTameSequence)
				{
					return false;      // "*a*b" doesn't match "ac".
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
				return false;          // "*bc" doesn't match "abcd".
			}
		}

		CodePointAdvance(&pWild);      // Everything's still a match.
		CodePointAdvance(&pTame);
	} while (true);
}

// C++ implementation of FastWildCompare(), for strings comprising valid 
// UTF-8 code points.  PERFORMS NO UTF-8 VALIDATION.
//
// Compares two strings up to a specified number of code points.  Accepts '?' 
// as a single-code-point wildcard.  For each '*' wildcard, seeks out a 
// matching sequence of any code points beyond it.  Otherwise compares the 
// strings a code point at a time. 
//
bool FastWildLenCompareUtf8(char *pWild, char *pTame, int lenWild, 
                            int lenTame)
{
	int iWild = 0;        // Index for both inputs in upper loop
	int iTame = 0;        // Index for tame content, used in lower loop
	int iWildSequence;    // Index for prospective match after '*'
	int iTameSequence;    // Index for match in tame content
	char *pWildSequence;  // Points to prospective wild string match after '*'
	char *pTameSequence;  // Points to prospective tame string match

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
						return true;   // "ab" matches "ab*".
					}
				}

			    return false;          // "abcd" doesn't match "abc".
			}
			else
			{
				return true;           // "abc" matches "abc".
			}
		}
		else if (lenWild <= iWild)
		{
			return false;              // "abc" doesn't match "abcd".
		}
		else if (*pWild == '*')
		{
			// Got wild: set up for the second loop and skip on down there.
			iTame = iWild;

			while (CodePointAdvance(&pWild))
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
			if (*pWild !='?')
			{
				while (!CodePointCompare(pWild, pTame))
				{
					if (!CodePointAdvance(&pTame))
					{
						return false;  // "a*bc" doesn't match "ab".
					}

					iTame++;
				}
			}

			// Keep fallback positions for retry in case of incomplete match.
			pWildSequence = pWild;
			pTameSequence = pTame;
			iWildSequence = iWild;
			iTameSequence = iTame;
			break;
		}
		else if (!CodePointCompare(pWild, pTame) && *pWild != '?')
		{
			return false;              // "abc" doesn't match "abd".
		}

		// Everything's a match, so far.
		CodePointAdvance(&pWild);
		CodePointAdvance(&pTame);
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
					return true;   // "ab*c*" matches "abcd".
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
				while (!CodePointCompare(pWild, pTame))
				{
					if (!CodePointAdvance(&pTame))
					{
						return false;  // "a*b*c" doesn't match "ab".
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
		else if (!CodePointCompare(pWild, pTame) && *pWild != '?')
		{
			// The equivalent portion of the upper loop is really simple.
			if (lenTame <= iTame || !*pTame)
			{
				return false;          // "*bcd" doesn't match "abc".
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

			while (!CodePointAdvanceAndCompare(pWild, &pTameSequence))
			{
				if (lenTame <= ++iTameSequence || !*pTameSequence)
				{
					return false;      // "*a*b" doesn't match "ac".
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

			return false;              // "*bc" doesn't match "abcd".
		}
		
		// Everything's still a match.
		CodePointAdvance(&pWild);
		CodePointAdvance(&pTame);
		iWild++;
		iTame++;
	} while (true);
}


// Compares two ASCII text strings.  Accepts '?' as a single-character 
// wildcard.  For each '*' wildcard, seeks out a matching sequence of any 
// characters beyond it.  Otherwise compares the strings a character at a 
// time.  DUPLICATE OF 2018 CODE, INCLUDED HERE FOR PERFORMANCE COMPARISON.
//
bool FastWildCompare(char *pWild, char *pTame)
{
	char *pWildSequence;  // Points to prospective wild string match after '*'
	char *pTameSequence;  // Points to prospective tame string match

	// Find a first wildcard, if one exists, and the beginning of any  
	// prospectively matching sequence after it.
	do
	{
		// Check for the end from the start.  Get out fast, if possible.
		if (!*pTame)
		{
			if (*pWild)
			{
				while (*(pWild++) == '*')
				{
					if (!(*pWild))
					{
						return true;   // "ab" matches "ab*".
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
			while (*(++pWild) == '*')
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
				while (*pWild != *pTame)
				{
					if (!*(++pTame))
					{
						return false;  // "a*bc" doesn't match "ab".
					}
				}
			}

			// Keep fallback positions for retry in case of incomplete match.
			pWildSequence = pWild;
			pTameSequence = pTame;
			break;
		}
		else if (*pWild != *pTame && *pWild != '?')
		{
			return false;              // "abc" doesn't match "abd".
		}

		++pWild;                       // Everything's a match, so far.
		++pTame;
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
				while (*pWild != *pTame)
				{
					if (!*(++pTame))
					{
						return false;  // "a*b*c" doesn't match "ab".
					}
				}
			}

			// Keep the new fallback positions.
			pWildSequence = pWild;
			pTameSequence = pTame;
		}
		else if (*pWild != *pTame && *pWild != '?')
		{
			// The equivalent portion of the upper loop is really simple.
			if (!*pTame)
			{
				return false;          // "*bcd" doesn't match "abc".
			}

			// A fine time for questions.
			while (*pWildSequence == '?')
			{
				++pWildSequence;
				++pTameSequence;
			}

			pWild = pWildSequence;

			// Fall back, but never so far again.
			while (*pWild != *(++pTameSequence))
			{
				if (!*pTameSequence)
				{
					return false;      // "*a*b" doesn't match "ac".
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
				return false;          // "*bc" doesn't match "abcd".
			}
		}

		++pWild;                       // Everything's still a match.
		++pTame;
	} while (true);
}
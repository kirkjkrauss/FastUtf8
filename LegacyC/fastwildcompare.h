// UTF-8-ready functions for matching wildcards in C/C++.
int FastWildCompareUtf8(char *pWild, char *pTame);
int FastWildLenCompareUtf8(char *pWild, char *pTame, int lenWild, 
                            int lenTame);

// ASCII-only function for matching wildcards (2018).
int FastWildCompare(char *pWild, char *pTame);

// Given a null-terminated UTF-8 string, returns the number of code points 
// in it.  PERFORMS NO UTF-8 VALIDATION OTHER THAN NULL CHECKING.  Used 
// for testing FastWildLenCompareUtf8().
size_t CodePointCount(char *pContent);
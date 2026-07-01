# FastUtf8
This <a href="https://developforperformance.com/FastUtf8.html">package</a> provides fast UTF-8 handling for modern C++ with support for legacy C.

It includes a <a href="https://developforperformance.com/UniseriesGuide.html">FastUtf8::Uniseries C++ class</a> that lets you target UTF-8 for internationalization.  UTF-8 is all you need for working with for most Internet-based content, and targeting it can slash bloat associated with unnecessary encodings.  Included methods for <a href="https://developforperformance.com/FastUtf8.html#DeclarativeQueries">expressive queries against semistructured internationalized content</a> get outstanding performance <a href="https://developforperformance.com/FastUtf8.html#TokensetSearchAndTargetedWildcardSearchDemos">results</a> relative to prior methods.  A <a href="https://developforperformance.com/CDevelopersGuideUtf8.html">family of C-compatible functions</a> does the heavy lifting for FastUtf8 and can be called directly by legacy C code.


Within the FastUtf8 namespace you’ll find methods that do these operations:

<b>Setup / teardown</b>

A pair of <a href="https://developforperformance.com/UniseriesGuide.html#ParameterizedConstructors">parameterized constructors</a>, for uint8_t * or char * buffers, each of which makes a deep copy of the buffers’ existing content for use as a Uniseries object;

A <a href="https://developforperformance.com/UniseriesGuide.html#StandardConstructor">standard constructor</a>;

A <a href="https://developforperformance.com/UniseriesGuide.html#CopyConstructor">copy constructor</a> that creates a Uniseries object from the content buffer and metadata of an existing one;

A pair of <a href="https://developforperformance.com/UniseriesGuide.html#RangeBasedSliceConstructors">range-based slice constructors</a>, for uint8_t * or char * buffers, each of which creates a Uniseries object from a buffer designated by pFirst and pLast pointers;

A <a href="https://developforperformance.com/UniseriesGuide.html#Destructor">destructor</a>;

<a href="https://developforperformance.com/UniseriesGuide.html#UniseriesAssignmentOperators">Assignment operators</a> for uint8_t * and for char *;


<b>Slice & concatenate</b>

A <a href="https://developforperformance.com/UniseriesGuide.html#RangeBasedSliceMethods">slice()</a> constructor that creates a new Uniseries object from an existing one, making a deep copy of a portion of its content specified by iFirst and iLast code point indices;

A <a href="https://developforperformance.com/UniseriesGuide.html#fromSlice">fromSlice()</a> constructor, similar to slice() but pointer-based;

<a href="https://developforperformance.com/UniseriesGuide.html#UniseriesConcatenationOperators">Concatenation operators</a> for uint8_t *, for char *, and for Uniseries objects.


<b>Separate</b>

A set of pointer-based <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesContentSeparationMethods"pSeparate() content separation methods</a>, each of which constructs a Uniseries object from a portion of the existing object’s content based on a search for one or more tokens in a set;

A set of pointer-based <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesTokensetSearchMethods">pFindToken()</a> methods that also perform <a href="https://developforperformance.com/FastUtf8.html#TokensetSearch">tokenset search</a>;


<b>Access</b>

An <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesIterator">Iterator class</a> for working with individual code points;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#GetContentMethods">getContent()</a> methods;

A <a href="https://developforperformance.com/UniseriesGuide.html#getMetadataMethod">getMetadata()</a> method for direct access to a Uniseries object’s <a href="https://developforperformance.com/UniseriesGuide.html#Flags">flags</a> and length;


<b>Length check & case fold</b>

Methods for checking or modifiying the Uniseries object’s <a href="https://developforperformance.com/UniseriesGuide.html#MethodsForSettingCaseSensitivity">case sensitivity</a> and <a href="https://developforperformance.com/UniseriesGuide.html#MethodsForSettingTheLengthLimit">length-limited</a> behavior;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#getLengthMethods">getLength()</a> and <a href="https://developforperformance.com/UniseriesGuide.html#getSizeMethods"getSize()</a> methods that return a code point count or a size in bytes, respectively;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#getSizeFoldedMethods">getSizeFolded()</a> methods that precompute the size of a buffer needed to hold content after case folding;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#getFoldedMethods"getFolded()</a> methods that perform case folding;


<b>Evaluate & compare whole content</b>

An <a href="https://developforperformance.com/UniseriesGuide.html#Is7BitMethod">is7Bit() method</a> that indicates whether content is entirely ASCII text;

<a href="https://developforperformance.com/UniseriesGuide.html#EqualityOperators">Equality operators</a> and <a href="https://developforperformance.com/UniseriesGuide.html#caseCompareMethods">caseCompare() methods</a>;


<b>Basic full-text search</b>

A set of <a href="https://developforperformance.com/UniseriesGuide.html#containsMethodsForRawBufferContent">contains() partial content comparison methods</a> that return Boolean results;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#pFindMethodsForRawBufferContent">find() partial content comparison methods</a>, each of which can return the index of a chunk of content within a Uniseries buffer;

A similar set of <a href="https://developforperformance.com/UniseriesGuide.html#pFindMethodsForRawBufferContent">pFind() methods</a>, each of which returns a pointer instead of an index;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#caseContainsMethodsForRawBufferContent">caseContains() methods</a>, similar to contains() but case-insensitive;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#caseFindMethodsForRawBufferContent">caseFind()</a> and <a href="https://developforperformance.com/UniseriesGuide.html#casepFindMethodsForRawBufferContent">casepFind()</a> case-insensitive methods;


<b>Wildcard techniques</b>

A set of <a href="https://developforperformance.com/UniseriesGuide.html#wildCompareMethods">wildCompare()</a> and <a href="https://developforperformance.com/UniseriesGuide.html#compareWildMethods">compareWild()</a> methods for <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesMethodsForMatchingWildcards">matching wildcards</a>;

A set of <a href="https://developforperformance.com/UniseriesGuide.html#wildCaseCompareMethods">wildCaseCompare()</a> and <a href="https://developforperformance.com/UniseriesGuide.html#caseCompareWildMethods">caseCompareWild()</a> methods for case-insensitive matching;

A family of <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesMethodsForTargetedWildcardSearch">pFindWild()</a> and <a href="https://developforperformance.com/UniseriesGuide.html#CasepFindWildMethods">casepFindWild()</a> methods for <a href="https://developforperformance.com/FastUtf8.html#TargetedWildcardSearch">targeted wildcard search</a>;


<b>Other features</b>

<a href="https://developforperformance.com/UniseriesGuide.html#UniseriesSubscriptOperators">Subscript operators</a> with terrible performance;

A <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesTrimMethod">trim() method</a> that removes outboard white space;

A pair of <a href="https://developforperformance.com/UniseriesGuide.html#ValidateMethods">validate() methods</a>;

A pair of <a href="https://developforperformance.com/UniseriesGuide.html#Convert8BitAsciiMethods">methods for converting 8-bit ASCII text to UTF-8</a>; and

An <a href="https://developforperformance.com/UniseriesGuide.html#UniseriesOutputStreamOperator">output stream operator</a>.


Documentation is available at https://developforperformance.com including an <a href="https://developforperformance.com/FastUtf8.html">overview</a>, which introduces the <a href="https://developforperformance.com/FastUtf8.html#TargetedWildcardSearch">targeted wildcard search</a> and <a href="https://developforperformance.com/FastUtf8.html#TokensetSearch">tokenset search</a> techniques for <a href="https://developforperformance.com/FastUtf8.html#DeclarativeQueries">declarative functionality</a>, as well as a C++ developer’s guide and reference for the <a href="https://developforperformance.com/UniseriesGuide.htm">FastUtf8::Uniseries class</a>, plus a legacy C developer’s guide and reference for the <a href="https://developforperformance.com/CDevelopersGuideUtf8.html">*Utf8() family of functions</a> underlying the Uniseries class.

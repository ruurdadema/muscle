/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStringTokenizer_h
#define MuscleStringTokenizer_h

#include "support/MuscleSupport.h"
#include "util/Queue.h"
#include "util/String.h"

namespace muscle {

/** String tokenizer class, similar to Java's java.util.StringTokenizer.  This class is used
  * to interpret a specified character string as a series of sub-strings, with each sub-string
  * differentiated from its neighbors by the presence of one or more of the specified separator-tokens
  * between the two sub-strings.
  */
class StringTokenizer MUSCLE_FINAL_CLASS
{
public:
   /** Initializes the StringTokenizer to parse (tokenizeMe), which should be a string of tokens (e.g. words) separated by any
    *  of the characters specified in (separators)
    *  @param tokenizeMe the string to tokenize.  If NULL is passed in, an empty string ("") is assumed.
    *  @param hardSeparators ASCII string representing a list of characters to interpret as "hard" substring-separators.
    *                        Defaults to ","; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param softSeparators ASCII string representing a list of characters to interpret as "soft" substring-separators.
    *                        Defaults to " \t\r\n"; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param escapeChar If specified as non-zero, separator-chars appearing immediately after this char will not be used as separators.  Defaults to zero.
    *  @note the difference between a "soft" separator and a "hard" separator is that a contiguous series of soft separators
    *        is counted as a single separator, while a contiguous series of hard separators is counted as separating one or
    *        more empty strings.  For example, given the default separator-arguments listed above, ",A,B,,,C,D" would be tokenized
    *        as ("", "A", "B", "", "", "C", "D"), while "  A B  C   D  " would be tokenized as ("A", "B", "C", "D").
    */
   StringTokenizer(const char * tokenizeMe, const char * hardSeparators = ",", const char * softSeparators = " \t\r\n", char escapeChar = '\0');

   /** This Constructor is the same as above, only with this one you allow the StringTokenizer to modify (tokenizeMe) directly,
    *  rather than making a copy of the string first and modifying that.  (it's a bit more efficient as no dynamic allocations
    *  need to be done; however it does modify (tokenizeMe) in the process)
    *  @param junk Ignored; it's only here to disambiguate the two constructors.
    *  @param tokenizeMe The string to tokenize.  This string will get munged!
    *  @param hardSeparators ASCII string representing a list of characters to interpret as "hard" substring-separators.
    *                        Defaults to ","; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param softSeparators ASCII string representing a list of characters to interpret as "soft" substring-separators.
    *                        Defaults to " \t\r\n"; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param escapeChar If specified as non-zero, separator-chars appearing immediately after this char will not be used as separators.  Defaults to zero.
    *  @note the difference between a "soft" separator and a "hard" separator is that a contiguous series of soft separators
    *        is counted as a single separator, while a contiguous series of hard separators is counted as separating one or
    *        more empty strings.  For example, given the default separator-arguments listed above, ",A,B,,,C,D" would be tokenized
    *        as ("", "A", "B", "", "", "C", "D"), while "  A B  C   D  " would be tokenized as ("A", "B", "C", "D").
    */
   StringTokenizer(bool junk, char * tokenizeMe, const char * hardSeparators = ",", const char * softSeparators = " \t\r\n", char escapeChar = '\0');

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   StringTokenizer(const StringTokenizer & rhs);

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   StringTokenizer & operator = (const StringTokenizer & rhs);

   /** Destructor */
   ~StringTokenizer();

   /** Returns the next token in the parsed string, or NULL if there are no more tokens left */
   char * GetNextToken();

   /** Convenience synonym for GetNextToken() */
   char * operator()() {return GetNextToken();}

   /** Returns the remainder of the string, starting with the next token,
    *  or NULL if there are no more tokens in the string.
    *  Doesn't affect the next return value of GetNextToken(), though.
    */
   char * GetRemainderOfString();

   /** Returns the separator-escape character that was passed in to our constructor (or '\0' if none) */
   char GetEscapeChar() const {return _escapeChar;}

   /** Convenience method:  Returns a Queue containing all the remaining the tokenized substrings from this StringTokenizer.
    *  @param maxResults the maximum number of tokens to add to the returned Queue before returning.  Defaults to MUSCLE_NO_LIMIT.
    */
   Queue<String> Split(uint32 maxResults = MUSCLE_NO_LIMIT);

   /** Convenience method:  Joins the tokenizedStrings in the supplied list together with (joinChar), and returns the resulting String.
     * @param tokenizedStrings the list of sub-strings to join together
     * @param joinChar the separator-char to place between adjacent sub-strings
     * @param includeEmptyStrings if true, we'll include empty sub-strings in the resulting String (as two joinChar's in a row).  If false, we'll omit them.
     * @param escapeChar if non-zero, we'll use this escape-character to keep instances of (joinChar) within the tokenized substrings unambiguous.
     *                   Defaults to zero.
     */
   static String Join(const Queue<String> & tokenizedStrings, bool includeEmptyStrings, char joinChar, char escapeChar = '\0');

private:
   bool IsHardSeparatorChar(char prevChar, char c) const {return ((prevChar!=_escapeChar)&&(IsBitSet(_hardSepsBitChord, c)));}
   bool IsSoftSeparatorChar(char prevChar, char c) const {return ((prevChar!=_escapeChar)&&(IsBitSet(_softSepsBitChord, c)));}

   void DefaultInitialize();
   void DeletePrivateBufferIfNecessary();
   void CopyDataToPrivateBuffer(const StringTokenizer & copyFrom);
   void SetPointersAnalogousTo(char * myNewBuf, const StringTokenizer & copyFrom);
   void MovePastSoftSeparatorChars() {while((*_nextToRead)&&(IsSoftSeparatorChar(_prevChar, *_nextToRead))) Advance();}
   void Advance()
   {
      _prevChar = (_prevChar==_escapeChar)?(_escapeChar+1):(*_nextToRead);
      *_nextToWrite = *_nextToRead++;
      if (_prevChar != _escapeChar) _nextToWrite++;  // conditional because we don't want (_escapeChar) to appear in the string returned by GetNextToken()
   }

   bool IsBitSet(const uint32 * bits, uint8 whichBit) const {return ((bits[whichBit/32]&(1<<(whichBit%32))) != 0);}
   void SetBitChord(uint32 * bits, const char * seps);

   bool _allocedBufferOnHeap;
   bool _prevSepWasHard;
   char _escapeChar;
   char _prevChar;

   uint32 _bufLen;

   char * _tokenizeMe;   // pointer to the beginning of the string to tokenize
   char * _nextToRead;   // where our parser is reading from
   char * _nextToWrite;  // where our parser is writing to (the writing is necessary in to absorb escape-chars so they won't be seen by the calling code)

   char _smallStringBuf[128];

   uint32 _hardSepsBitChord[8];  // 32x8 = 256, aka all possible 8-bit values of a sep
   uint32 _softSepsBitChord[8];  // bit N is set iff the corresponding 8-bit value is a sep
};

} // end namespace muscle

#endif

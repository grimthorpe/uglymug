/** \file regexp_interface.h
 * Interface to regular expression stuff, to keep it in C with only one copy.
 */

#include "mudstring.h"
#include "config.h"

#include <pcre.h>

class RegularExpression
{
	pcre*		_compileresults;

public:
	RegularExpression(const String& pattern);
	~RegularExpression();

	bool	Match(const String& target);
};

// Use PCRE (Perl Compatible Regular Expressions)
inline RegularExpression::RegularExpression(const String& pattern)
{
	const char* errptr;
	int erroffset;

	_compileresults = pcre_compile(pattern.c_str(), PCRE_CASELESS, &errptr, &erroffset, NULL);
}

inline RegularExpression::~RegularExpression()
{
	if(_compileresults)
	{
		pcre_free(_compileresults);
		_compileresults = NULL;
	}
}

inline bool RegularExpression::Match(const String& target)
{
	if(!_compileresults)
		return false;

	if(pcre_exec(_compileresults, NULL, target.c_str(), (int)(target.length()), 0, 0, NULL, 0) < 0)
	{
		return false;
	}
	return true;
}


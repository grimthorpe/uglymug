/** \file regexp_interface.h
 * Interface to regular expression stuff, to keep it in C with only one copy.
 */

#include "mudstring.h"
#include "config.h"

#if defined REGEXP_PCRE
#include <pcre.h>
#else
// Old regexp stuff, as found in regexp.[ch]
extern char    *compile        (const char *, char *, char *, int);
extern int     step            (const char *, const char *);
#endif

class RegularExpression
{
#if defined REGEXP_PCRE
	pcre*		_compileresults;
#else
	char		_compileresults[BUFFER_LEN];
#endif

public:
	RegularExpression(const String& pattern);
	~RegularExpression();

	bool	Match(const String& target);
};

// Use PCRE (Perl Compatible Regular Expressions)
#if defined(REGEXP_PCRE)
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
#else
inline RegularExpression::RegularExpression(const String& regexp)
{
	compile(regexp.c_str(), _compileresults, _compileresults+sizeof(_compileresults), '\0');
}
inline RegularExpression::~RegularExpression()
{
}

inline bool RegularExpression::Match(const String& target)
{
	return step(target.c_str(), _compileresults);
}

#endif



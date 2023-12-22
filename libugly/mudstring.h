#include "copyright.h"
#ifndef _MUDSTRING_H
#define _MUDSTRING_H
/**********************************************************************
 * MUDSTRING
 *
 * An implementation of a managed string class.
 *
 * We can't use std::string because under Solaris (and other Sys-V UNIX)
 * the C library doesn't like NULL pointers, which have been used a lot
 * historically in UglyMUG code.
 *
 **********************************************************************/

#include <functional>
#include <iostream>
#include <stdarg.h>
#include <string>

class	String;

// Default NULL string.
extern String NULLSTRING;

/*
 * String
 *
 * A NULL-pointer safe string class that manages its own buffer.
 */
class	String : public std::string
{
	bool operator==(int) = delete;

public:
// PUBLIC DESTRUCTOR
	~String();

// PUBLIC CONSTRUCTORS
	String(const char* str = 0);	// Create a new String based on the data pointer to.
	String(const char* str, size_t len);
	String(const String& str);	// COPY constructor
	String(const char c, size_t repeat);
	String(const std::string& str);

// Assignment operators
	String& operator=(const String& cstr);
	String& operator=(const char* cstr);
	String& operator=(const char c);
	String& operator=(const std::string& str);

	//String& operator+=(const String& other);
	//String& operator+=(char c);

// Cheeky overload to remove 'amount' number of characters from the end.
	String& operator-=(size_t amount);

// __attribute__ ((format(printf, 2, 3))) is used to enfore printf-style format checking in gcc.
// NOTE: There is an implicit parameter of 'this', which is why it is parameter 2 that should be checked.
	String& printf(const char*, ...) __attribute__ ((format (printf, 2, 3)));
	String& vprintf(const char*, va_list) __attribute__ ((format (printf, 2, 0)));

	static String format(const char*, ...) __attribute__ ((format (printf, 1, 2)));

// operator bool - shorthand for checking if the string has data in it.
//	Returns: TRUE if there is data in the string
//		 FALSE if the string is empty
// NOTE: A 0-length string is the same as a 'NULL' string.
	operator bool()			const	{ return !empty(); }

	String	substring(size_t start)			const;
	String	substring(size_t start, ssize_t length)	const;
	ssize_t	find(char c, size_t start = 0)		const;
	char	operator[](size_t pos)			const;

	bool operator<(const String& other)	const;
	bool operator>(const String& other)	const;
	bool operator==(const String& other)	const;
	bool operator==(const char* other)	const;
	bool operator!=(const String& other)	const;
};

namespace std {
	template<> class less<String> {
	public:
		bool operator() (const String& s1, const String& s2) const { return s1 < s2; }
	};
};	

/** Output a string to a stream */
extern std::ostream &operator<< (std::ostream &os, const String &s);

/* Output a string to a maximum length (or pad to the length with spaces), taking colour markup into account */
String chop_string(const String&, size_t length);
String chop_string(const char*, size_t length);
#endif /* _MUDSTRING_H */

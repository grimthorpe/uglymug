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

class	String;

// Default NULL string.
extern String NULLSTRING;

/**
 * The real storage space for the String class.
 * The idea is that it is shared between several identical strings, and
 * reference counted so we know when to get rid of it.
 *
 * If a String wants to modify its contents, it should create a new
 * StringBuffer.
 * Note that this isn't enforced as it is possible that you may wish to
 * have a shared StringBuffer that updates all instances at the same time.
 */

class StringBuffer
{
friend class String;

	char*		_buf;		// Pointer to the data
	unsigned int	_len;		// The length of the string
	unsigned int	_capacity;	// The total allocated space
	int		_ref;		// The reference count.

// Private member functions.
// resize - Resize the storage space to at least 'newsize'.
//		If copy is true, copy over the data into the new buffer
//		If copy is false, the buffer contents are undefined.
	void resize(unsigned int newsize, bool copy=true);

// DESTRUCTOR - Private so that we enforce the reference counting.
	~StringBuffer();

// DUMMY FUNCTIONS - Defined so that C++ doesn't make them public and
//			create them by default. Dummy so that the class
//			can't use them internally.
	StringBuffer& operator=(const StringBuffer&);
	StringBuffer(const StringBuffer&);

// CONSTRUCTORS - Private so that we can share a common 'Empty' buffer
//			if required. Use NewBuffer to create a new StringBuffer
	StringBuffer(unsigned int capacity = 0);
	StringBuffer(const char* str);
	StringBuffer(const char* str, unsigned int len, unsigned int capacity = 0);

public:
// Public member functions

// NewBuffer - Create a new buffer.
	static StringBuffer* NewBuffer(const char*str, unsigned int len = 0);

// assign - Copy that data pointed to by str, up to 'len' characters.
//		The internal buffer will sort itself out to cope with it
	void assign(const char* str, int len);

// ref - Increase the reference count.
	void ref();
// unref - Decrease the reference count. If it hits 0, destroy the buffer.
	void unref();
// refcount - The current reference count.
	int		refcount()	const	{ return _ref; }

// c_str - Return a const pointer to the underlying buffer.
//		It is not guaranteed to be valid after any other operation.
	const char*	c_str()		const	{ return _buf?_buf:""; }

// length - Return the length of the data, not including the \0 terminator.
	unsigned int	length()	const	{ return _len; }

	void		append(const char*, unsigned int len);
	void		printf(const char*, va_list va);
};

/*
 * String
 *
 * A NULL-pointer safe string class that manages its own buffer.
 */
class	String
{
	StringBuffer*	_buffer;	// Pointer to the underlying buffer

// Private DUMMY members. Defined so that the main code can't use them,
// DUMMY so that we don't use them internally.
//	operator int() const;
//	bool operator==(const String& str);
	bool operator==(int);

// PRIVATE CONSTRUCTORS
	String(StringBuffer* buf);
public:
// PUBLIC DESTRUCTOR
	~String();

// PUBLIC CONSTRUCTORS
	String(const char* str = 0);	// Create a new String based on the data pointer to.
	String(const char* str, unsigned int len);
	String(const String& str);	// COPY constructor

// Assignment operators
	String& operator=(const String& cstr);
	String& operator=(const char* cstr);

	String& operator+=(const String& other);
	String& operator+=(char c);

// __attribute__ ((format(printf, 2, 3))) is used to enfore printf-style format checking in gcc.
// NOTE: There is an implicit parameter of 'this', which is why it is parameter 2 that should be checked.
	String& printf(const char*, ...) __attribute__ ((format (printf, 2, 3)));
	String& vprintf(const char*, va_list) __attribute__ ((format (printf, 2, 0)));
// Useful methods
// c_str - Return a pointer to the underlying character buffer.
//		Same guarantee as StringBuffer::c_str
	const char*	c_str()		const	{ return _buffer->c_str(); }
// length - Return the length of the string
	unsigned int	length()	const	{ return _buffer->length(); }

// operator bool - shorthand for checking if the string has data in it.
//	Returns: TRUE if there is data in the string
//		 FALSE if the string is empty
// NOTE: A 0-length string is the same as a 'NULL' string.
	operator bool()			const	{ return _buffer->length() > 0; }

	String substring(int start, int length = -1)	const;
	int	find(char c, int start = 0)		const;
	char	operator[](int pos)			const;

	bool operator<(const String& other)	const;
	bool operator>(const String& other)	const;
	bool operator==(const String& other)	const;
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
String chop_string(const String&, int length);
String chop_string(const char*, int length);
#endif /* _MUDSTRING_H */

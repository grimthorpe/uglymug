/********************************************************************
 *
 * MUDSTRING
 *
 * A managed string.
 *
 * Latest version: uses a shared buffer between strings to reduce the
 *                 work done when copying data.
 *
 * Implemented Feb-2002 by Grimthorpe.
 ********************************************************************/

#include <iostream>
#include <string.h>
#include "mudstring.h"
#include "copyright.h"
#include "externs.h"

String NULLSTRING;

String::~String()
{
}

// str is defaulted to 0 in the definition
String::String(const char* str)
{
	if(str)
		operator=(str);
}

String::String(const char* str, size_t len)
	: std::string(str, len)
{
}

String::String(const String& str)
{
	operator=(str);
}

String::String(const char c, size_t repeat)
	: std::string(c, repeat)
{
}

String::String(const std::string& str)
	: std::string(str)
{
}

String& String::operator=(const String& cstr)
{
	operator=(static_cast<const std::string&>(cstr));
	return *this;
}

String& String::operator=(const char* cstr)
{
	if(cstr)
		std::string::operator=(cstr);
	else
		clear();

	return *this;
}

String& String::operator=(const char c)
{
	clear();
	push_back(c);
	return *this;
}

String& String::operator=(const std::string& str)
{
	std::string::operator=(str);
	return *this;
}

String&
String::operator-=(size_t amount)
{
	if(amount >= length())
	{
		clear();
	}
	else
	{
		resize(length() - amount);
	}
	return *this;
}

bool
String::operator<(const String& other) const
{
	return string_compare(*this, other) < 0;
}

bool
String::operator>(const String& other) const
{
	return string_compare(*this, other) > 0;
}

bool
String::operator==(const String& other) const
{
	return string_compare(*this, other) == 0;
}

bool
String::operator==(const char* other) const
{
	return string_compare(*this, other) == 0;
}

bool
String::operator!=(const String& other) const
{
	return string_compare(*this, other) != 0;
}

std::ostream &
operator<< (
std::ostream &os,
const String &s)

{
	return os << static_cast<const std::string&>(s);
}

String
chop_string(const String& string, size_t size)
{
	return chop_string(string.c_str(), size);
}

String
chop_string(const char* string, size_t size)
{
	String retval;
	if(string != NULL)
	{
		const char* p = string;
		for(size_t i = 0; i < size; i++)
		{
			if(!*p)
				break;
			if(*p++ == '%')
			{
				size++;
				if(*p != '%')
				{
					size++;
					p++;
				}
			}
		}
	}

	retval.printf("%-*.*s", (int)size, (int)size, string);
	return retval;
}

String
String::substring(size_t start) const
{
	return substring(start, length()-start);
}

String
String::substring(size_t start, ssize_t len) const
{
	size_t totallength = length();
	if((len == 0) || (start >= totallength))
		return NULLSTRING;

	if(len < 0)
		len = (ssize_t)totallength-(ssize_t)start;

	return String(c_str() + start, (size_t)len);
}

ssize_t
String::find(char c, size_t start /* = 0 */) const
{
	size_t len = length();
	const char* ptr = c_str();
	for(size_t pos = start; pos < len; pos++)
	{
		if(ptr[pos] == c)
			return (ssize_t)pos;
	}
	return -1;
}

char
String::operator[](size_t pos) const
{
	if(pos < length())
		return c_str()[pos];
	return '\0';
}

String&
String::printf(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	this->vprintf(fmt, va);
	va_end(va);

	return *this;
}

String
String::format(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	String ret;
	ret.vprintf(fmt, va);
	va_end(va);

	return ret;
}

String&
String::vprintf(const char *fmt, va_list va)
{
	int size;

	va_list va2;
	va_copy(va2, va);
	size = vsnprintf(nullptr, 0, fmt, va2);
	va_end(va2);

	if(size > -1)
	{
		resize(size+1);
		vsnprintf(data(), size+1, fmt, va);
	}
	else
		clear();

	return *this;
}


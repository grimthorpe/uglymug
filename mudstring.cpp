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

// NewBuffer - static method that returns a StringBuffer.
// len is defaulted to 0 in the definition..
StringBuffer*
StringBuffer::NewBuffer(const char* ptr, size_t len, size_t capacity)
{
// Have a static data member; When NULLSTRING is destroyed, the 
// reference count on it should go to 0, and it will go too.
static StringBuffer* EmptyBuffer = new StringBuffer();

	if(capacity == 0)
	{
		if(!ptr || !*ptr)
		{
			// If there is no data to store, then return the default empty one.
			return EmptyBuffer;
		}
	}

	if(!len)
	{
		return new StringBuffer(ptr, 0, capacity);
	}
	return new StringBuffer(ptr, len, capacity);
}

String NULLSTRING;

#define STRINGBUFFER_GROWSIZE	64

// copy is defaulted to true in the definition
void StringBuffer::resize(size_t newsize, bool copy)
{
	if(newsize <= _capacity)
	{
		if(newsize < _len)
		{
			_len = newsize;
			_buf[_len] = 0;	// Null-terminate
		}
		return;
	}
	if((newsize - _capacity) < 32)
	{
		newsize += 32;
	}
	_capacity = newsize;
	if(!_buf)
	{
		_buf = (char*)malloc(_capacity + 2);
	}
	else
	{
		_buf = (char*)realloc(_buf, _capacity + 2);
	}
}
StringBuffer::~StringBuffer() // Private so that nobody can delete this. Use the reference counting!
{
	if(_buf)
		free(_buf);
	_buf = 0;
}
// capacity is defaulted to 0 in the definition 
StringBuffer::StringBuffer(size_t capacity) : _buf(0), _len(0), _capacity(0), _ref(0)
{
	resize(capacity, false);
}
StringBuffer::StringBuffer(const char* str) : _buf(0), _len(0), _capacity(0), _ref(0)
{
	if(str && *str)
	{
		assign(str, strlen(str));
	}
}
StringBuffer::StringBuffer(const char* str, size_t len, size_t capacity) : _buf(0), _len(0), _capacity(0), _ref(0)
{
	if(capacity > 0)
		resize(capacity, false);	// Make sure we've got enough space.
	if(str && *str)
	{
		if(len == 0)
			len=strlen(str);
		assign(str, len);
	}
}

void StringBuffer::assign(const char* str, size_t len)
{
	resize(len, false);
	if(str)
	{
		memcpy(_buf, str, len);
		_len = len;
	}
	else
	{
		_len = 0;
	}
	if(_buf)
		_buf[len] = 0; // NULL terminate.
}
void StringBuffer::ref()
{
	_ref++;
}
void StringBuffer::unref()
{
	if((--_ref) == 0)
	{
		delete const_cast<StringBuffer*>(this);
	}
}

void
StringBuffer::append(const char* other, size_t len)
{
	resize(_len+len, true);
	memcpy(_buf+_len, other, len);
	_len += len;
	_buf[_len] = 0;
}

void
StringBuffer::fill(const char c, size_t len)
{
	resize(len);
	memset(_buf, c, len);
}

String::~String()
{
	_buffer->unref();
}

// str is defaulted to 0 in the definition
String::String(const char* str) : _buffer(0)
{
	_buffer = StringBuffer::NewBuffer(str);
	_buffer->ref();
}

String::String(const char* str, size_t len) : _buffer(0)
{
	_buffer = StringBuffer::NewBuffer(str, len);
	_buffer->ref();
}

String::String(const String& str) : _buffer(0)
{
	if(str._buffer != NULL)
		_buffer=str._buffer;
	else
		_buffer = StringBuffer::NewBuffer(NULL);
	_buffer->ref();
}

String::String(StringBuffer* buf) : _buffer(0)
{
	_buffer = buf;
	_buffer->ref();
}

String::String(const char c, size_t repeat) : _buffer(0)
{
	_buffer=StringBuffer::NewBuffer(NULL, 0, repeat);
	_buffer->ref();
	_buffer->fill(c, repeat);
}

String& String::operator=(const String& cstr)
{
	StringBuffer* oldbuf=_buffer;

	_buffer=cstr._buffer;
	_buffer->ref();
	oldbuf->unref();

	return *this;
}

String& String::operator=(const char* cstr)
{
	if(_buffer->refcount () != 1)
	{
		_buffer->unref();
		_buffer = StringBuffer::NewBuffer(cstr);
		_buffer->ref();
	}
	else if(cstr)
	{
		_buffer->assign(cstr, strlen(cstr));
	}
	else
	{
		_buffer->assign(0, 0);
	}
	return *this;
}

String& String::operator=(const char c)
{
    char str[2];
    str[0] = c;
    str[1] = 0;
    *this = str;

    return *this;
}

String&
String::operator+=(const String& other)
{
	if(!other)
		return *this;

	if(!*this)
	{
		(*this) = other;
	}
	else
	{
		if(_buffer->refcount() > 1)
		{
			StringBuffer* tmp = new StringBuffer(_buffer->c_str(), _buffer->length(), _buffer->length() + other.length());
			_buffer->unref();
			_buffer = tmp;
			_buffer->ref();
		}
		_buffer->append(other.c_str(), other.length());
	}
	return *this;
}

String&
String::operator+=(char c)
{
	char tmp[2];
	tmp[0] = c;
	tmp[1] = 0;

	return (*this)+=String(tmp);
}

String&
String::operator-=(size_t amount)
{
	if(amount >= length())
	{
		*this = NULLSTRING;
		return *this;
	}
	if(_buffer->refcount() > 1)
	{
		StringBuffer* tmp = new StringBuffer(_buffer->c_str(), _buffer->length()-amount, _buffer->length()-amount);
		_buffer->unref();
		_buffer = tmp;
		_buffer->ref();
	}
	else
	{
		_buffer->resize(_buffer->length()-amount);
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
String::operator!=(const String& other) const
{
	return string_compare(*this, other) != 0;
}

std::ostream &
operator<< (
std::ostream &os,
const String &s)

{
	return os << s.c_str ();
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
String::substring(size_t start, size_t len) const
{
	size_t totallength = length();
	if((len == 0) || (start >= totallength))
		return NULLSTRING;

	if(len < 0)
		len = totallength-start;

	return String(c_str() + start, len);
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

String&
String::vprintf(const char *fmt, va_list va)
{
	if(_buffer->refcount() > 1)
	{
		_buffer->unref();
		_buffer = new StringBuffer(512);
		_buffer->ref();
	}

	_buffer->printf(fmt, va);
	return *this;
}

void
StringBuffer::printf(const char *fmt, va_list va)
{
	int size;

	for(int i=0; i < 2; i++)
	{
		va_list va2;
		va_copy(va2, va);
		size = vsnprintf(_buf, _capacity, fmt, va2);
		va_end(va2);
		if(size > -1)
		{
			if((size_t)size < _capacity)
			{
				_len = (size_t)size;
				return;
			}
			else
				resize(size + 1, false);
		}
		else
		{
			// SUSv2 fix - set _capacity to > 0
			resize(_capacity + 512, false);
		}
	}
	// If we've not returned before now then we've tried
	// too many times.
	_len=0;
}


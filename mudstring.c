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

#include "mudstring.h"
#include "copyright.h"
#include <time.h>
#include <string.h>

// NewBuffer - static method that returns a StringBuffer.
StringBuffer*
StringBuffer::NewBuffer(const char* ptr, unsigned int len = 0)
{
// Have a static data member; When NULLSTRING is destroyed, the 
// reference count on it should go to 0, and it will go too.
static StringBuffer* EmptyBuffer = new StringBuffer();

	if(!ptr || !*ptr)
	{
		// If there is no data to store, then return the default empty one.
		return EmptyBuffer;
	}

	if(!len)
	{
		return new StringBuffer(ptr);
	}
	return new StringBuffer(ptr, len);
}

String NULLSTRING;

#define STRINGBUFFER_GROWSIZE	64

void StringBuffer::resize(unsigned int newsize, bool copy=true)
{
	if(newsize <= _capacity)
		return;
	if((newsize - _capacity) < 32)
	{
		newsize += 32;
	}
	_capacity = newsize;
	//while(newsize > _capacity)
	//	_capacity += STRINGBUFFER_GROWSIZE;
	char* tmp = new char[_capacity+2]; // Allow for slight overrun (JIC)
	if(_buf && copy)
	{
		// We use memcpy because the regions will not overlap.
		memcpy(tmp, _buf, _len);
		tmp[_len] = 0;
	}
	if(_buf)
		delete[] _buf;
	_buf = tmp;
}
StringBuffer::~StringBuffer() // Private so that nobody can delete this. Use the reference counting!
{
	if(_buf)
		delete[] _buf;
}

StringBuffer::StringBuffer(unsigned int capacity = 0) : _buf(0), _len(0), _capacity(0), _ref(0)
{
	resize(capacity);
}
StringBuffer::StringBuffer(const char* str) : _buf(0), _len(0), _capacity(0), _ref(0)
{
	if(str && *str)
	{
		assign(str, strlen(str));
	}
}
StringBuffer::StringBuffer(const char* str, unsigned int len, unsigned int capacity) : _buf(0), _len(0), _capacity(0), _ref(0)
{
	resize(capacity, false);	// Make sure we've got enough space.
	if(str && *str)
	{
		assign(str, len);
	}
}

void StringBuffer::assign(const char* str, int len)
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
	if(this)
		_ref++;
}
void StringBuffer::unref()
{
	if((this) && ((--_ref) == 0))
	{
		//if(this != &EMPTYBUFFER)
			delete const_cast<StringBuffer*>(this);
	}
}

void
StringBuffer::append(const char* other, unsigned int len)
{
	resize(_len+len, true);
	memcpy(_buf+_len, other, len);
	_len += len;
	_buf[_len] = 0;
}


String::~String()
{
	_buffer->unref();
}

String::String(const char* str = 0) : _buffer(0)
{
	_buffer = StringBuffer::NewBuffer(str);
	_buffer->ref();
}

String::String(const String& str) : _buffer(0)
{
	_buffer=str._buffer;
	_buffer->ref();
}

String::String(StringBuffer* buf) : _buffer(0)
{
	_buffer = buf;
	_buffer->ref();
}

String& String::operator=(const String& cstr)
{
	if(cstr._buffer != _buffer)
	{
		_buffer->unref();
		_buffer = cstr._buffer;
		_buffer->ref();
	}
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


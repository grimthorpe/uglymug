#include "mudstring.h"
#include <time.h>
#ifndef OLD_STRING

StringBuffer StringBuffer::EMPTYBUFFER;

StringBuffer*
StringBuffer::NewBuffer(const char* ptr, unsigned int len = 0)
{
	if(!ptr || !*ptr)
	{
		return &EMPTYBUFFER;
	}

	if(!len)
	{
		return new StringBuffer(ptr);
	}
	return new StringBuffer(ptr, len);
}

String NULLSTRING;
CString NULLCSTRING;

#ifdef CSTRING
void CString::empty()
{
	buf = 0;
	len = 0;
}
CString::CString()
{
	empty();
}
CString::CString(const char* str)
{
	if(str && *str)
	{
		buf = str;
		len = strlen(buf);
	}
	else
	{
		empty();
	}
}
CString::CString(const CString& str)
{
	buf = str.buf;
	len = str.len;
}
CString::~CString() {}

CString& CString::operator=(const CString& str)
{
	buf = str.buf;
	len = str.len;
	return *this;
}
CString& CString::operator=(const char* str)
{
	if(str && *str)
	{
		buf = str;
		len = strlen(str);
	}
	else
	{
		empty();
	}
	return *this;
}

// Strict equivalence
bool CString::operator==(const CString& str) const
{
	if(str.buf == buf)
	{
		return true;
	}
	return false;
}
CString::CString(const String& str)
{
	if(str)
	{
		buf = str.c_str();
		len = str.length();
	}
	else
	{
		empty();
	}
}
CString& CString::operator=(const String& str)
{
	if(str)
	{
		buf = str.c_str();
		len = str.length();
	}
	else
	{
		empty();
	}
	return *this;
}

#endif //CSTRING

#define STRINGBUFFER_GROWSIZE	64

void StringBuffer::init()
{
	_capacity = 0;
	_len = 0;
	_buf = 0;
	_ref = 0;
}
void StringBuffer::resize(unsigned int newsize, bool copy=true)
{
	if(newsize <= _capacity)
		return;
	_capacity = newsize;
	//while(newsize > _capacity)
	//	_capacity += STRINGBUFFER_GROWSIZE;
	char* tmp = (char*)malloc(_capacity+2); // Allow for slight overrun (JIC)
	if(_buf && copy)
	{
		// We use memcpy because the regions will not overlap.
		memcpy(tmp, _buf, _len);
		tmp[_len] = 0;
	}
	if(_buf)
		free(_buf);
	_buf = tmp;
}
StringBuffer::~StringBuffer() // Private so that nobody can delete this. Use the reference counting!
{
	if(_buf)
		free(_buf);
}

StringBuffer::StringBuffer(unsigned int capacity = 0)
{
	init();
	resize(capacity);
}
StringBuffer::StringBuffer(const char* str)
{
	init();
	if(str && *str)
	{
		assign(str, strlen(str));
	}
}
StringBuffer::StringBuffer(const char* str, unsigned int len)
{
	init();
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
		delete const_cast<StringBuffer*>(this);
	}
}

String::~String()
{
	_buffer->unref();
}
String::String(const char* str = 0)
{
	_buffer = StringBuffer::NewBuffer(str);
	_buffer->ref();
}
String::String(const String& str)
{
	_buffer=str._buffer;
	_buffer->ref();
}
String::String(StringBuffer* buf)
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
#ifdef CSTRING
String::String(const CString& str)
{
	_buffer = StringBuffer::NewBuffer(str.c_str(), str.length());
	_buffer->ref();
}
String& String::operator=(const CString& cstr)
{
	if(_buffer->refcount () != 1)
	{
		_buffer->unref();
		_buffer = StringBuffer::NewBuffer(cstr.c_str(), cstr.length());
		_buffer->ref();
	}
	else
		_buffer->assign(cstr.c_str(), cstr.length());
	return *this;
}
#endif
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

#endif

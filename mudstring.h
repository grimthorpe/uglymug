#include "copyright.h"
#ifndef _MUDSTRING_H
#define _MUDSTRING_H
//#pragma interface
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRINGBUFFER_GROWSIZE	256
class	String;
class	CString;

class	CString
{
private:
	const char* buf;
	unsigned int len;

		operator int()	const;
		bool operator==(const CString& str);
		bool operator==(int);
	void empty()
	{
		buf = 0;
		len = 0;
	}
public:
	CString()
	{
		empty();
	}
	CString(const char* str)
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
	CString(const CString& str)
	{
		buf = str.buf;
		len = str.len;
	}
	CString(const String& str);
	~CString() {}

	CString& operator=(const CString& str)
	{
		buf = str.buf;
		len = str.len;
		return *this;
	}
	CString& operator=(const char* str)
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
	CString& operator=(const String& str);

		operator bool()		const { return len != 0; }
	const char*	c_str()		const { return buf?buf:""; }
	unsigned int	length()	const { return len; }

	// Strict equivalence
	bool operator==(const CString& str) const
	{
		if(str.buf == buf)
		{
			return true;
		}
		return false;
	}
};

class	String
{
public:
	class Buffer
	{
		friend	class	::String;	// Doesn't have to be a friend, but it stops
						// gcc from complaining.

		int		_ref;
		char*		_buf;
		unsigned int	_len;
		unsigned int	_capacity;

		void init()
		{
			_capacity = 0;
			_len = 0;
			_buf = 0;
			_ref = 0;
		}
		void resize(unsigned int newsize, bool copy=true)
		{
			if(newsize < _capacity)
				return;
			newsize++; // The newsize doesn't include space for the terminator.
			while(newsize >= _capacity)
				_capacity += STRINGBUFFER_GROWSIZE;
			char* tmp = new char[_capacity];
			if(_buf && copy)
			{
				// We use memcpy because the regions will not overlap.
				memcpy(tmp, _buf, _len); // Copy the \0 terminator
				delete[] _buf;
				tmp[_len+1] = 0;
			}
			_buf = tmp;
		}
		~Buffer() // Private so that nobody can delete this. Use the reference counting!
		{
			delete[] _buf;
		}
		Buffer& operator=(const Buffer&);
	public:
		void assign(const char* str, int len)
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
			_buf[len] = 0; // NULL terminate.
		}
		Buffer(unsigned int capacity = 0)
		{
			init();
			resize(capacity);
		}
		Buffer(const char* str)
		{
			init();
			if(str && *str)
			{
				assign(str, strlen(str));
			}
		}
		Buffer(const char* str, unsigned int len)
		{
			init();
			if(str && *str)
			{
				assign(str, len);
			}
		}
		Buffer(const Buffer& b)
		{
			init();
			assign(b._buf, b._len);
		}
		void ref()
		{
			if(this)
				_ref++;
		}
		void unref()
		{
			if((this) && ((--_ref) == 0))
			{
				delete const_cast<Buffer*>(this);
			}
		}
		int		refcount()	const	{ return _ref; }
		const char*	c_str()		const	{ return _buf?_buf:""; }
		unsigned int	length()	const	{ return _len; }
	};

	Buffer*	_buffer;

	operator int() const;
	bool operator==(const String& str);
	bool operator==(int);
public:
	~String()
	{
		if(_buffer)
			_buffer->unref();
	}
	String()
	{
		_buffer = new Buffer();
		_buffer->ref();
	}
	String(const char* str)
	{
		_buffer = new Buffer(str);
		_buffer->ref();
	}
	String(const String& str)
	{
		_buffer=str._buffer;
		_buffer->ref();
	}
	String(const CString& str)
	{
		_buffer = new Buffer(str.c_str(), str.length());
		_buffer->ref();
	}
	String& operator=(const String& cstr)
	{
		if(cstr._buffer != _buffer)
		{
			_buffer->unref();
			_buffer = cstr._buffer;
			_buffer->ref();
		}
		return *this;
	}
	String& operator=(const CString& cstr)
	{
		if(_buffer->refcount () != 1)
		{
			_buffer->unref();
			_buffer = new Buffer(cstr.c_str(), cstr.length());
			_buffer->ref();
		}
		else
			_buffer->assign(cstr.c_str(), cstr.length());
		return *this;
	}
	String& operator=(const char* cstr)
	{
		if(_buffer->refcount () != 1)
		{
			_buffer->unref();
			_buffer = new Buffer(cstr);
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
	const char*	c_str()		const	{ return _buffer->c_str(); }
	unsigned int	length()	const	{ return _buffer->length(); }
	operator bool()			const	{ return _buffer->length() > 0; }
};

inline CString::CString(const String& str)
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
inline CString& CString::operator=(const String& str)
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

extern String NULLSTRING;
extern CString NULLCSTRING;

#endif /* _MUDSTRING_H */

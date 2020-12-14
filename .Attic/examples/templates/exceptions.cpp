#include <cstddef>

extern "C" {
	int puts( const char* str);
	void* malloc( std::size_t nn);
	void free( char* ptr);
	void memcpy( char* dest, const char* str, std::size_t nn);
	std::size_t strlen( const char* str);
}


class Exception
{
public:
	Exception( int errcode_, const char* msg_)
		:m_errcode(errcode_)
	{
		std::size_t nn = ::strlen(msg_);
		if (nn >= MsgBufSize) nn = MsgBufSize-1;
		::memcpy( m_msg, msg_, nn);
		m_msg[ nn] = 0;
	}

	const char* what() const {return m_msg;}

private:
	enum {MsgBufSize = (1<<12) - sizeof( int)};
	int m_errcode;
	char m_msg[ MsgBufSize];
};


class String
{
public:
	String( const char* ptr_)
	{
		init( ptr_);
	}
	String( const String& o)
	{
		init( o.m_ptr);
	}
	~String()
	{
		::free( m_ptr);
	}

	const char* c_str() const noexcept	{return m_ptr;}
	std::size_t size() const noexcept	{return m_len;}

private:
	void init( const char* ptr_)
	{
		std::size_t nn = ::strlen( ptr_);
		m_ptr = (char*)::malloc( nn+1);
		if (!m_ptr) throw Exception( 12/*ENOMEM*/, "Bad alloc");
		::memcpy( m_ptr, ptr_, nn+1);
	}

private:
	char* m_ptr;
	std::size_t m_len;
};

class StringPair
{
public:
	StringPair( const String& _1, const String& _2)
		:m_1(_1),m_2(_2){}
	~StringPair(){}

private:
	String m_1;
	String m_2;
};


int main( int argc, char const* argv[])
{
	try {
		StringPair( "blabla", "hallygally");
	} catch (const Exception& e) {
		return 1;
	}
	return 0;
}



// Testing exception handling

extern "C" procedure printf( const byte^ fmt ...);
extern "C" procedure putchar( const byte);
extern "C" function malloc byte^( long);
extern "C" procedure free( byte^);
extern "C" function memcpy byte^( byte^ dest, const byte^ src, long n);
extern "C" function strlen long( const byte^ str);

int g_allocCnt = 0;
int g_maxAllocCnt = 0;
long g_exceptionCode = 0;

private function allocmem byte^( long bytes)
{
	g_allocCnt += 1;
	if (g_allocCnt == g_maxAllocCnt)
	{
		printf( "Throw: %s\n", "Max allowed allocations reached");
		throw 201, "Max allowed allocations reached";
	}
	var byte^ rt = malloc( bytes);
	if (rt == null)
	{
		printf( "Throw: %s\n", "Out of memory");
		throw 12, "Out of memory";
	}
	return rt;
}
private procedure freemem( byte^ ptr) nothrow
{
	free( ptr);
}


namespace std {
class String
{
	public constructor() nothrow
	{
		printf( "Default constructor string\n");
		m_ptr = null;
		m_size = 0;
	}
	public constructor( const byte^ cstr_)
	{
		var int size_ = strlen( cstr_);
		m_ptr = allocmem( size_ + 1);
		m_size = size_;
		memcpy( m_ptr, cstr_, size_);
		m_ptr[ size_] = 0;
		printf( "Constructor string [%s]\n", c_str());
	}
	destructor
	{
		printf( "Destructor string [%s]\n", c_str());
		freemem( m_ptr);
	}
	public procedure append( const byte^ cstr_, int size_)
	{
		var byte^ ptr_ = allocmem( m_size + size_ + 1);
		memcpy( ptr_, m_ptr, m_size);
		memcpy( ptr_ + m_size, cstr_, size_);
		ptr_[ m_size + size_] = 0;
		freemem( m_ptr);
		m_ptr = ptr_;
		m_size += size_;
	}
	public operator + String( const String o) const
	{
		var String rt;
		var byte^ ptr_ = allocmem( m_size + o.m_size + 1);
		memcpy( ptr_, m_ptr, m_size);
		memcpy( ptr_ + m_size, o.m_ptr, o.m_size);
		ptr_[ m_size + o.m_size] = 0;
		rt.m_ptr = ptr_;
		rt.m_size = m_size + o.m_size;
		return rt;
	}
	public function c_str const byte^() const nothrow
	{
		printf( "Get string\n");
		if (m_ptr != null)
		{
			return m_ptr;
		}
		else
		{
			return "";
		}		
	}
	byte^ m_ptr;
	int m_size;
}
}

private procedure test( int allocCnt) nothrow
{
	g_maxAllocCnt = allocCnt;
	g_allocCnt = 0;
	try
	{
		var std::String str = "Hello world!";
		printf( "DEBUG\n");
		var std::String[20] ar = {"H","e","l","l","o"," ","w","o","r","l","d!"};
		printf( "%s\n", str.c_str());
		{
			var std::String greeting = (cast std::String: "Hello")
				+ (cast std::String: " ")
				+ (cast std::String: "World!");
			printf( "GREETING %s\n", greeting.c_str());
		}
	}
	catch errcode, errmsg
	{
		printf( "ERR %ld %ld %s\n", g_exceptionCode, errcode, errmsg);
	}
}

main
{
	test( 100);
	test( 1);
}




extern "C" procedure printf( const byte^ fmt ...);
extern "C" procedure putchar( const byte);
extern "C" function malloc byte^( long);
extern "C" procedure free( byte^);
extern "C" function memcpy byte^( byte^ dest, const byte^ src, long n);
extern "C" function strlen long( const byte^ str);

int g_allocCnt = 0;
int g_maxAllocCnt = 0;

private function allocmem byte^( long bytes)
{
	g_allocCnt += 1;
	if (g_allocCnt == g_maxAllocCnt)
	{
		throw 201, "Max allowed allocations reached";
	}
	var byte^ rt = malloc( bytes);
	if (rt == null)
	{
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
	public constructor( const byte^ cstr_)
	{
		var int size_ = strlen( cstr_);
		m_ptr = allocmem( size_ + 1);
		m_size = size_;
		memcpy( m_ptr, cstr_, size_);
		m_ptr[ size_] = 0;
	}
	destructor
	{
		freemem( m_ptr);
	}
	public function c_str const byte^() const
	{
		return m_ptr;
	}
	byte^ m_ptr;
	int m_size;
}
}

main {
	var std::String str = "Hello world!";
	printf( str.c_str());
}




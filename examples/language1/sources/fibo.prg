// Testing free functions, local functions, calculating the 20th fibonacci number

extern "C" procedure printf( const byte^ fmt, int arg);

private function fibonacci int( int n) nothrow
{
	procedure swap( int& a, int& b) nothrow
	{
		var int tmp = a;
		a = b;
		b = tmp;
	}
	var int i1 = 0;
        var int i2 = 1;
        var int i = 0;
	while (i < n)
	{
		i1 = i2 + i1;
		swap( i1, i2);
		i += 1;
	}
	return i2;
}

main
{
	var int val = fibonacci( 20);
	printf( "Result: %d\n", val);
}


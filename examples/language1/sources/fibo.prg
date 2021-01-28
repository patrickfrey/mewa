extern "C" procedure printf( const byte^, int);

private function fibonacci int( int n)
{
	procedure swap( int& a, int& b)
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


procedure swap( int& a, int& b)
{
        var int tmp = a;
	a = b;
	b = tmp;
}

function int fibonacci( int n)
{
        var int i1 = 0;
        var int i2 = 1;
	while (i < n)
	{
		i1 = i2 + i1;
		swap( i1,i2);
	}
	return i2;
}


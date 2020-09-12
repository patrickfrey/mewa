function swap( int& a, int& b)
{
	int tmp = a;
	a = b;
	b = tmp;
}

function fibonacci( int n)
{
	int i1 = 0;
	int i2 = 1;
	while (i < n)
	{
		i1 = i2 + i1;
		swap( i1,i2);
	}
	return i2;
}


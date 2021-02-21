// Testing simple array construction and access

extern "C" procedure printf( const byte^ fmt ...);

class IntArray
{
	constructor(){}
	constructor( const int[ 5]& ar_)
	{
		m_ar = ar_;
	}
	public operator [] int& (uint idx)
	{
		return m_ar[ idx];
	}
	public operator [] const int& (uint idx) const
	{
		return m_ar[ idx];
	}

	int[5] m_ar;
}

class FloatArray
{
	constructor(){}
	constructor( const double[ 5]& ar_)
	{
		m_ar = ar_;
	}
	public operator [] double& (uint idx)
	{
		return m_ar[ idx];
	}
	public operator [] const double& (uint idx) const
	{
		return m_ar[ idx];
	}
	double[5] m_ar;
}

procedure printIntArray()
{
	var IntArray array = { 911, 822, 733, 644, 555 };
	var int i = 0;
	while (i<5) {
		printf("ARRAY[ %d] = %d\n", i, array[ i]);
		i += 1;
	}
}

procedure printFloatArray()
{
	var double a = 3.2;
	var double b = a;
	printf("FL = %f %f\n", a, b);

	var FloatArray array = { 9.11, 8.22, 7.33, 6.44, 5.55 };
	var int i = 0;
	while (i<5) {
		printf("ARRAY[ %d] = %f\n", i, array[ i]);
		i += 1;
	}
}

main {
	printIntArray();
	printFloatArray();
}


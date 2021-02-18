// Testing simple array construction and access

extern "C" procedure printf( const byte^ fmt ...);

class Array
{
	constructor(){}
	constructor( const float& ar_[ 5])
	{
		m_ar = ar_;
	}

	public operator [] float& (uint idx)
	{
		return m_ar[ idx];
	}
	public operator [] const float& (uint idx) const
	{
		return m_ar[ idx];
	}

	float m_ar[ 5];
}

main {
	var Array array = {1,2,3,4,5};
	array[ 1] = 42;
	array[ 2] = -1.9;
	array[ 3] = 3.124;

	var int i=0; 
	while (i<5) {
		printf("ARRAY[ %d] = %d\n", i, array[i]);
		i += 1;
	}
}


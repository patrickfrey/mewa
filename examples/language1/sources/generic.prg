// Testing simple generic classes, structures and functions

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function malloc byte^( long);

generic class Array<ELEMENT,SIZE>
{
	constructor(){}

	public operator [] ELEMENT& (uint idx)
	{
		return m_ar[ idx];
	}
	public operator [] const ELEMENT& (uint idx) const
	{
		return m_ar[ idx];
	}
	ELEMENT[ SIZE] m_ar;
}

main {
	var Array<float,5> array;
	array[ 1] = 42;
	array[ 2] = -1.9;
	array[ 3] = 3.124;

	var int i=0; 
	while (i<5) {
		printf("ARRAY[ %d] = %d\n", i, array[i]);
		i += 1;
	}
}


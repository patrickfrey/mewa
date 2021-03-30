// Testing simple generic classes, structures and functions

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function malloc byte^( long);

generic class Array[ELEMENT,SIZE]
{
	constructor() nothrow{}
	constructor( const ELEMENT[ SIZE]& ar_) nothrow
	{
		m_ar = ar_;
	}
	public operator [ ] ELEMENT& (uint idx) nothrow
	{
		return m_ar[ idx];
	}
	public operator [ ] const ELEMENT& (uint idx) const nothrow
	{
		return m_ar[ idx];
	}
	ELEMENT[ SIZE] m_ar;
}

main {
	var Array[float,5] array = {1.1,2.2,3.3,4.4,5.5};

	array[ 1] = -11.11;
	array[ 3] = -33.33;

	var int i=0; 
	while (i<5) {
		printf("ARRAY[ %d] = %.4f\n", i, array[i]);
		i += 1;
	}
}


// Testing control structures (control boolean types)

extern "C" procedure printf( const byte^ fmt ...);

main
{
	var int ii = 0;
	while (ii < 5)
	{
		if (ii == 0 || ii == 1 || ii == 2 || ii == 3)
		{
			printf( "Value is smaller than %d\n", 4);
		}
		else
		{
			printf( "Value is bigger than %d\n", 4);
		}
		ii += 1;
	}
}

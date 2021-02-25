// Testing different types of pointers, access via poitners and pointer arithmetics

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function malloc byte^( long);

procedure printNumber( double aa, int& cnt)
{
	printf("CALL printNumber %f\n", aa);
	cnt += 1;
}

function printNumber int( double aa, int cnt)
{
	printf("CALL printNumber %f\n", aa);
	return cnt + 1;
}

typedef procedure PNP( double aa, int& cnt);

PNP g_pnp = printNumber;

main {
	typedef function PNF int( double aa, int cnt);

	var PNP pnp1 = printNumber;
	var PNF pnf1 = printNumber;
	var int cnt = 31;

	// cnt += 2 * pnp1( 3.11, 3);
	// pnf1( 7.22, cnt);
	// var int res = g_pnp( 19.1, 2);

	printf("RES cnt = %d\n", cnt);
	// printf("RES res = %d\n", res);	
}


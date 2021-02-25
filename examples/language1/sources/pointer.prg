// Testing different types of pointers, access via poitners and pointer arithmetics

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function malloc byte^( long);

procedure printNumber( double aa, int& cnt)
{
	printf("CALL printNumber [%d] %f\n", cnt, aa);
	cnt += 1;
}

function printNumber int( double aa, int cnt)
{
	printf("CALL printNumber [%d] %f\n", cnt, aa);
	return cnt + 1;
}

typedef procedure PNP( double aa, int& cnt);

PNP g_pnp = printNumber;

main {
	typedef function PNF int( double aa, int cnt);

	var PNP pnp1 = printNumber;
	var PNF pnf1 = printNumber;
	var int cnt = 31;

	pnp1( 3.11, cnt);
	cnt += 2 * pnf1( 4.22, 4);
	pnf1( 7.22, cnt);
	g_pnp( 19.1, cnt);

	printf("RES cnt = %d\n", cnt);
}


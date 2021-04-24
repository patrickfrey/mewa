// Testing different types of pointers, access via pointers and pointer arithmetics

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function malloc byte^( long);

procedure printNumber( double aa, int& cnt) nothrow
{
	printf("CALL printNumber [%d] %f\n", cnt, aa);
	cnt += 1;
}

function printNumber int( double aa, int cnt) nothrow
{
	printf("CALL printNumber [%d] %f\n", cnt, aa);
	return cnt + 1;
}

typedef procedure PNP( double aa, int& cnt) nothrow;

PNP g_pnp = printNumber;

main {
	typedef function PNF int( double aa, int cnt) nothrow;

	var PNP pnp1 = printNumber;
	var PNF pnf1 = printNumber;
	var int cnt = 31;
	var const int num = 23;

	pnp1( 3.11, cnt);
	cnt += 2 * pnf1( 4.22, 4);
	pnf1( 7.22, cnt);
	g_pnp( 19.1, cnt);

	printf("RES cnt = %d\n", cnt);
	var const byte^ str = "Hello World!";
	var const byte^ ptr = str + 6;
	var const byte^ bas = ptr - 5;
	var byte fc = str[0];
	var byte sc = str[6];
	printf("Initials %c %c %s %s\n", fc, sc, ptr, bas);
	var int^ cntptr = &cnt;
	var const int^ numptr = &num;
	printf("PTR &cnt -> %d\n", *cntptr);
	printf("PTR &num -> %d\n", *numptr);
}


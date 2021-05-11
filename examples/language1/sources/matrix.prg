// Testing matrix implementation using generics and lambdas

extern "C" procedure printf( const byte^ fmt ...);

double epsilon = 1.11e-16;

function sgn int( double val) nothrow
{
	if (val >= epsilon)
	{
		return 1;
	}
	elseif (val <= -epsilon)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

generic class Matrix[R,DIM1,DIM2]
{
	constructor() nothrow
	{
                m_ar = {};
        }
	constructor( const R[DIM2][DIM1] ar_) nothrow
	{
		m_ar = ar_;
	}
	constructor( const Matrix[R,DIM1,DIM2] o) nothrow
	{
		m_ar = o.m_ar;
	}
	generic private function summate[TYPE] TYPE() const nothrow
	{
		var TYPE rt = 0;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                rt += m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public function summate_int int() const nothrow
	{
		return summate[ int]();
	}
	generic private function elementwise[LAMBDA] Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const nothrow
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                LAMBDA[ rt.m_ar[ ii][ jj], m_ar[ ii][ jj], o.m_ar[ ii][ jj]];
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	generic private function elementwise_scalar[LAMBDA] Matrix[R,DIM1,DIM2]( const R scalar) const nothrow
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                LAMBDA[ rt.m_ar[ ii][ jj], m_ar[ ii][ jj], scalar];
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public operator + Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const nothrow
	{
		return elementwise[ lambda(a,b,c) {a=b+c;}]( o);
	}
	public operator - Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const nothrow
	{
		return elementwise[ lambda(a,b,c) {a=b-c;}]( o);
	}
	public operator - Matrix[R,DIM1,DIM2]() const nothrow
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                rt.m_ar[ ii][ jj] = -m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public operator * Matrix[R,DIM1,DIM1]( const Matrix[R,DIM2,DIM1] o) const nothrow
	{
                var Matrix[R,DIM1,DIM1] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM1)
                        {
                                var double sum = 0;
                                var int kk = 0;
                                while (kk < DIM2)
                                {
                                        sum += m_ar[ ii][ kk] * o.m_ar[ kk][ jj];
                                        kk += 1;
                                }
                                rt.m_ar[ ii][ jj] = sum;
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public operator * Matrix[R,DIM1,DIM2]( const R scalar) const nothrow
	{
		return elementwise_scalar[ lambda(a,b,v) {a=b*v;}]( scalar);
	}
	public operator / Matrix[R,DIM1,DIM2]( const R scalar) const nothrow
	{
		return elementwise_scalar[ lambda(a,b,v) {a=b/v;}]( scalar);
	}
        public operator [] R& (int i, int j) nothrow
        {
                return m_ar[i][j];
        }
        public operator [] const R (int i, int j) const nothrow
        {
                return m_ar[i][j];
        }

	R[DIM2][DIM1] m_ar;
}

generic procedure printMatrix[DIM1,DIM2]( const Matrix[double,DIM1,DIM2] mt) nothrow
{
	var int ii = 0;
	while (ii < DIM1)
	{
		var int jj = 0;
		while (jj < DIM2)
		{
			printf( "\t%.4f", mt[ ii, jj]);
			jj += 1;
		}
		printf( "\n");
		ii += 1;
	}
}

main {
	// Matrix A:
	//      0	2	1
	//      3	1	1
	//      4	0	1
	//      1	5	2
	var Matrix[double,4,3] A = {{0,2,1},{3,1,1},{4,0,1},{1,5,2}};
	printf( "Matrix %s:\n", "A");
	printMatrix[4,3]( A);
	printf( "\n");

	// Matrix B:
	//      1	2	0	3
	//      4	0	5	2
	//      3	3	1	1
	var Matrix[double,3,4] B = {{1,2,0,3},{4,0,5,2},{3,3,1,1}};
	printf( "Matrix %s:\n", "B");
	printMatrix[3,4]( B);
	printf( "\n");

	// Matrix C:
	//      1	-2	-1
	//      -3	0	-1
	//      -4	1	-1
	//      -1	-5	-1
	var Matrix[double,4,3] C = {{1,-2,-1},{-3,0,-1},{-4,1,-1},{-1,-5,-1}};
	printf( "Matrix %s:\n", "C");
	printMatrix[4,3]( C);
	printf( "\n");
	printf( "Matrix %s:\n", "A+C");
	printMatrix[4,3]( A+C);
	printf( "\n");

	// Result A*B:
	//	11	3	11	5
	//	10	9	6	12
	//	7	11	1	13
	//	27	8	27	15
	var Matrix[double,4,4] AB = A * B;
	printf( "Matrix %s:\n", "A*B");
	printMatrix[4,4]( AB);
	printf( "\n");

	// Result A*1.4:
	//      0	2.8	1.4
	//      4.2	1.4	1.4
	//      5.6	0	1.4
	//      1.4	7	2.8
	var Matrix[double,4,3] X = A * 1.4;
	printf( "Matrix %s:\n", "A*1.4");
	printMatrix[4,3]( X);
	printf( "\n");

	// Result B * A:
	// 	9	19	9
	//	22	18	13
	//	14	14	9
	var Matrix[double,3,3] BA = B * A;
	printf( "Matrix %s:\n", "B*A");
	printMatrix[3,3]( BA);
	printf( "\n");
}


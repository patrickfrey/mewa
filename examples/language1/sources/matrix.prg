// Testing matrix implementation with generics

extern "C" procedure printf( const byte^ fmt ...);

double epsilon = 1.11e-16;

function sgn int( double val)
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
	constructor()
	{
                m_ar = {};
        }
	constructor( const R[DIM2][DIM1] ar_)
	{
		m_ar = ar_;
	}
	constructor( const Matrix[R,DIM1,DIM2] o)
	{
		m_ar = o.m_ar;
	}
	public operator + Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                rt.m_ar[ ii][ jj] = m_ar[ ii][ jj] + o.m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public operator - Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                rt.m_ar[ ii][ jj] = m_ar[ ii][ jj] - o.m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public operator - Matrix[R,DIM1,DIM2]() const
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
	public operator * Matrix[R,DIM1,DIM1]( const Matrix[R,DIM2,DIM1] o) const
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
	public operator * Matrix[R,DIM1,DIM2]( const R o) const
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                rt.m_ar[ ii][ jj] = m_ar[ ii][ jj] * o;
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
	public operator / Matrix[R,DIM1,DIM2]( const R o) const
	{
                var Matrix[R,DIM1,DIM2] rt;
		var int ii = 0;
                while (ii < DIM1)
                {
                        var int jj = 0;
                        while (jj < DIM2)
                        {
                                rt.m_ar[ ii][ jj] = m_ar[ ii][ jj] / o;
                                jj += 1;
                        }
                        ii += 1;
                }
                return rt;
	}
        public operator [] R& (int i, int j)
        {
                return m_ar[i][j];
        }
        public operator [] const R (int i, int j) const
        {
                return m_ar[i][j];
        }

	R[DIM2][DIM1] m_ar;
}

generic procedure printMatrix[DIM1,DIM2]( const Matrix[double,DIM1,DIM2] mt)
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

	// Result A*B:
	//	11	3	11	5
	//	10	9	6	12
	//	7	11	1	13
	//	27	8	27	15
	var Matrix[double,4,4] C = A * B;
	printf( "Matrix %s:\n", "A*B");
	printMatrix[4,4]( C);
	printf( "\n");

	// Result B * A:
	// 	9	19	9
	//	22	18	13
	//	14	14	9
	var Matrix[double,3,3] D = B * A;
	printf( "Matrix %s:\n", "B*A");
	printMatrix[3,3]( D);
	printf( "\n");
}


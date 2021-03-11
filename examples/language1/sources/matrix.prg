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
	constructor( const R[DIM1][DIM2] ar_)
	{
		m_ar = ar_;
	}
	constructor( const Matrix[R,DIM1,DIM2] o)
	{
		m_ar = o.m_ar;
	}
	public operator + Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const
	{
                Matrix[R,DIM1,DIM2] rt;
		int ii = 0;
                while (ii < DIM1)
                {
                        int jj = 0;
                        while (jj < DIM2)
                        {
                                rt[ ii][ jj] = m_ar[ ii][ jj] + o.m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
	}
	public operator - Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const
	{
                Matrix[R,DIM1,DIM2] rt;
		int ii = 0;
                while (ii < DIM1)
                {
                        int jj = 0;
                        while (jj < DIM2)
                        {
                                rt[ ii][ jj] = m_ar[ ii][ jj] - o.m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
	}
	public operator - Matrix[R,DIM1,DIM2]() const
	{
                Matrix[R,DIM1,DIM2] rt;
		int ii = 0;
                while (ii < DIM1)
                {
                        int jj = 0;
                        while (jj < DIM2)
                        {
                                rt[ ii][ jj] = -m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
	}
	public operator * Matrix[R,DIM1,DIM2]( const Matrix[R,DIM1,DIM2] o) const
	{
                Matrix[R,DIM1,DIM2] rt;
		int ii = 0;
                while (ii < DIM1)
                {
                        int jj = 0;
                        while (jj < DIM2)
                        {
                                rt[ ii][ jj] = m_ar[ ii][ jj] - o.m_ar[ ii][ jj];
                                jj += 1;
                        }
                        ii += 1;
                }
	}
	public operator * Matrix[R,DIM1,DIM2]( const R o) const
	{
                Matrix[R,DIM1,DIM2] rt;
		int ii = 0;
                while (ii < DIM1)
                {
                        int jj = 0;
                        while (jj < DIM2)
                        {
                                rt[ ii][ jj] = m_ar[ ii][ jj] * o;
                                jj += 1;
                        }
                        ii += 1;
                }
	}
	public operator / Matrix[R,DIM1,DIM2]( const R o) const
	{
                Matrix[R,DIM1,DIM2] rt;
		int ii = 0;
                while (ii < DIM1)
                {
                        int jj = 0;
                        while (jj < DIM2)
                        {
                                rt[ ii][ jj] = m_ar[ ii][ jj] / o;
                                jj += 1;
                        }
                        ii += 1;
                }
	}
	R m_ar[DIM1][DIM2];
}

main {
}


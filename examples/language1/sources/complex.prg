// Testing complex number implementation

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function sqrt double( double val);

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

generic class Complex[R]
{
	constructor()
	{
		m_real = 0.0;
		m_img = 0.0;
	}
	constructor( const R real_, const R img_)
	{
		m_real = real_;
		m_img = img_;
	}
	constructor( const Complex[R]& o)
	{
		m_real = o.m_real;
		m_img = o.m_img;
	}
	constructor( const R real_)
	{
		m_real = real_;
		m_img = 0.0;
	}
	public operator = ( const R real_, const R img_)
	{
		m_real = real_;
		m_img = img_;
	}
	public operator + Complex[R]( const Complex[R]& o)
	{
		return {(m_real + o.m_real), (m_img + o.m_img)};
	}
	public operator - Complex[R]( const Complex[R]& o)
	{
		return {(m_real - o.m_real), (m_img - o.m_img)};
	}
	public operator * Complex[R]( const Complex[R]& o)
	{
		return {(m_real * o.m_real) - (m_img * o.m_img), (m_real * o.m_img) + (m_img * o.m_real)};
	}
	public operator / Complex[R]( const Complex[R]& o)
	{
		return {((m_real * o.m_real) + (m_img * o.m_img)) / (o.m_real * o.m_real + o.m_img * o.m_img), 
					((m_img * o.m_real) - (m_real * o.m_img)) / (o.m_real * o.m_real + o.m_img * o.m_img)};
	}
	public function real R() const
	{
		return m_real;
	}
	public function img R() const
	{
		return m_img;
	}
	public function abs R() const
	{
		return sqrt( m_real * m_real + m_img * m_img);
	}
	public function squareRoot R() const
	{
		return {sqrt( abs() + m_real) / 2, (sgn(m_img) / sqrt(2)) * sqrt( abs() - m_real) / 2};
	}
	public function square R() const
	{
		return self * self;
	}
	R m_real;
	R m_img;
}

generic procedure printComplex[R]( const byte^ text, const Complex[R]& complex)
{
	printf("%s %f i%f\n", text, complex.real(), complex.img());
}

main {
	Complex[double] xx = (cast Complex[double]: { 5, 2 }).square().squareRoot();
	printComplex[double]( "square root (5 + 2i) squared =", xx);
}


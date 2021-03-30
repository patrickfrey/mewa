// Testing complex number implementation

extern "C" procedure printf( const byte^ fmt ...);
extern "C" function sqrt double( double val);

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

generic class Complex[R]
{
	constructor() nothrow
	{
		m_real = 0.0;
		m_img = 0.0;
	}
	constructor( const R real_, const R img_) nothrow
	{
		m_real = real_;
		m_img = img_;
	}
	constructor( const Complex[R] o) nothrow
	{
		m_real = o.real();
		m_img = o.img();
	}
	constructor( const R real_) nothrow
	{
		m_real = real_;
		m_img = 0.0;
	}
	public operator = ( const R real_, const R img_) nothrow
	{
		m_real = real_;
		m_img = img_;
	}
	public function real R() const nothrow
	{
		return m_real;
	}
	public function img R() const nothrow
	{
		return m_img;
	}
	public function abs R() const nothrow
	{
		return sqrt( m_real * m_real + m_img * m_img);
	}
	public function squareRoot Complex[R]() const nothrow
	{
		var double abs_ = abs();
		return (cast Complex[R]: {m_real + abs_, m_img}) / sqrt( 2 * (m_real + abs_));
	}
	public function square Complex[R]() const nothrow
	{
		return self * self;
	}
	public operator + Complex[R]( const Complex[R] o) const nothrow
	{
		return {(m_real + o.real()), (m_img + o.img())};
	}
	public operator - Complex[R]( const Complex[R] o) const nothrow
	{
		return {(m_real - o.real()), (m_img - o.img())};
	}
	public operator - Complex[R]() const nothrow
	{
		return {-m_real,-m_img};
	}
	public operator * Complex[R]( const Complex[R] o) const nothrow
	{
		return {(m_real * o.real()) - (m_img * o.img()), (m_real * o.img()) + (m_img * o.real())};
	}
	public operator / Complex[R]( const R o) const nothrow
	{
		return {m_real / o, m_img / o};
	}
	public operator / Complex[R]( const Complex[R] o) const nothrow
	{
		return {((m_real * o.real()) + (m_img * o.img())) / (o.real() * o.real() + o.img() * o.img()), 
					((m_img * o.real()) - (m_real * o.img())) / (o.real() * o.real() + o.img() * o.img())};
	}
	R m_real;
	R m_img;
}

generic procedure printComplex[R]( const byte^ text, const Complex[R] complex) nothrow
{
	var int sgn_img = sgn(complex.img());
	if (sgn_img == 1)
	{
		printf("%s %.4f +%.4fi\n", text, complex.real(), complex.img());
	}
	elseif (sgn_img == -1)
	{
		printf("%s %.4f%.4fi\n", text, complex.real(), complex.img());
	}
	else
	{
		printf("%s %.4f\n", text, complex.real());
	}
}
generic procedure printFloat[R]( const byte^ text, const R val) nothrow
{
	printf("%s %.4f\n", text, val);
}

main {
	var Complex[double] xx = (cast Complex[double]: { 5, 2 }).square().squareRoot();
	printComplex[double]( "square root (5 + 2i) squared = 5 + 2i | ", xx);
	printComplex[double]( "(1 + 3i) squared = -8 + 6i | ", (cast Complex[double]: { 1, 3 }).square());
	printComplex[double]( "(2 + 3i) * (4 - 7i) = 29 - 2i | ", (cast Complex[double]: { 2, 3 }) * {4,-7});
	printComplex[double]( "(3 - 1i) + (-4 - 3i) = -1 - 4i | ", (cast Complex[double]: { 3, -1 }) + {-4,-3});
	printComplex[double]( "(-2 + 2i) - (-3 + 2i) = 1 | ", (cast Complex[double]: { -2, 2 }) - {-3,2});
	printComplex[double]( "(2 + 3i) / (3 - 3i) = -0.1666 + 0.8333i | ", (cast Complex[double]: { 2, 3 }) / {3,-3});
	printComplex[double]( "(2 + 2i) / (3 - 3i) = 0.6666i | ", (cast Complex[double]: { 2, 2 }) / {3,-3});
	printFloat[double]( "abs(5 - 1i) = 5.0990 | ", (cast Complex[double]: { 5, -1 }).abs());
	printComplex[double]( "square root i = 0.7071 + 0.7071i | ", (cast Complex[double]: { 0, 1 }).squareRoot());
	printComplex[double]( "square root i+1 = 1.0987 + 0.4551i | ", (cast Complex[double]: { 1, 1 }).squareRoot());
	printComplex[double]( "(-8 + 6i) square root = 1 + 3i | ", (cast Complex[double]: { -8, 6 }).squareRoot());
}


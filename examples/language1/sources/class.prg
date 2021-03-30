// Testing class construction and access and interface calls

extern "C" procedure printf( const byte^ fmt ...);

interface Object
{
	function x int() const nothrow;
	function y int() const nothrow;
	function ofs_x int( int addx) const nothrow;
	function ofs_y int( int addy) const nothrow;
}

interface ObjectUpdater
{
	operator = ( int x_, int y_);
}

class Point :Object,ObjectUpdater
{
	public constructor( int x_, int y_) nothrow
	{
		m_x = x_;
		self.m_y = y_;
	}

	public operator = ( int x_, int y_) nothrow
	{
		self.m_x = x_;
		m_y = y_;
	}

	public function x int() const nothrow
	{
		return m_x;
	}

	public function y int() const nothrow
	{
		return self.m_y;
	}

	function ofs_x int( int addx) const nothrow
	{
		return m_x + addx;
	}

	function ofs_y int( int addy) const nothrow
	{
		return m_y + addy;
	}

	public function object Object() const nothrow
	{
		return self.Object;
	}
	public function updater ObjectUpdater() nothrow
	{
		return self.ObjectUpdater;
	}

	int m_x;
	int m_y;
}

class Line :Point
{
	public constructor( int x_, int y_) nothrow
	{
		self.Point = {x_,y_};
	}

	public procedure move( int x_, int y_) nothrow
	{
		self.Point = {x_,y_};
	}
}

main {
	var float x = 1.23;
	var Line ab = {7.23, 4.23};
	var Object obj = ab.object();
	printf("RESULT[1] x = %d\n", obj.x());
	printf("RESULT[1] y = %d\n", obj.y());
	var ObjectUpdater updater = ab.updater();
	updater = {31,411};
	printf("RESULT[2] x = %d\n", obj.x());
	printf("RESULT[2] y = %d\n", obj.y());
	updater = {71,511};
	printf("RESULT[3] x = %d\n", obj.ofs_x( -13));
	printf("RESULT[3] y = %d\n", obj.ofs_y( 1));
	var ObjectUpdater updater2 = updater;
	updater2 = {32,412};
	printf("RESULT[4] x = %d\n", obj.ofs_x( -11));
	printf("RESULT[4] y = %d\n", obj.y());
	return 0;
}


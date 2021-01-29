
interface Object
{
	function x int() const;
	function y int() const;
}

class Point
{
	public constructor( int x_, int y_)
	{
		m_x = x_;
		m_y = y_;
	}

	public operator = ( int x_, int y_)
	{
		m_x = x_;
		m_y = y_;
	}

	public function x int() const
	{
		return m_x;
	}

	public function y int() const
	{
		return m_y;
	}

	int m_x;
	int m_y;
}

class Line :Point
{
	public constructor( int x_, int y_)
	{
		Point = {x_,y_};
	}

	public procedure move( int x_, int y_)
	{
		Point = {x_,y_};
	}
}

main {
	return 0;
}


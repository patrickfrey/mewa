
interface Object
{
	function int x() const;
	function int y() const;
}

class Point
{
	public constructor( int x_, int y_)
	{
		m_x = x_;
		m_y = y_;
	}

	public function int x() const
	{
		return m_x;
	}

	public function int y() const
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


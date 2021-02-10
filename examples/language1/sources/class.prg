
interface Object
{
	function x int() const;
	function y int() const;
}

interface ObjectUpdater
{
	operator = ( int x_, int y_);
}

class Point :Object,ObjectUpdater
{
	public constructor( int x_, int y_)
	{
		m_x = x_;
		this->m_y = y_;
	}

	public operator = ( int x_, int y_)
	{
		this->m_x = x_;
		m_y = y_;
	}

	public function x int() const
	{
		return m_x;
	}

	public function y int() const
	{
		return this->m_y;
	}

	public function object Object() const
	{
		return this->Object;
	}
	public function updater ObjectUpdater()
	{
		return this->ObjectUpdater;
	}

	int m_x;
	int m_y;
}

class Line :Point
{
	public constructor( int x_, int y_)
	{
		this->Point = {x_,y_};
	}

	public procedure move( int x_, int y_)
	{
		this->Point = {x_,y_};
	}
}

main {
	var float x = 1.23;
	var Line ab = {1.23, 4.23};
	var Object obj = ab.object();
	var ObjectUpdater updater = ab.updater();
	var int xx = obj.y();
	return 0;
}


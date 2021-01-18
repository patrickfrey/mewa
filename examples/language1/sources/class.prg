
interface Point
{
	function int x() const;
	function int y() const;
}

class Line :Point
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

public procedure move( int x_, int y_)
{
	m_x = x_;
	m_y = y_;
}

int m_x;
int m_y;
}

main {
	return 0;
}


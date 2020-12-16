
class Point
{
public:
	Point() noexcept		:m_x(0),m_y(0){}
	Point( int x_, int y_) noexcept	:m_x(x_),m_y(y_){}
	Point( const Point& o) noexcept	:m_x(o.m_x),m_y(o.m_y){}
	~Point() noexcept {}

private:
	int m_x;
	int m_y;
};

static Point g_par[ 100];
static int g_scar[ 10];

int main( int argc, const char* argv[]) noexcept
{
	Point par[ 100];
	int scar[ 50];

	return 0;
}

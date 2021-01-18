
class I
{
public:
	virtual ~I(){}
	virtual void do_one( int a) = 0;
	virtual void do_two( double a) = 0;
};

class J
{
public:
	virtual ~J(){}
	virtual int ho() = 0;
	virtual double go() = 0;
};

class A :public I, public J
{
public:
	A( int _1, double _2) :m_1(_1),m_2(_2){}
	~A(){}

	void do_one( int v) override		{m_1 = v;}
	void do_two( double v) override		{m_2 = v;}
	int ho() override			{return m_1;}
	double go() override			{return m_1;}

private:
	int m_1;
	double m_2;
};

int main( int argc, char const* argv[])
{
	A aa( 2, 3.14159265358759);
	aa.do_one( 44);
	aa.do_two( 1.181);
	return aa.ho() * aa.go();
}


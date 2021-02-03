
bool do_throw_flag = false;

void freeptr( char const*)
{
	do_throw_flag = true;
}

class E
{
public:
	E( const char* msg_) :m_msg(msg_){}
	const char* operator*() const {return m_msg;}
private:
	const char* m_msg;
};

class A
{
public:
	A( const char* ptr_)
	{
		m_ptr = ptr_;
		m_a = 0;
		m_b = 0;
	}
	A( const A& o)
		:m_ptr(o.m_ptr),m_a(o.m_a),m_b(o.m_b){}

	virtual float xxx( int p1, float p2)
	{
		m_a += 1;
		m_b += m_a;
		return p1 + p2;
	}
	~A()
	{
		freeptr( m_ptr);
	}
private:
	const char* m_ptr;
	int m_a;
	int m_b;
};

class B
{
public:
	B( const A& _1, const A& _2) :m_1(_1),m_2(_2){}
	~B(){}

	A getNewA( const char* ptr_)
	{
		return A(ptr_);
	}

private:
	A m_1;
	A m_2;
};

int main( int argc, char const* argv[])
{
	do_throw_flag = argc > 3;
	B b( "blabla", argv[1]);
	A rr = b.getNewA( "gu");
	return 0;
}



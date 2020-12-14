
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
		if (do_throw_flag) throw E(ptr_);
	}
	A( const A& o)
	{
		m_ptr = o.m_ptr;
	}
	~A()
	{
		freeptr( m_ptr);
	}
private:
	const char* m_ptr;
};

class B
{
public:
	B( const A& _1, const A& _2) :m_1(_1),m_2(_2){}
	~B(){}

private:
	A m_1;
	A m_2;
};

int main( int argc, char const* argv[])
{
	try {
		do_throw_flag = argc > 3;
		B( "blabla", "hallygally");
	} catch (const E& e) {
		return 1;
	}
	return 0;
}



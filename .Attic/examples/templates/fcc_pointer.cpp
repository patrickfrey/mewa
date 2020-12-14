int globalvar = 3;
int& globalvar2 = globalvar;

static int localf1( int a)
{
	int b = a;
	return b + a;
}

static int localf2( int a)
{
	return a+1;
}

int function( int a)
{
	return (3294672 + a) ? localf1( globalvar2) : localf2( globalvar2);
}


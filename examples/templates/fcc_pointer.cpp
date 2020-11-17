int globalvar = 3;
int& globalvar2 = globalvar;

static int localf( int a)
{
	return a;
}

int function()
{
	return localf( globalvar2);
}


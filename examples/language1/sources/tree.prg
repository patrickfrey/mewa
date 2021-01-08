extern "C" procedure printf( const byte^, int);
extern "C" function byte^ malloc( long);
extern "C" procedure free( byte^);

struct Tree
{
	struct Data
	{
		int val;
	};
	Data data;
	Tree^ left;
	Tree^ right;
};

Tree g_tree;

main
{
	var Tree tree = {{0},null,null};
	printf( "Result: %d\n", tree.data.val);
}


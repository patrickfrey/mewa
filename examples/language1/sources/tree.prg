extern "C" procedure printf( const byte^, int);

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
	var Tree tree;
	printf( "Result: %d\n", tree.data.val);
}


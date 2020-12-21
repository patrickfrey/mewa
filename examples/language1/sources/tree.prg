extern "C" procedure printf( const byte^, int);

struct Tree
{
	struct Data
	{
		var int val;
	};
	var Data data;
	var Tree^ left;
	var Tree^ right;
};

var Tree g_tree;

main
{
	var Tree tree;
	var int val = fibonacci( 20);
	printf( "Result: %d\n", tree.data.val);
}


extern "C" procedure printf( const byte^ ...);
extern "C" procedure putchar( const byte);
extern "C" function byte^ malloc( long);
extern "C" procedure free( byte^);

function byte^ allocmem( long bytes)
{
	return malloc( bytes);
}
procedure freemem( byte^ ptr)
{
	free( ptr);
}

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

procedure printTree( const Tree^ node, int indent)
{
	var int ii = 0; 
	while (ii < indent) 
	{
		putchar( ' ');
		ii += 1;
	}
	printf( "Node: %d\n", node->data.val);
	if (node->left != null) {printTree( node->left, indent+1);}
	if (node->right != null) {printTree( node->right, indent+1);}
}

procedure deleteTree( Tree^ node)
{
	if (node->left != null) {deleteTree( node->left);}
	if (node->right != null) {deleteTree( node->right);}
	delete node;
}

main
{
	var Tree^ tree = new Tree: {{11},new Tree: {{1},null,null}, new Tree: {{2},null,null}};
	printf( "VAL %d\n", tree->data.val);
	printTree( tree, 0);
	deleteTree( tree);
	printf( "Done\n");
}


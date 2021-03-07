// Testing structures and initialization from a tree structure specified as initializer lists in the source

extern "C" procedure printf( const byte^ fmt ...);
extern "C" procedure putchar( const byte);
extern "C" function malloc byte^( long);
extern "C" procedure free( byte^);

private function allocmem byte^( long bytes)
{
	return malloc( bytes);
}
private procedure freemem( byte^ ptr)
{
	free( ptr);
}

struct Tree
{
	struct Data
	{
		int val;
	}
	Data data;
	Tree^ left;
	Tree^ right;
}

Tree g_tree;

private procedure printTree( const Tree^ node, int indent)
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

private procedure deleteTree( Tree^ node)
{
	if (node->left != null) {deleteTree( node->left);}
	if (node->right != null) {deleteTree( node->right);}
	delete node;
}

main
{
	var Tree^ tree = new Tree: {{11},new Tree: {{1},null,null}, new Tree: {{2},null,new Tree: {{22},null,null}}};
	printf( "VAL %d\n", tree->data.val);
	printTree( tree, 0);
	deleteTree( tree);
	printf( "Done\n");
}


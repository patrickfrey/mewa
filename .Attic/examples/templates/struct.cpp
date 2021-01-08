struct Tree
{
	struct Data
	{
		int val1;
		int val2;
	};
	Data data;
	struct Tree* left;
	struct Tree* right;
};

static void initData( Tree::Data* ptr, int val1, int val2)
{
	ptr->val1 = val1;
	ptr->val2 = val2;
}
static void copyData( Tree::Data* ptr, Tree::Data* oth)
{
	ptr->val1 = oth->val1;
	ptr->val2 = oth->val2;
}

static void copyTree( Tree* ptr, Tree* oth)
{
	copyData( &ptr->data, &oth->data);
	ptr->left = oth->left;
	ptr->right = oth->right;
}

Tree g_tree;

int main( int argc, const char** argv)
{
	Tree tree;
	Tree oth;
	initData( &tree.data, 45, 67);
	tree.data.val1 = 2;
	tree.left = 0;
	copyTree( &tree, &oth);
	return tree.data.val1;
}


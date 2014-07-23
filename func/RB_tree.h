#ifndef __RB_TREE_HPP
#define __RB_TREE_HPP

#include <cstddef>

/*
 * red-black properties:
 *
 * 1. Every node is either red or black.
 * 2. The root is black.
 * 3. Every leaf(NULL) is black.
 * 4. If a node is red, then both its children are black.
 * 5. For each node, all paths from the node to descendant leaves contain the
 * same number of black nodes.
 *
 */

enum _RB_tree_color { RED, BLACK };

template <typename _Key, typename _Value>
class _RB_tree_node {
public:
	_RB_tree_node(void)
		: _left(NULL), _right(NULL), _p(NULL), _color(BLACK)
	{}

	_RB_tree_node(_Key& k, _Value& v)
		: _left(NULL), _right(NULL), _p(NULL), _color(BLACK), _key(k), _value(v)
	{}

	_Key key(void)
	{
		return _key;
	}

	_Value value(void)
	{
		return _value;
	}


	_RB_tree_node *_left, *_right, *_p;
	_RB_tree_color _color;
	_Key _key;
	_Value _value;
};


/*template <typename _Key, typename _Value>
void
print_tree(_RB_tree_node<_Key, _Value> *x, _RB_tree_node<_Key, _Value> *nil)
{
	static int depth;

	depth++;
	if (x != nil) {
		std::cout << "!" << x->_key << "! " << ((x->_color == RED) ? "red" : "black") << std::endl;

//		if (x->_left != nil) {
			for (int i = 0; i < depth; i++)
				std::cout << "  ";
			std::cout << "(l ";
//		}

		print_tree(x->_left, nil);

//		if (x->_left != nil) {
		if (x->_left != nil)
			for (int i = 0; i < depth; i++)
				std::cout << "  ";
		else
			std::cout << "nil black";

			std::cout << ")\n";
//		}

//		if (x->_right != nil) {
			for (int i = 0; i < depth; i++)
				std::cout << "  ";
			std::cout << "(r ";
//		}

		print_tree(x->_right, nil);

//		if (x->_right != nil) {
		if (x->_right != nil)
			for (int i = 0; i < depth; i++)
				std::cout << "  ";
		else
			std::cout << "nil black";
			std::cout << ")\n";
//		}
	}
	depth--;
}*/


template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
class _RB_tree {
public:
	_RB_tree(void)
		: _nil(&_nil_node), _size(0)
	{
		_root = _nil;
		_nil->_p = _root;
		_nil->_color = BLACK;
		_nil->_left = _nil;
		_nil->_right = _nil;
	}

	virtual ~_RB_tree(void)
	{
	}

	size_t size(void)
	{
		return _size;
	}

	_RB_tree_node<_Key, _Value> *find(_Key k)
	{
		//printf("%d\n", *k);
		return rb_search(_root, k);
	}

	//在添加插入时不查找，查找在调用前做
	_RB_tree_node<_Key, _Value> * insert_direct(_Key k, _Value v)
	{
		_RB_tree_node<_Key, _Value> *n =
			(_RB_tree_node<_Key, _Value> *)_alloc.alloc(sizeof(_RB_tree_node<_Key, _Value>));
		n->_key = k;
		n->_value = v;

		++_size;
		rb_insert(n);
		return n;
	}

	//添加返回值为新的结点(或新值的结点)
	_RB_tree_node<_Key, _Value> * insert(_Key k, _Value v)
	{
		_RB_tree_node<_Key, _Value> * temp=NULL;
		if ((temp = find(k)) == _nil) {
			_RB_tree_node<_Key, _Value> *n =
				(_RB_tree_node<_Key, _Value> *)_alloc.alloc(sizeof(_RB_tree_node<_Key, _Value>));
			n->_key = k;
			n->_value = v;

			++_size;
			rb_insert(n);
			temp=n;
		}
		else
		{
			temp->_value = v;
		}
		return temp;
	}

	bool remove(_Key k)
	{
		_RB_tree_node<_Key, _Value> *t(NULL);
		if ((t = rb_search(_root, k)) != _nil && ((t = rb_delete(t)) != _nil)) {
			--_size;
			_alloc.dealloc(t);
			return true;
		} else {
			return false;
		}
	}

	bool remove(_RB_tree_node<_Key, _Value> * t)
	{
		if ((t != _nil) && ((t = rb_delete(t)) != _nil)) {
			--_size;
			_alloc.dealloc(t);
			return true;
		} else {
			return false;
		}
	}

	_RB_tree_node<_Key, _Value> *root(void)
	{
		return _root;
	}

	_RB_tree_node<_Key, _Value> *nil(void)
	{
		return _nil;
	}

	void removeall();

private:
	void left_rotate(_RB_tree_node<_Key, _Value> *x);
	void right_rotate(_RB_tree_node<_Key, _Value> *x);

	void rb_insert_fixup(_RB_tree_node<_Key, _Value> *z);
	void rb_delete_fixup(_RB_tree_node<_Key, _Value> *z);

	_RB_tree_node<_Key, _Value> *rb_search(_RB_tree_node<_Key, _Value> *x, _Key& k);
	void rb_insert(_RB_tree_node<_Key, _Value> *z);
	_RB_tree_node<_Key, _Value> *rb_delete(_RB_tree_node<_Key, _Value> *z);

	void rb_delete_all(_RB_tree_node<_Key, _Value> *x);

	_RB_tree_node<_Key, _Value> *tree_successor(_RB_tree_node<_Key, _Value> *x);
	_RB_tree_node<_Key, _Value> *tree_minimum(_RB_tree_node<_Key, _Value> *x);

private:
	_RB_tree_node<_Key, _Value> *_root;
	_RB_tree_node<_Key, _Value> *_nil;
	_RB_tree_node<_Key, _Value> _nil_node;

	_Compare _compare;
	_Alloc _alloc;

	size_t _size;

public:
	
	/*
	 copy from linux kernel source
	 */
    _RB_tree_node<_Key, _Value> * first()
    {
        _RB_tree_node<_Key, _Value>  *n;

        n = _root;
        if (n == _nil)
            return _nil;
        while (n->_left != _nil)
            n = n->_left;
        return n;
    }

    _RB_tree_node<_Key, _Value> * next(_RB_tree_node<_Key, _Value> * node)
    {
        _RB_tree_node<_Key, _Value> *parent;

        if (node->_p == node)
            return _nil;

        /* If we have a right-hand child, go down and then left as far
           as we can. */
        if (node->_right != _nil) {
            node = node->_right;
            while (node->_left != _nil)
                node = node->_left;
            return node;
        }

        /* No right-hand children.  Everything down and left is
           smaller than us, so any 'next' node must be in the general
           direction of our parent. Go up the tree; any time the
           ancestor is a right-hand child of its parent, keep going
           up. First time it's a left-hand child of its parent, said
           parent is our 'next' node. */
        while (((parent = node->_p) != _nil) && node == parent->_right)
            node = parent;

        return parent;
    }
	
	
    /*void mid_tranverse(_RB_tree_node<_Key, _Value> *n)
    {
        if ( n != NULL && n != _nil )
        {
            mid_tranverse(n->_left);

            printf("Value = %d  Color = %s\n", n->_value, n->_color == BLACK ? "BLACK" : "RED");

			mid_tranverse(n->_right);
        }
    }*/
};






template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::rb_delete_all(_RB_tree_node<_Key, _Value> *x)
{
	if (x != _nil) {
		rb_delete_all(x->_left);
		rb_delete_all(x->_right);
		_alloc.dealloc(x);
		--_size;
	}
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::removeall()
{
	rb_delete_all(_root);
	_root = _nil;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
_RB_tree_node<_Key, _Value> *
_RB_tree<_Key, _Value, _Compare, _Alloc>::rb_search(_RB_tree_node<_Key, _Value> *x, _Key& k)
{
	int64_t r = 0;

	while (x != _nil && (r = _compare(k, x->_key)) != 0) {
		if (r < 0) {
			x = x->_left;
		} else {
			x = x->_right;
		}
	}
	return x;
	/*
	if (x == _nil) {
		return x;
	}

	int r = _compare(k, x->_key);
	if (r < 0) {
		return rb_search(x->_left, k);
	} else if (r > 0) {
		return rb_search(x->_right, k);
	} else {
		return x;
	}
	*/
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::left_rotate(_RB_tree_node<_Key, _Value> *x)
{
	_RB_tree_node<_Key, _Value> *y(x->_right);	// Set y.

	x->_right = y->_left;				// Turn y's left subtree into x's right subtree.
	if (y->_left != _nil) {
		y->_left->_p = x;
	}

	y->_p = x->_p;						// Link x's parent to y.
	if (x->_p == _nil) {
		_root = y;
	} else {
		if (x == x->_p->_left) {
			x->_p->_left = y;
		} else {
			x->_p->_right = y;
		}
	}

	y->_left = x;						// Put x on y's left.
	x->_p = y;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::right_rotate(_RB_tree_node<_Key, _Value> *y)
{
	_RB_tree_node<_Key, _Value> *x(y->_left);

	y->_left = x->_right;
	// !!!, _nil is sentinal in rb_delete, u can't change its parent here
	if (x->_right != _nil) {
		x->_right->_p = y;
	}

	x->_p = y->_p;
	if (y->_p == _nil) {
		_root = x;
	} else {
		if (y == y->_p->_left) {
			y->_p->_left = x;
		} else {
			y->_p->_right = x;
		}
	}

	x->_right = y;
	y->_p = x;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::rb_insert(_RB_tree_node<_Key, _Value> *z)
{
	_RB_tree_node<_Key, _Value> *y(_nil), *x(_root);

	while (x != _nil) {
		y = x;
		if (_compare(z->_key, x->_key) < 0) {
			x = x->_left;
		} else {
			x = x->_right;
		}
	}

	z->_p = y;
	if (y == _nil) {
		_root = z;
	} else if (_compare(z->_key, y->_key) < 0) {
		y->_left = z;
	} else {
		y->_right = z;
	}

	z->_left = _nil;
	z->_right = _nil;
	z->_color = RED;

	rb_insert_fixup(z);
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::rb_insert_fixup(_RB_tree_node<_Key, _Value> *z)
{
	_RB_tree_node<_Key, _Value> *y;

	while (z->_p->_color == RED) {

		if (z->_p == z->_p->_p->_left) {
			y = z->_p->_p->_right;
			if (y->_color == RED) {
				z->_p->_color = BLACK;			// Case 1
				y->_color = BLACK;				// Case 1
				z->_p->_p->_color = RED;		// Case 1
				z = z->_p->_p;					// Case 1
			} else {
				if (z == z->_p->_right) {
					z = z->_p;					// Case 2
					left_rotate(z);				// Case 2
				}
				z->_p->_color = BLACK;			// Case 3
				z->_p->_p->_color = RED;		// Case 3
				right_rotate(z->_p->_p);		// Case 3
			}
		} else {	// with "right" and "left" exchanged
			y = z->_p->_p->_left;
			if (y->_color == RED) {
				z->_p->_color = BLACK;			// Case 1
				y->_color = BLACK;				// Case 1
				z->_p->_p->_color = RED;		// Case 1
				z = z->_p->_p;					// Case 1
			} else {
				if (z == z->_p->_left) {
					z = z->_p;					// Case 2
					right_rotate(z);			// Case 2
				}
				z->_p->_color = BLACK;			// Case 3
				z->_p->_p->_color = RED;		// Case 3
				left_rotate(z->_p->_p);			// Case 3
			}
		}
	}

	_root->_color = BLACK;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
_RB_tree_node<_Key, _Value> *
_RB_tree<_Key, _Value, _Compare, _Alloc>::rb_delete(_RB_tree_node<_Key, _Value> *z)
{
	_RB_tree_node<_Key, _Value> *y, *x;

	if (z->_left == _nil || z->_right == _nil) {
		y = z;
	} else {
		y = tree_successor(z);
	}

	if (y->_left != _nil) {
		x = y->_left;
	} else {
		x = y->_right;
	}

	x->_p = y->_p;

	if (y->_p == _nil) {
		_root = x;
	} else {
		if (y == y->_p->_left) {
			y->_p->_left = x;
		} else {
			y->_p->_right = x;
		}
	}

	if (y != z) {
		z->_key = y->_key;
		// copy y's satellite data into z
		z->_value = y->_value;
	}


	if (y->_color == BLACK) {
		rb_delete_fixup(x);
	}


	return y;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
void
_RB_tree<_Key, _Value, _Compare, _Alloc>::rb_delete_fixup(_RB_tree_node<_Key, _Value> *x)
{
	_RB_tree_node<_Key, _Value> *w;

	while (x != _root && x->_color == BLACK) {
		if (x == x->_p->_left) {
			w = x->_p->_right;

			if (w->_color == RED) {
				w->_color = BLACK;			// Case 1
				x->_p->_color = RED;		// Case 1
				left_rotate(x->_p);			// Case 1
				w = x->_p->_right;			// Case 1
			}

			if (w->_left->_color == BLACK && w->_right->_color == BLACK) {
				w->_color = RED;			// Case 2
				x = x->_p;					// Case 2
			} else  {
				if (w->_right->_color == BLACK) {
					w->_left->_color = BLACK;		// Case 3
					w->_color = RED;				// Case 3
					right_rotate(w);				// Case 3
					w = x->_p->_right;				// Case 3
				}

				w->_color = x->_p->_color;		// Case 4
				x->_p->_color = BLACK;			// Case 4
				w->_right->_color = BLACK;		// Case 4
				left_rotate(x->_p);				// Case 4
				x = _root;						// Case 4
			}
		} else {	// with "right" and "left" exchanged
			w = x->_p->_left;

			if (w->_color == RED) {
				w->_color = BLACK;			// Case 1
				x->_p->_color = RED;		// Case 1
				right_rotate(x->_p);			// Case 1
				w = x->_p->_left;			// Case 1
			}

			if (w->_right->_color == BLACK && w->_left->_color == BLACK) {
				w->_color = RED;			// Case 2
				x = x->_p;					// Case 2
			} else  {
				if (w->_left->_color == BLACK) {
					w->_right->_color = BLACK;		// Case 3
					w->_color = RED;				// Case 3
					left_rotate(w);				// Case 3
					w = x->_p->_left;				// Case 3
				}

				w->_color = x->_p->_color;		// Case 4
				x->_p->_color = BLACK;			// Case 4
				w->_left->_color = BLACK;		// Case 4
				right_rotate(x->_p);				// Case 4
				x = _root;						// Case 4
			}
		}
	}

	x->_color = BLACK;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
_RB_tree_node<_Key, _Value> *
_RB_tree<_Key, _Value, _Compare, _Alloc>::tree_successor(_RB_tree_node<_Key, _Value> *x)
{
	if (x->_right != _nil) {
		return tree_minimum(x->_right);
	}

	_RB_tree_node<_Key, _Value> *y(x->_p);
	while (y != _nil && x == y->_right) {
		x = y;
		y = y->_p;
	}

	return y;
}

template <typename _Key, typename _Value, typename _Compare, typename _Alloc>
_RB_tree_node<_Key, _Value> *
_RB_tree<_Key, _Value, _Compare, _Alloc>::tree_minimum(_RB_tree_node<_Key, _Value> *x)
{
	while (x->_left != _nil) {
		x = x->_left;
	}
	return x;
}

#endif	/* __RB_TREE_HPP */

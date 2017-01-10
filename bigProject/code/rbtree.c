/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/lib/rbtree.c
*/

#include <linux/rbtree.h>
#include <linux/module.h>

//对节点进行左旋操作
static void __rb_rotate_left(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *right = node->rb_right;
	struct rb_node *parent = rb_parent(node);

	if ((node->rb_right = right->rb_left))
		rb_set_parent(right->rb_left, node);
	right->rb_left = node;

	rb_set_parent(right, parent);

	if (parent)
	{
		if (node == parent->rb_left)
			parent->rb_left = right;
		else
			parent->rb_right = right;
	}
	else
		root->rb_node = right;
	rb_set_parent(node, right);
}

//对节点进行右旋操作
static void __rb_rotate_right(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *left = node->rb_left;
	struct rb_node *parent = rb_parent(node);

	if ((node->rb_left = left->rb_right))
		rb_set_parent(left->rb_right, node);
	left->rb_right = node;

	rb_set_parent(left, parent);

	if (parent)
	{
		if (node == parent->rb_right)
			parent->rb_right = left;
		else
			parent->rb_left = left;
	}
	else
		root->rb_node = left;
	rb_set_parent(node, left);
}

/*
*node是新插入的节点，默认为红色
*此函数检查是否有违反红黑树定义的地方并纠正
*/
void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *parent, *gparent;

	/*
	*如果有父节点且父节点是红色的，进行树的调整以保证其符合红黑树
	*/
	while ((parent = rb_parent(node)) && rb_is_red(parent))
	{
		gparent = rb_parent(parent);

		if (parent == gparent->rb_left)
		{
			/*
			*这里是父节点为祖父节点左子节点的情况
			*else里的是右子节点的情况，左右相反而已，所以只在这里作注释
			*/
			{
				register struct rb_node *uncle = gparent->rb_right;
				if (uncle && rb_is_red(uncle))
				{
					/*
					*如果叔叔节点存在且为红色的
					*将叔叔节点和父节点均变为黑色
					*并将祖父节点变为红色
					*/
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					/*
					*现在局部的黑色节点数是平衡的，但是祖父节点变为红色的了
					*因为不知道是否会造成父子节点均为红色的情况
					*所以需要对祖父节点进行下一轮修复
					*/
					node = gparent;
					continue;
				}
			}

			//现在是叔叔节点为空或者为黑色的情况
			if (parent->rb_right == node)
			{
				/*
				*如果新节点是父节点的右子节点，对父节点进行左旋
				*旋转后新节点成了新的父节点
				*现在父节点是红色的，其左子节点也是红色的
				*/
				register struct rb_node *tmp;
				__rb_rotate_left(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			/*
			*此时父节点为红色，其左子节点也为红色
			*将父节点变为黑色，祖父节点变为红色，然后对祖父节点右旋，调整完成
			*/
			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_right(gparent, root);
		} else {
			{
				register struct rb_node *uncle = gparent->rb_left;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_left == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_right(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_left(gparent, root);
		}
	}

	rb_set_black(root->rb_node);
}
EXPORT_SYMBOL(rb_insert_color);

/*
*在node与parent之前刚刚删除了一个黑色节点，现在树很可能不平衡
*node与parent也可能有颜色上的冲突
*本函数进行树的性质的修复，以使树恢复平衡
*在一些情况下问题会转移到上一层节点，则需对上一层节点进行递归检查与修正
*/
static void __rb_erase_color(struct rb_node *node, struct rb_node *parent,
			     struct rb_root *root)
{
	/*
	*other用来保存兄弟节点
	*/
	struct rb_node *other;

	/*
	*循环条件：node不是红色节点也不是根节点
	*因为对于红色节点或者根节点直接将其变为黑色即可
	*/
	while ((!node || rb_is_black(node)) && node != root->rb_node)
	{
		/*
		*if-else两个分支里的逻辑是一样的，只是左右相反，所以只注释if里的代码
		*当前的状态：
		*1、因为删除的是黑色节点，所以node与parent均有可能是红色节点
		*2、node与parent之间少了一个黑色节点，则所有通过node的路径都少了一个黑色节点
		*但node的兄弟节点的高度并未变化
		*/
		if (parent->rb_left == node)
		{
			other = parent->rb_right;
			if (rb_is_red(other))
			{
				/*
				*如果兄弟节点是红色的，则父节点是黑色的
				*并交换父节点与兄弟节点的颜色，对父节点进行左旋
				*旋转之后兄弟节点占据的原先父节点的位置，且和原父节点颜色一样
				*所以不会有向上的颜色冲突
				*仍以原的父节点为父节点来看，当前状态是：
				*父节点右子节点保持平衡，只有经过node的路径少了一个黑色节点
				*现在问题和之前类似，但是node有了一个黑色的兄弟节点以及红色父节点
				*/
				rb_set_black(other);
				rb_set_red(parent);
				__rb_rotate_left(parent, root);
				//other指向新的兄弟节点，other必为一个黑色节点而不会是空的
				other = parent->rb_right;
			}
			/*
			*此时的状态：
			*node有黑色兄弟，父节点可能是黑色的也可能是红色的
			*/
			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
			    (!other->rb_right || rb_is_black(other->rb_right)))
			{
				/*如果other没有红色子节点，那就可以将other变为红色，向上转移问题
				*other变为红色后，other分支少了一个黑色节点，与node分支保持了平衡
				*但parent整体上少了一个黑色节点
				*
				*/
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				//现在黑色兄弟节点有红色子节点，父节点颜色未知
				if (!other->rb_right || rb_is_black(other->rb_right))
				{
					/*
					*如果黑色兄弟节点为空或为黑色，则左子节点定为红色
					*将其调整为右子节点为红色
					*other->left与other互换颜色，对other进行右旋，other指向新的兄弟
					*现在other仍为黑色，且有了一个红色右子节点
					*/
					rb_set_black(other->rb_left);
					rb_set_red(other);
					__rb_rotate_right(other, root);
					other = parent->rb_right;
				}
				/*
				*此时的状态：黑色兄弟节点有红色的右子节点
				*把other涂成父节点的颜色（之后旋转，other占据父亲的位置，向上没有颜色冲突）
				* 把父节点变为黑色，把黑色兄弟节点的other变为黑色，other分枝多了一个黑色节点
				* 现在对父节点进行左旋，旋转后的情况是右边分枝（原other右子）少了一个黑色节点，重归平衡
				* 左边分枝则增加了一个黑色节点，也恢复了平衡，此时也没有颜色冲突
				*/
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->rb_right);
				__rb_rotate_left(parent, root);
				node = root->rb_node;
				break;
			}
		}
		else
		{
			other = parent->rb_left;
			if (rb_is_red(other))
			{
				rb_set_black(other);
				rb_set_red(parent);
				__rb_rotate_right(parent, root);
				other = parent->rb_left;
			}
			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
			    (!other->rb_right || rb_is_black(other->rb_right)))
			{
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->rb_left || rb_is_black(other->rb_left))
				{
					rb_set_black(other->rb_right);
					rb_set_red(other);
					__rb_rotate_left(other, root);
					other = parent->rb_left;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->rb_left);
				__rb_rotate_right(parent, root);
				node = root->rb_node;
				break;
			}
		}
	}
	//对于红色节点或者根节点直接将其变为黑色即可
	if (node)
		rb_set_black(node);
}

void rb_erase(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *child, *parent;
	int color;

	/*
	*如果node有任一空子节点，则记录child并跳转至条件语句后的语句
	*如果两子节点都不空，则进入else分支
	*/
	if (!node->rb_left)
		child = node->rb_right;
	else if (!node->rb_right)
		child = node->rb_left;
	else
	{
		/*
		*将要删除的节点转换成只有一个子节点或者吴子节点
		*先找到它的右子节点，再循环找左子节点
		*知道没有左子节点，找到的节点一定是比node大且紧邻node的节点
		*将找到的此节点放到node位置，并保持node的原有颜色，树的性质不受影响
		*此时问题转化为了删除找到的没有左子节点的节点
		*/
		struct rb_node *old = node, *left;

		node = node->rb_right;
		while ((left = node->rb_left) != NULL)
			node = left;

		/*
		*找到节点之后，将其转移过去
		*首先用old的父节点或root指向新找到的节点node
		*/
		if (rb_parent(old)) {
			if (rb_parent(old)->rb_left == old)
				rb_parent(old)->rb_left = node;
			else
				rb_parent(old)->rb_right = node;
		} else
			root->rb_node = node;

		/*
		*记录找到的node节点的信息，对于这种删除操作，node节点移动至old位置后会沿用old的颜色
		*树的性质不受影响，但原node的位置将失去一个节点，需要记录其信息以便做删除
		*/
		child = node->rb_right;
		parent = rb_parent(node);
		color = rb_color(node);

		if (parent == old) {
			/*
			*参考前面的while循环，如果parent==old，则找到的单子节点只能是old节点的右子节点
			*这里保存的parent可以理解为将用作原来子节点的亲父节点（对于old是node右子节点的情况）
			*node取代old后，原node的父节点还是node
			*/
			parent = node;
		} else {
			/*
			*node节点的删除处理，主要是将相应的父、子给连接起来
			*删的数据是old节点，但将node转移过去占了old的位置
			*所以对于节点结构来说，删除的是node原来的位置
			*/
			if (child)
				rb_set_parent(child, parent);
			parent->rb_left = child;

			node->rb_right = old->rb_right;
			rb_set_parent(old->rb_right, node);
		}

		/*
		*node转移过去之后继续使用old节点的颜色和父节点属性
		*rb_parent_color同时保存了父节点信息和自己的颜色信息
		*/
		node->rb_parent_color = old->rb_parent_color;
		/*
		*之前已经将old的parent的指向old子节点的指针指向了node
		*并且node也已经连接了old的右子节点（如果node与old是子与父的关系，则node保留原右子节点即可）
		*已经完成node指向新父节点即设置颜色的操作，最后只剩下左子节点了
		*/
		node->rb_left = old->rb_left;
		rb_set_parent(old->rb_left, node);

		//已经删除完毕，现在需要检查颜色
		goto color;
	}

	/*
	*这里对应于node有任一空子节点即node无子节点或者有单子节点的情况
	*区别是node可能是右子节点也可能是左子节点
	*/
	parent = rb_parent(node);
	color = rb_color(node);

	if (child)
		rb_set_parent(child, parent);
	if (parent)
	{
		if (parent->rb_left == node)
			parent->rb_left = child;
		else
			parent->rb_right = child;
	}
	else
		root->rb_node = child;

 color:
 /*
 *color是删除节点的颜色，删除的结点的层次正好在child参数与parent参数中间
 *如果删除的是红色节点，对树没有影响，所以只有删除的是黑色节点的时候才需要修正颜色
 */
	if (color == RB_BLACK)
		__rb_erase_color(child, parent, root);
}
EXPORT_SYMBOL(rb_erase);

static void rb_augment_path(struct rb_node *node, rb_augment_f func, void *data)
{
	struct rb_node *parent;

up:
	func(node, data);
	parent = rb_parent(node);
	if (!parent)
		return;

	if (node == parent->rb_left && parent->rb_right)
		func(parent->rb_right, data);
	else if (parent->rb_left)
		func(parent->rb_left, data);

	node = parent;
	goto up;
}

/*
 * after inserting @node into the tree, update the tree to account for
 * both the new entry and any damage done by rebalance
 */
void rb_augment_insert(struct rb_node *node, rb_augment_f func, void *data)
{
	if (node->rb_left)
		node = node->rb_left;
	else if (node->rb_right)
		node = node->rb_right;

	rb_augment_path(node, func, data);
}
EXPORT_SYMBOL(rb_augment_insert);

/*
 * before removing the node, find the deepest node on the rebalance path
 * that will still be there after @node gets removed
 */
struct rb_node *rb_augment_erase_begin(struct rb_node *node)
{
	struct rb_node *deepest;

	if (!node->rb_right && !node->rb_left)
		deepest = rb_parent(node);
	else if (!node->rb_right)
		deepest = node->rb_left;
	else if (!node->rb_left)
		deepest = node->rb_right;
	else {
		deepest = rb_next(node);
		if (deepest->rb_right)
			deepest = deepest->rb_right;
		else if (rb_parent(deepest) != node)
			deepest = rb_parent(deepest);
	}

	return deepest;
}
EXPORT_SYMBOL(rb_augment_erase_begin);

/*
 * after removal, update the tree to account for the removed entry
 * and any rebalance damage.
 */
void rb_augment_erase_end(struct rb_node *node, rb_augment_f func, void *data)
{
	if (node)
		rb_augment_path(node, func, data);
}
EXPORT_SYMBOL(rb_augment_erase_end);

/*
 * This function returns the first node (in sort order) of the tree.
 */
struct rb_node *rb_first(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}
EXPORT_SYMBOL(rb_first);

struct rb_node *rb_last(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}
EXPORT_SYMBOL(rb_last);

struct rb_node *rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (rb_parent(node) == node)
		return NULL;

	/* If we have a right-hand child, go down and then left as far
	   as we can. */
	if (node->rb_right) {
		node = node->rb_right;
		while (node->rb_left)
			node=node->rb_left;
		return (struct rb_node *)node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}
EXPORT_SYMBOL(rb_next);

struct rb_node *rb_prev(const struct rb_node *node)
{
	struct rb_node *parent;

	if (rb_parent(node) == node)
		return NULL;

	/* If we have a left-hand child, go down and then right as far
	   as we can. */
	if (node->rb_left) {
		node = node->rb_left;
		while (node->rb_right)
			node=node->rb_right;
		return (struct rb_node *)node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	while ((parent = rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}
EXPORT_SYMBOL(rb_prev);

void rb_replace_node(struct rb_node *victim, struct rb_node *new,
		     struct rb_root *root)
{
	struct rb_node *parent = rb_parent(victim);

	/* Set the surrounding nodes to point to the replacement */
	if (parent) {
		if (victim == parent->rb_left)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	} else {
		root->rb_node = new;
	}
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, new);

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;
}
EXPORT_SYMBOL(rb_replace_node);

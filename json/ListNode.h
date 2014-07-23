#pragma once


// 链表数据结构 
typedef struct tagNode 
{
	tagNode()
	{
		prev=0;
		next=0;
		value=0;
	}

	tagNode* prev;
	tagNode* next;
	void* value;
}Node, *PNode;


class ListNode
{
public:
	ListNode()
	{
		head=0;
		tail=0;
	}
	~ListNode(){}


	// 获取链表头部节点 
	PNode  GetHead()
	{
		return head;
	}

	// 获取链表尾部节点 
	PNode  GetTail()
	{
		return tail;
	}


	// 获取给定节点的下一个节点 
	PNode  next(PNode lpNode)
	{
		return lpNode->next;
	}

	// 获取给定节点的上一个节点 
	PNode  prev(PNode lpNode) 
	{
		return lpNode->prev;
	}

	// 在链表头部插入一个节点 
	void  PushFront(PNode lpNode)
	{
		lpNode->prev=0;
		lpNode->next=head;
		if(head)
		{
			head->prev=lpNode;
		}
		else
		{
			tail=lpNode;
		}
		head=lpNode;
	}

	// 在链表尾部插入一个节点 
	void  PushBack(PNode lpNode) 
	{
		lpNode->next=0;
		lpNode->prev=tail;
		if(tail)
		{
			tail->next=lpNode;
		}
		else
		{
			head=lpNode;
		}
		tail=lpNode;
	}


	// 在指定节点后插入一个节点 
	void  insert(PNode lpAfter,PNode lpNode)
	{
		if(lpAfter) 
		{
			if(lpAfter->next)
			{
				lpAfter->next->prev =lpNode;
			}
			else
			{
				tail=lpNode;
			}
			lpNode->prev=lpAfter;
			lpNode->next=lpAfter->next;
			lpAfter->next =lpNode;
		} 
		else 
		{
			PushFront(lpNode);
		}
	}

	// 从链表头部弹出一个节点 
	PNode  PopFront() 
	{
		if(head) 
		{
			PNode lpNode=head;
			if(head->next)
			{
				head->next->prev=0;
			}
			else
			{
				tail=0;
			}
			head=head->next;
			lpNode->prev=lpNode->next=0;
			return lpNode;
		} 
		else 
		{
			return 0;
		}
	}

	// 从链表尾部弹出一个节点 
	PNode  PopBack() 
	{
		if(tail) 
		{
			PNode lpNode=tail;
			if(tail->prev)
			{
				tail->prev->next=0;
			}
			else
			{
				head=0;
			}
			tail=tail->prev;
			lpNode->prev=lpNode->next=0;
			return lpNode;
		} 
		else 
		{
			return 0;
		}
	}

	// 从链表中删除给定节点 
	PNode  remove(PNode lpNode) 
	{
		if(lpNode->prev)
		{
			lpNode->prev->next=lpNode->next;
		}
		else
		{
			head=lpNode->next;
		}
		if(lpNode->next)
		{
			lpNode->next->prev=lpNode->prev;
		}
		else
		{
			tail=lpNode->prev;
		}
		return lpNode;
	}

	// 检查链表是否为空 
	bool  empty() 
	{
		if(head || tail)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	// 获取链表中的节点数 
	long size() 
	{
		long lnSize =0;
		PNode lpNode=head;
		while(lpNode)
		{
			++lnSize;
			lpNode=next(lpNode);
		}
		return lnSize;
	}

	// 合并两个链表 
	ListNode* combine(ListNode* other) 
	{
		if(!other->empty()) 
		{
			if(!empty()) 
			{
				tail->next =other->head;
				other->head->prev =tail;
				tail =other->tail;
			}
			else 
			{
				head =other->head;
				tail =other->tail;
			}
			other->head =other->tail =0;
		}
		return this;
	} 
private:
	PNode head;
	PNode tail;
};


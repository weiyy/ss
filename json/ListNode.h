#pragma once


// �������ݽṹ 
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


	// ��ȡ����ͷ���ڵ� 
	PNode  GetHead()
	{
		return head;
	}

	// ��ȡ����β���ڵ� 
	PNode  GetTail()
	{
		return tail;
	}


	// ��ȡ�����ڵ����һ���ڵ� 
	PNode  next(PNode lpNode)
	{
		return lpNode->next;
	}

	// ��ȡ�����ڵ����һ���ڵ� 
	PNode  prev(PNode lpNode) 
	{
		return lpNode->prev;
	}

	// ������ͷ������һ���ڵ� 
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

	// ������β������һ���ڵ� 
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


	// ��ָ���ڵ�����һ���ڵ� 
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

	// ������ͷ������һ���ڵ� 
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

	// ������β������һ���ڵ� 
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

	// ��������ɾ�������ڵ� 
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

	// ��������Ƿ�Ϊ�� 
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

	// ��ȡ�����еĽڵ��� 
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

	// �ϲ��������� 
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


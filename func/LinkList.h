#pragma once

#include "Lock.h"
/////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
class CLinkList
{
private:
	CCriticalSection cs;
	
	T * first;
	T * last;
	
	uint32_t item_count;
	
public:
	CLinkList()
	{
		Init();
	}
	
	~CLinkList()
	{
	}
	
	void Init()
	{
		first = NULL;
		last = NULL;
		item_count = 0;
	}
	
	void Lock()
	{
		cs.Lock();
	}
	
	void Unlock()
	{
		cs.Unlock();
	}
	
	uint32_t GetCount()
	{
		return item_count;
	}
	
	void AddTail(T * t)
	{
		t->next = NULL;
		
		cs.Lock();
		if(first)
		{
			t->prev = last;
			last->next = t;
		}
		else
		{
			t->prev = NULL;
			first = t;
		}
		last = t;
		
		item_count++;
		cs.Unlock();
	}
	
	void AddHead(T * t)
	{
		t->prev = NULL;
		
		cs.Lock();
		if(first)
		{
			first->prev = t;
			t->next = first;
		}
		else
		{
			t->prev = NULL;
			last = t;
		}
		first = t;
		
		item_count++;
		cs.Unlock();
	}
	
	T* GetHeadAndRemove()
	{
		T * ret;
		
		cs.Lock();
		if(first)
		{
			ret = first;
			
			first = first->next;
			
			if(!first)
			{
				last = NULL;
			}
			else
			{
				first->prev = NULL;
			}
			
			item_count--;
		}
		else
		{
			ret = NULL;
		}
		cs.Unlock();
		
		return ret;
	}
	
	T* GetHead()
	{
		T * ret;
		
		cs.Lock();
		if(first)
		{
			ret = first;
		}
		else
		{
			ret = NULL;
		}
		cs.Unlock();
		
		return ret;
	}
	
	T* Next(T * t)
	{
		T * ret;
		
		cs.Lock();
		if(t)
		{
			ret = t->next;
		}
		else
		{
			ret = NULL;
		}
		cs.Unlock();
		
		return ret;
	}
	
	T* GetTail()
	{
		T * ret;
		
		cs.Lock();
		if(last)
		{
			ret = last;
		}
		else
		{
			ret = NULL;
		}
		cs.Unlock();
		
		return ret;
	}
	
	void MoveToHead(T* t)
	{	
		cs.Lock();
		
		if(t != first)
		{
            t->prev->next = t->next;
			
			first->prev = t;

            if(t == last)
			{
				last = t->prev;
			}
			else
			{
                t->next->prev = t->prev;
			}

            t->prev = NULL;
            t->next = first;

            first = t;
		}
		
		cs.Unlock();
	}
	
	void MoveToTail(T* t)
	{
		cs.Lock();
		
		if(t != last)
		{
            t->next->prev = t->prev;
			
			last->next = t;
			
            if(t == first)
			{
				first = t->next;
			}
			else
			{
                t->prev->next = t->next;
			}

            t->next = NULL;
            t->prev = last;

            last = t;
		}
		
		cs.Unlock();
	}
	
	void Remove(T* t)
	{
		cs.Lock();
		if(t->prev)
		{
			t->prev->next = t->next;
		}
		
		if(t->next)
		{
			t->next->prev = t->prev;
		}
		
		if(t == first)
		{
			first = t->next;
		}
		
		if(t == last)
		{
			last = t->prev;
		}
		
		item_count--;
		cs.Unlock();
	}
	
	void RemoveAll()
	{
		cs.Lock();
		first = NULL;
		last = NULL;
		item_count = 0;
		cs.Unlock();
	}
};














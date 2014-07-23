#pragma once
/*
 SwapQueue
 by Shasure 2011.5.3
 */

#include "ObjectPool.h"
#include "Lock.h"


template <class T>
class CSwapQueue
{
	public:
	struct SWAPQUEUENODE
	{
		T *t;
		SWAPQUEUENODE * next;
	};

	CSwapQueue(int initsize, int gran)
	{
		m_queue_buffer_0 = new CObjectPool<SWAPQUEUENODE>(initsize, gran, false);
		m_queue_buffer_1 = new CObjectPool<SWAPQUEUENODE>(initsize, gran, false);

		m_queue_0 = NULL;
		m_queue_1 = NULL;
		m_queue_0_end = NULL;
		m_queue_1_end = NULL;
		m_swapper = 0;
	}

	/*
	*/
	~CSwapQueue(void)
	{
		delete m_queue_buffer_0;
		delete m_queue_buffer_1;
	}

	/*
	*/
	T * Get()
	{
		T * ret;
		//
		for(;;)
		{
			m_EvSwap.Lock();
			if(m_queue_0 || m_queue_1)
			{
				m_EvSwap.Unlock();
				break;
			}
			m_EvSwap.Wait();
			m_EvSwap.Unlock();
		}
		//while(!m_queue_0 && !m_queue_1)
		//	m_EvSwap.Wait();

		//check if need swap
		if((m_swapper == 0 && !m_queue_0) || (m_swapper == 1 && !m_queue_1))
		{
			m_CSLock.Lock();
			m_swapper = !m_swapper;
			m_CSLock.Unlock();
		}

		if(m_swapper == 0)
		{
			ret = m_queue_0->t;

			return ret;
		}
		else
		{
			ret = m_queue_1->t;

			return ret;
		}
	}

	void Pop()
	{
		if(m_swapper == 0)
		{
			SWAPQUEUENODE * temp = m_queue_0;
			m_queue_0 = m_queue_0->next;
			m_queue_buffer_0->PushBackNode(temp);
		}
		else
		{
			SWAPQUEUENODE * temp = m_queue_1;
			m_queue_1 = m_queue_1->next;
			m_queue_buffer_1->PushBackNode(temp);
		}
	}

	/*
	*/
	void Put(T * t)
	{
		m_CSLock.Lock();
		if(m_swapper == 1)
		{
			SWAPQUEUENODE * temp = m_queue_buffer_0->GetFreeNode();
			temp->next = NULL;
			temp->t = t;
			if(!m_queue_0)
			{
				m_queue_0 = temp;
			}
			else
			{
				m_queue_0_end->next = temp;
			}
			m_queue_0_end = temp;
		}
		else
		{
			SWAPQUEUENODE * temp = m_queue_buffer_1->GetFreeNode();
			temp->next = NULL;
			temp->t = t;
			if(!m_queue_1)
			{
				m_queue_1 = temp;
			}
			else
			{
				m_queue_1_end->next = temp;
			}
			m_queue_1_end = temp;
		}
		m_CSLock.Unlock();

		m_EvSwap.Lock();
		m_EvSwap.Post();
		m_EvSwap.Unlock();
	}


	private:
		CObjectPool<SWAPQUEUENODE> * m_queue_buffer_0, * m_queue_buffer_1;

		SWAPQUEUENODE * m_queue_0, * m_queue_1;
		SWAPQUEUENODE * m_queue_0_end, * m_queue_1_end;
		int m_swapper;
		CCriticalSection m_CSLock;
		CEvent	m_EvSwap;
};

#pragma once
/*
ObjectPool �����ڴ��
by Shasure 2010.10.19
*/

//�ڴ�������� BYTE
#define MEMORYPAGESIZE	4096

#include "Lock.h"

typedef bool	BOOL;
typedef uint32_t DWORD;

template <class T>
class CObjectPool
{
public:
	struct OBJECTLINKEDLISTNODE
	{
		T tObject;
		OBJECTLINKEDLISTNODE * pNext;
	};

	struct ALLOCMEMNODE
	{
		OBJECTLINKEDLISTNODE * pObject;
		ALLOCMEMNODE * pNext;
	};

	/*
	���캯��
	nInitSize ��ʼ������ؽڵ����
	nGran �ڴ��������(��4KΪ1����λ)
	*/
	CObjectPool(DWORD nInitSize, DWORD nGran, bool bNeedLock = true)
	{
		m_bNeedLock = bNeedLock;
		m_nGran = nGran;
 		m_AllocMemNodeFirst = NULL;

		DWORD nBestSize = ((nInitSize * sizeof(T)) / (MEMORYPAGESIZE*nGran) + ((nInitSize * sizeof(T)) % (MEMORYPAGESIZE*nGran) == 0 ? 0 : 1)) * (MEMORYPAGESIZE*nGran) / sizeof(T);

		OBJECTLINKEDLISTNODE * na = new OBJECTLINKEDLISTNODE[nBestSize];
		m_TList = na;

		m_AllocMemNodeFirst = new ALLOCMEMNODE;
		m_AllocMemNodeFirst->pObject = na;
		m_AllocMemNodeFirst->pNext = NULL;

		for(DWORD i=0; i<nBestSize; i++)
		{
			if(0 == i)
			{
				na[i].pNext = NULL;
				m_FreeNodeLast = &na[i];
			}
			else
				na[i].pNext = m_FreeNodeFirst;

			m_FreeNodeFirst = &na[i];
		}


		m_UsedCount = 0;
		m_Count = nBestSize;
	}

	/*
	��������
	*/
	~CObjectPool(void)
	{
 		ALLOCMEMNODE * n = m_AllocMemNodeFirst;
 		while(n)
 		{
 			ALLOCMEMNODE * t = n->pNext;
			delete n->pObject;
 			delete n;
 			n = t;
 		}
	}

	/*
	ȡ��һ�����нڵ�
	*/
	T * GetFreeNode()
	{
		OBJECTLINKEDLISTNODE * n;

		if(m_bNeedLock)m_CSLock.Lock();
		//Free��
		if(m_FreeNodeFirst)
		{
			n = m_FreeNodeFirst;
			if(m_FreeNodeLast == m_FreeNodeFirst)
				m_FreeNodeLast = NULL;
			m_FreeNodeFirst = m_FreeNodeFirst->pNext;
		}
		else
		{
			DWORD nAllocSize = (MEMORYPAGESIZE*m_nGran) / sizeof(T);
			OBJECTLINKEDLISTNODE * na = new OBJECTLINKEDLISTNODE[nAllocSize];

			ALLOCMEMNODE * mn = new ALLOCMEMNODE;
			mn->pObject = na;
			mn->pNext = m_AllocMemNodeFirst;
			m_AllocMemNodeFirst = mn;

			n = &na[0];
			for(DWORD i=1; i<nAllocSize; i++)
			{
				if(i == 1)
					m_FreeNodeLast = &na[1];

				na[i].pNext = m_FreeNodeFirst;

				m_FreeNodeFirst = &na[i];
			}

			m_Count += nAllocSize;
		}
		n->pNext = (OBJECTLINKEDLISTNODE*)0xFFFFFFFF;

		m_UsedCount ++;
		if(m_bNeedLock)m_CSLock.Unlock();

		//����ֵ
		return (T*)n;
	}

	/*
	����һ���ڵ�
	*/
	void PushBackNode(T * t)
	{
		OBJECTLINKEDLISTNODE * n = (OBJECTLINKEDLISTNODE*)t;

		if(m_bNeedLock)m_CSLock.Lock();
		if( (!n) || ((OBJECTLINKEDLISTNODE*)0xFFFFFFFF != n->pNext) )
		{
			if(m_bNeedLock)m_CSLock.Unlock();
			//printf("\nPushBackNode : NULL\n");
			return;
		}

		//Free��
		//n->pNext = m_FreeNodeFirst;
		//m_FreeNodeFirst = n;
		n->pNext = NULL;
		if(m_FreeNodeLast)
		{
			m_FreeNodeLast->pNext = n;
		}
		m_FreeNodeLast = n;
		if(!m_FreeNodeFirst)
		{
			m_FreeNodeFirst = n;
		}

		m_UsedCount --;
		if(m_bNeedLock)m_CSLock.Unlock();
	}

	/*
	�ڵ��Ƿ�Free
	*/
	BOOL IsNodeFree(T * t)
	{
		BOOL bRet;
		OBJECTLINKEDLISTNODE * n = (OBJECTLINKEDLISTNODE*)t;

		if(m_bNeedLock)m_CSLock.Lock();
		bRet = ((OBJECTLINKEDLISTNODE*)0xFFFFFFFF != n->pNext);
		if(m_bNeedLock)m_CSLock.Unlock();

		return bRet;
	}

public:
	volatile long m_Count;
	volatile long m_UsedCount;

	OBJECTLINKEDLISTNODE * m_TList;

private:
	OBJECTLINKEDLISTNODE * m_FreeNodeFirst;
	OBJECTLINKEDLISTNODE * m_FreeNodeLast;
	ALLOCMEMNODE * m_AllocMemNodeFirst;
	DWORD m_nGran;
	bool m_bNeedLock;

	CCriticalSection m_CSLock;
};

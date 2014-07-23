#pragma once
/*
MemPool
by Shasure 2012.1.30
*/

#define MEMORYPAGESIZE	4096

#include <sys/mman.h>
#include "Lock.h"

#define MEMPOOL_USE_MMAP
///////////////////////////////////////////////////////////////////////////////////////////////
struct MEMPOOL_ITEM
{
    void * some_pointer;	//free:next MEMPOOL_ITEM    or    inuse:which MEMPOOL_UNIT

    char mem[];
};


struct MEMPOOL_UNIT
{
    int item_size;

    int gran_item_num;

    MEMPOOL_ITEM * item_first;

    MEMPOOL_UNIT * next;
};


struct MEMPOOL_ALLOC_TABLE
{
	void * alloc_mem;
	
	size_t total_alloc_size;
	
	MEMPOOL_ALLOC_TABLE * next;
};


class CMemPool
{
public:
    CCriticalSection mempool_locker;
    MEMPOOL_UNIT * unit_first;
	MEMPOOL_ALLOC_TABLE * alloc_table_first;
	MEMPOOL_ALLOC_TABLE * alloc_table_new_first;
	
private:
	void AddToAllocTable(MEMPOOL_ALLOC_TABLE ** mat, void* p, size_t total_alloc_size)//新来节点加在链表头
	{
		MEMPOOL_ALLOC_TABLE * a_t = new MEMPOOL_ALLOC_TABLE;
		a_t->alloc_mem = p;
		a_t->total_alloc_size = total_alloc_size;
		a_t->next = *mat;
		*mat = a_t;
	}
	
	void RemoveFromAllocTable(MEMPOOL_ALLOC_TABLE ** mat, void* p)
	{
		MEMPOOL_ALLOC_TABLE * prev = NULL;
		MEMPOOL_ALLOC_TABLE * it = *mat;
		while(it)
		{
			if(it->alloc_mem == p)
			{
				if(!prev)
				{
					*mat = it->next;
					
#ifdef MEMPOOL_USE_MMAP
					munmap(it->alloc_mem, it->total_alloc_size);
#else
					delete[] (char*)it->alloc_mem;
#endif
					
					delete it;
				}
				else
				{
					prev->next = it->next;
					
#ifdef MEMPOOL_USE_MMAP
					munmap(it->alloc_mem, it->total_alloc_size);
#else
					delete[] (char*)it->alloc_mem;
#endif
					
					delete it;
				}
				
				break;
			}
			
			prev = it;
			it = it->next;
		}
	}
	
	inline MEMPOOL_ITEM * AllocNewItems(int item_size_real, int gran_item_num)
	{
		MEMPOOL_ITEM * new_items;
		
#ifdef MEMPOOL_USE_MMAP		
		gran_item_num += ( (4096 - ((item_size_real * gran_item_num) % 4096)) / item_size_real );
#endif
		
		size_t total_alloc_size = item_size_real * gran_item_num;

#ifdef MEMPOOL_USE_MMAP
		new_items = (MEMPOOL_ITEM*)mmap(NULL, total_alloc_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,-1,0);
#else
		new_items = (MEMPOOL_ITEM*) new char[total_alloc_size];
#endif

        AddToAllocTable(&alloc_table_first, new_items, total_alloc_size);

        MEMPOOL_ITEM * item = new_items;
        for (int i=0; i<gran_item_num - 1; i++)
        {
            item->some_pointer = (void*)((char *)new_items + item_size_real * (i+1));

            item = (MEMPOOL_ITEM*)item->some_pointer;
        }
        //the last item
        item->some_pointer = NULL;
          
        return new_items;
	}

public:
    /*
     */
    CMemPool()
    {
      unit_first = NULL;
			alloc_table_first = NULL;
			alloc_table_new_first = NULL;
    }

    /*
     */
    ~CMemPool()
	{
		MEMPOOL_ALLOC_TABLE * temp_alloc = alloc_table_first;
		while(temp_alloc)
		{
			MEMPOOL_ALLOC_TABLE * temp_delete = temp_alloc;
			void * mem = temp_alloc->alloc_mem;
			temp_alloc = temp_alloc->next;
			
#ifdef MEMPOOL_USE_MMAP
			munmap(mem, temp_delete->total_alloc_size);
#else
			delete[] (char *)mem;
#endif
			
			delete temp_delete;
		}
		
		temp_alloc = alloc_table_new_first;
		while(temp_alloc)
		{
			MEMPOOL_ALLOC_TABLE * temp_delete = temp_alloc;
			void * mem = temp_alloc->alloc_mem;
			temp_alloc = temp_alloc->next;
			
#ifdef MEMPOOL_USE_MMAP
			munmap(mem, temp_delete->total_alloc_size);
#else
			delete[] (char *)mem;
#endif
			
			delete temp_delete;
		}
		
		MEMPOOL_UNIT * temp_unit = unit_first;
		while(temp_unit)
		{
			MEMPOOL_UNIT * temp_delete = temp_unit;
			temp_unit = temp_unit->next;
			
			delete temp_delete;
		}
	}

    /*
     */
    int AddUnit(int item_size, int gran_pages)
    {
        MEMPOOL_UNIT * unit = NULL;

        mempool_locker.Lock();

        if (unit_first)
        {
            MEMPOOL_UNIT * curr_unit = unit_first;
            MEMPOOL_UNIT * prev_unit = NULL;
            while (curr_unit)
            {
                if (curr_unit->item_size > item_size)
                {
                    unit = new MEMPOOL_UNIT;
                    unit->next = curr_unit;

                    if (prev_unit == NULL)
                    {
                        unit_first = unit;
                    }
                    else
                    {
                        prev_unit->next = unit;
                    }

                    break;
                }
                else if (curr_unit->item_size == item_size)
                {
                    mempool_locker.Unlock();
                    return 0;
                }

                prev_unit = curr_unit;
                curr_unit = curr_unit->next;
            }//while

            if (!unit)
            {
                unit = new MEMPOOL_UNIT;
                unit->next = NULL;

                if (prev_unit == NULL)
                {
                    unit_first = unit;
                }
                else
                {
                    prev_unit->next = unit;
                }
            }
        }
        else
        {
            unit = new MEMPOOL_UNIT;
            unit->next = NULL;
            unit_first = unit;
        }

        int item_size_real = sizeof(MEMPOOL_ITEM) + item_size;
        int item_count_per_page = (MEMORYPAGESIZE*gran_pages) / item_size_real;

        unit->item_size = item_size;
        unit->gran_item_num = item_count_per_page > 0 ? item_count_per_page : 1;
        int ret_count = unit->gran_item_num;

        unit->item_first = AllocNewItems(item_size_real, unit->gran_item_num);

        mempool_locker.Unlock();

        return ret_count;
    }


    /*
     */
    void * Alloc(int item_size)
    {
      MEMPOOL_ITEM * ret_item = NULL;
		
			mempool_locker.Lock();

        MEMPOOL_UNIT * curr_unit = unit_first;
        while (curr_unit)
        {
            if (curr_unit->item_size >= item_size)
            {
                if (!curr_unit->item_first)
                {
                    int item_size_real = sizeof(MEMPOOL_ITEM) + curr_unit->item_size;

                    curr_unit->item_first = AllocNewItems(item_size_real, curr_unit->gran_item_num);
                }

                ret_item = curr_unit->item_first;
                curr_unit->item_first = (MEMPOOL_ITEM*)ret_item->some_pointer;
                ret_item->some_pointer = curr_unit;

                break;
            }

            curr_unit = curr_unit->next;
        }//while
        		
        mempool_locker.Unlock();

		if(!ret_item)
		{
			size_t total_alloc_size = sizeof(MEMPOOL_ITEM) + item_size;
#ifdef MEMPOOL_USE_MMAP
			ret_item = (MEMPOOL_ITEM*)mmap(NULL, total_alloc_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,-1,0);
#else
			ret_item = (MEMPOOL_ITEM*)new char[total_alloc_size];
#endif
			ret_item->some_pointer = NULL;
			mempool_locker.Lock();
			AddToAllocTable(&alloc_table_new_first, ret_item, total_alloc_size);
			mempool_locker.Unlock();
		}//if

        return (void*)((char *)ret_item + sizeof(MEMPOOL_ITEM));
    }
    

	/*
	 */
    void Free(void * p)
    {
        MEMPOOL_ITEM * free_item = (MEMPOOL_ITEM*)((char *)p - sizeof(MEMPOOL_ITEM));
		
		if(free_item->some_pointer == NULL)
		{
			mempool_locker.Lock();
			RemoveFromAllocTable(&alloc_table_new_first, free_item);
			mempool_locker.Unlock();
			return;
		}
		
		mempool_locker.Lock();
		
		MEMPOOL_UNIT * unit = (MEMPOOL_UNIT*)free_item->some_pointer;
		free_item->some_pointer = unit->item_first;
		unit->item_first = free_item;
		
		mempool_locker.Unlock();
    }
};






























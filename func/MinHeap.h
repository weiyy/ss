template<class T>

class MinHeap
{
public:
    T *heap;

private:
    int CurrentSize; //目前元素个数
    int MaxSize; //可容纳的最多元素个数

private:
	void FilterUp(int start) //自下往上调整
    {
        int j = start, i = (j-1)/2; //i指向j的双亲节点
        T temp = heap[j];
        while (j > 0)
        {
            if (heap[i]->sort_num <= temp->sort_num)
                break;
            else
            {
                heap[j] = heap[i];
                j = i;
                i= (i-1)/2;
            }
        }
        heap[j] = temp;
    }

    void FilterDown(const int start, const int end) //自上往下调整，使关键字小的节点在上
    {
        int i = start, j = 2*i+1;
        T temp = heap[i];
        while (j <= end)
        {
            if ( (j < end) && (heap[j]->sort_num > heap[j+1]->sort_num) )
                j++;
            if (temp->sort_num <= heap[j]->sort_num)
                break;
            else
            {
                heap[i] = heap[j];
                i = j;
                j = 2*j+1;
            }
        }
        heap[i] = temp;
    }

    void Sort(T *a, int x, int y)
    {
        int xx = x, yy = y;
        T k = a[x];
        if (x >= y)
            return;
        while (xx != yy)
        {
            while (xx < yy && a[yy]->sort_num >= k->sort_num)
                yy--;
            a[xx] = a[yy];
            while (xx < yy && a[xx]->sort_num <= k->sort_num)
                xx++;
            a[yy] = a[xx];
        }
        a[xx] = k;
        Sort(a, x, xx-1);
        Sort(a, xx+1, y);
    }
    
    
    T RemoveMin()
    {
        T x = heap[0];
        heap[0] = heap[CurrentSize-1];
        CurrentSize--;
        FilterDown(0, CurrentSize-1); //调整新的根节点
        return x;
    }

public:
    MinHeap(int n)
    {
        MaxSize = n;
        heap = new T[MaxSize];
        CurrentSize = 0;
    }

    ~MinHeap()
    {
        delete []heap;
    }
    
    T Insert(const T x)
    {
		T ret = NULL;
		
        if (CurrentSize == MaxSize)
        {
            if (x->sort_num > heap[0]->sort_num)
            {
                ret = RemoveMin();
            }
            else
            {
                return x;
            }
        }

        heap[CurrentSize] = x;
        FilterUp(CurrentSize);
        CurrentSize++;
        return ret;
    }

    T GetMin()
    {
        return heap[0];
    }

    bool IsEmpty() const
    {
        return CurrentSize == 0;
    }

    bool IsFull() const
    {
        return CurrentSize == MaxSize;
    }

    void Clear()
    {
        CurrentSize = 0;
    }
    
    int Size()
	{
		return CurrentSize;
	}
    
    void QuickSort()
	{
		Sort(heap, 0, CurrentSize - 1);
	}
};
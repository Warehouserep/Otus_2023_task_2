#include <iostream>
#include <map>
#include <stdlib.h>
#include <cstring>

using namespace std;

template<typename T,
         int batch_size,
         int expand_strategy, // опция для расширения
         int one_free_strategy> // опция для поэлементного освобождения
class SizedAllocator {
private:
    
    typedef struct MemBlock {
        T* m_memPtr;
        int m_nCapacity;
        int m_nSize;

        MemBlock* next;
    } MemBlock;
    
public:

  typedef size_t     size_type;
  typedef ptrdiff_t  difference_type;
  typedef T*         pointer;
  typedef const T*   const_pointer;
  typedef T&         reference;
  typedef const T&   const_reference;
  typedef T          value_type;

  SizedAllocator(): m_firstBlock(nullptr) {}
  ~SizedAllocator() {
      while (m_firstBlock) {
          MemBlock* save_next = m_firstBlock->next;
          free(m_firstBlock->m_memPtr);
          free(m_firstBlock);
          m_firstBlock = save_next;
      }
  }

  template<typename X>
  struct rebind
  { typedef SizedAllocator<X, batch_size, expand_strategy, one_free_strategy> other; };

  pointer address(reference _ref) const {
      return &_ref;
  }

  const_pointer address(const_reference _cref) const {
      return &_cref;
  }

  pointer allocate(size_type _n) {
      if (!one_free_strategy) {
          MemBlock* goodBlock = findGoodBlock(_n);
          if (!expand_strategy && m_firstBlock && !goodBlock) {
              // нерасширяемый аллокатор, а память нужно расширить
              throw logic_error("can not expand memory");
          }

          T* result;
          if (goodBlock) {
              result = goodBlock->m_memPtr + goodBlock->m_nSize;
              goodBlock->m_nSize += _n;
          } else {
              goodBlock = createNewBlock(_n);
              if(m_firstBlock) {
                  MemBlock* current = m_firstBlock;
                  while (current->next) {
                      current = current->next;
                  }
                  current->next = goodBlock;
              } else {
                  m_firstBlock = goodBlock;
              }
              result = goodBlock->m_memPtr;
              goodBlock->m_nSize += _n;
          }
          return result;
      } else {
         T* newPtr = (T*)malloc(sizeof(T) * _n);
         return newPtr;
      }
  }

  void deallocate(pointer _p, size_type) {
      if (one_free_strategy) {
          free(_p);
      }
  }

  size_type max_size() const throw() {
    if (one_free_strategy) return size_t(-1) / sizeof(T);
    if (!expand_strategy) return batch_size;
    return size_t(-1) / sizeof(T);
  }

  void construct(pointer _p, const T& _val) {
    new(_p) T(_val);
  }

  void destroy(pointer _p) {
    _p->~T();
  }

  private:

    MemBlock* createNewBlock(int _size) {
        MemBlock* newBlock = (MemBlock*)malloc(sizeof(MemBlock));

        newBlock->m_nSize = 0;
        newBlock->m_nCapacity = (_size / batch_size + 1) * batch_size;
        newBlock->m_memPtr = (T*)malloc(sizeof(T) * newBlock->m_nCapacity);
        newBlock->next = nullptr;

        return newBlock;
    }

    MemBlock* findGoodBlock(int _size) {
        MemBlock* current = m_firstBlock;
        while (current != nullptr) {
            if (current->m_nCapacity - current->m_nSize >= _size)
                return current;
            current = current->next;
        }
        return nullptr;
    }

    MemBlock* m_firstBlock;
};

template<typename T, int batch_size, int expand_strategy, int one_free_strategy>
inline bool operator==(const SizedAllocator<T, batch_size, expand_strategy, one_free_strategy>&, const SizedAllocator<T, batch_size, expand_strategy, one_free_strategy>&) {
  return true;
}

template<typename T, int batch_size, int expand_strategy, int one_free_strategy>
inline bool operator!=(const SizedAllocator<T, batch_size, expand_strategy, one_free_strategy>&, const SizedAllocator<T, batch_size, expand_strategy, one_free_strategy>&) {
  return false;
}

template<typename T, typename Alloc>
class MyVector {
public:

    class MyIterator {
    public:
        MyIterator(T* _data, int _offset = 0): m_data(_data), m_nOffset(_offset) {}

        MyIterator& operator++() {
            m_nOffset++;
            return *this;
        }

        T& operator *() {
            return m_data[m_nOffset];
        }

        bool operator != (const MyIterator& rhs) {
            return this->m_nOffset != rhs.m_nOffset;
        }
    private:
        T*  m_data;
        int m_nOffset;
    };

    MyVector(): m_nSize(0), m_nCapacity(4) {
        m_array = m_allocator.allocate(m_nCapacity);
    }
    ~MyVector() {
        if(m_array) {
            m_allocator.deallocate(m_array, m_nCapacity);
            m_array = nullptr;
        }
    }
    int size() {
        return m_nSize;
    }
    bool empty() {
        return !m_nSize;
    }
    void push_back(const T& el) {
        if (m_nSize == m_nCapacity) {
            m_nCapacity *= 1.5;
            T* newMem = m_allocator.allocate(m_nCapacity);
            memcpy(newMem, m_array, m_nSize * sizeof(T));
            m_allocator.deallocate(m_array, m_nSize);
            m_array = newMem;
        }
        m_array[m_nSize++] = el;
    }

    MyIterator begin() {
        return MyIterator(m_array);
    }

    MyIterator end() {
        return MyIterator(m_array, m_nSize);
    }
private:
    Alloc m_allocator;
    T* m_array;
    int m_nSize;
    int m_nCapacity;
};

int factorial(int n) {
    if (n <= 1) return 1;
    return n*factorial(n-1);
}

constexpr int my_batch_size = 5;

int main()
{
    // стандартный аллокатор
    map<int, int> def_fact_c;
    for(int i=0; i<10; i++)
        def_fact_c.insert(pair<int,int>(i, factorial(i)));
    // мой аллокатор
    using MyExpandableAlloc = SizedAllocator<pair<int, int>, my_batch_size, 0, 1>;
    map<
            int,
            int,
            less<int>,
            MyExpandableAlloc> my_fact_c;
    for(int i=0; i<10; i++)
        my_fact_c.insert(pair<int,int>(i, factorial(i)));

    // вывод значений
    for (auto& p:my_fact_c) {
       printf("%d %d\n", p.first, p.second);
    }

    // стандартный аллокатор
    MyVector<int, allocator<int>> my_vector_def_alloc;
    for(int i=0; i<10; i++)
        my_vector_def_alloc.push_back(i);

    // вывод значений
    for(auto it = my_vector_def_alloc.begin();
        it != my_vector_def_alloc.end(); ++it){
        printf("%d ", *it);
     }
    printf("\n");

    // мой аллокатор
    using MyExpandableAlloc2 = SizedAllocator<int, my_batch_size, 1, 0>;
    MyVector<int, MyExpandableAlloc2> my_vector_my_alloc;
    for(int i=0; i<10; i++)
        my_vector_my_alloc.push_back(i);

    // вывод значений
    for(auto it = my_vector_my_alloc.begin();
        it != my_vector_my_alloc.end(); ++it){
        printf("%d ", *it);
     }

    return 0;
}

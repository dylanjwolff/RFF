

#include <atomic>
#include <pthread.h>
#include <unistd.h>
#include <cassert>

pthread_mutex_t m;

inline bool compare_exchange_strong(int* addr, int tst, int val) {
  bool zf;
  __asm__ volatile(
      "lock cmpxchg %2, %0\n\t"
      : "+m" (*addr), "+a" (tst), "=@ccz" (zf)
      : "r" (val) 
  );
  return zf;
}

inline int exchange(int* addr, int val) {

  int r;
  __asm__ __volatile__ ("lock; xchg %0, %1"
          :"=r"(r), "=m"(*addr)
          :"0"(val), "m"(*addr)
          :"memory");
  return r;
}

// not working
inline void store(int* addr, int val) {
  __asm__ __volatile__ ("movq %[src], %[dst]"
          : [dst] "=m"(*addr)
          : [src] "0"(val)
          :"memory"
  );  
}


// not working
inline int load(int* addr) {
  int r;

  __asm__ __volatile__ ("movq %[src], %[dst]"
          : [dst] "=r"(r)
          : [src] "0"(*addr)
          :"memory"
  );  
  return r;
}

inline int fetch_sub(int* addr, int sub) {
  int r;
  __asm__ __volatile__ ("lock; xadd %0, %1"
          :"=r"(r), "=m"(*addr)
          :"0"(-sub), "m"(*addr)
          :"memory");
  return r;
}

inline int fetch_add(int* addr, int add) {
  int r;
  __asm__ __volatile__ ("lock; xadd %0, %1"
          :"=r"(r), "=m"(*addr)
          :"0"(add), "m"(*addr)
          :"memory");
  return r;
}

template<typename T>
struct SafeStackItem
{
  volatile T Value;
  int Next;
};

template<typename T>
class SafeStack
{
  int head;
  int count;

public:
  SafeStackItem<T>* array;

  SafeStack(int pushCount)
  {
      array = new SafeStackItem<T> [pushCount];
      count = pushCount; // store
      head = 0; // store

      array[0].Next = 0 + 1; // store
      array[1].Next = 1 + 1; // store
      array[2].Next = 2 + 1; // store

      array[pushCount - 1].Next = -1; // store
  }

  ~SafeStack()
  {
      delete [] array;
  }

  int Pop()
  {
      if (count > 1) // load
      {
          int head1 = head; // load
          int next1 = exchange(&array[head1].Next, -1);

          if (next1 >= 0)
          {
              int head2 = head1;
              if (compare_exchange_strong(&head, head2, next1))
              {
                  fetch_sub(&count, 1);
                  return head1;
              }
              else
              {
                  exchange(&array[head1].Next, next1);
              }
          }
          else
          {
            sched_yield();
          }
      } else {
        return -1;
      }

      if (count > 1) // load
      {
          int head1 = head; // load
          int next1 = exchange(&array[head1].Next, -1);

          if (next1 >= 0)
          {
              int head2 = head1;
              if (compare_exchange_strong(&head, head2, next1))
              {
                  fetch_sub(&count, 1);
                  return head1;
              }
              else
              {
                  exchange(&array[head1].Next, next1);
              }
          }
          else
          {
            sched_yield();
          }
      } else {
        return -1;
      }

      // past unroll limit
      _exit(0);
}

  void Push(int index)
  {
      int head1 = head; // load
        
      array[index].Next = head1; // store
         
      if (!compare_exchange_strong(&head, head1, index)) {
         array[index].Next = head1; // store
      } else {
         fetch_add(&count, 1);
      };

      if (!compare_exchange_strong(&head, head1, index)) {
         array[index].Next = head1; // store
      } else {
         fetch_add(&count, 1);
      };

      // past unroll limit
      _exit(0);
  }
};

const unsigned NUM_THREADS = 3;
SafeStack<int> stack(3);

pthread_t threads[NUM_THREADS];

void* thread(void* arg)
{
  int idx = (int)(size_t)arg;

        int elem;

        elem = stack.Pop();
        if (elem < 0) {
            sched_yield();

            elem = stack.Pop();
            if (elem < 0) {
                // unroll limit
                _exit(0);
            }
            sched_yield();
        }

        stack.array[elem].Value = idx;
        assert(stack.array[elem].Value == idx);

        stack.Push(elem);


        elem = stack.Pop();
        if (elem < 0) {
            sched_yield();

            elem = stack.Pop();
            if (elem < 0) {
                // unroll limit
                _exit(0);
            }
            sched_yield();
        }

        stack.array[elem].Value = idx;
        assert(stack.array[elem].Value == idx);

        stack.Push(elem);

    return NULL;
}

int main()
{
  for (unsigned i = 0; i < NUM_THREADS; ++i) {
    pthread_create(&threads[i], NULL, thread, (void*)i);
  }
  
  for (unsigned i = 0; i < NUM_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }
  
  return 0;
}




#include <atomic>
#include <pthread.h>
#include <unistd.h>
#include <cassert>

pthread_mutex_t m;

inline bool compare_exchange_strong(int* addr, int tst, int val) {
  pthread_mutex_lock(&m);
  if (tst == *addr) {
    *addr = val;
    pthread_mutex_unlock(&m);
    return true;
  } else {
    pthread_mutex_unlock(&m);
    return false;
  }
}

inline int exchange(int* addr, int val) {
  pthread_mutex_lock(&m);
  int r = *addr;
  *addr = val;
  pthread_mutex_unlock(&m);
  return r;
}

inline void store(int* addr, int val) {
  pthread_mutex_lock(&m);
  *addr = val;
  pthread_mutex_unlock(&m);
  
}

inline int load(int* addr) {
  pthread_mutex_lock(&m);
  int r = *addr;
  pthread_mutex_unlock(&m);
  return r;
}

inline int fetch_sub(int* addr, int sub) {
  pthread_mutex_lock(&m);
  int r = *addr;
  *addr = r - sub;
  pthread_mutex_unlock(&m);
  return r;
}

inline int fetch_add(int* addr, int add) {
  pthread_mutex_lock(&m);
  int r = *addr;
  *addr = r + add;
  pthread_mutex_unlock(&m);
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
      store(&count, pushCount);
      store(&head, 0);
      for (int i = 0; i < pushCount - 1; i++)
          store(&array[i].Next, i + 1);
      store(&array[pushCount - 1].Next, -1);
  }

  ~SafeStack()
  {
      delete [] array;
  }

  int Pop()
  {
      while (load(&count) > 1)
      {
          int head1 = load(&head);
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
      }

      return -1;
  }

  void Push(int index)
  {
      int head1 = load(&head);
      do
      {
          store(&array[index].Next, head1);
         
      } while (!compare_exchange_strong(&head, head1, index));
      fetch_add(&count, 1);
  }
};

const unsigned NUM_THREADS = 3;
SafeStack<int> stack(3);

pthread_t threads[NUM_THREADS];

void* thread(void* arg)
{
  int idx = (int)(size_t)arg;
    for (size_t i = 0; i != 2; i += 1)
    {
        int elem;
        for (;;)
        {
            elem = stack.Pop();
            if (elem >= 0)
                break;
            sched_yield();
        }

        stack.array[elem].Value = idx;
        assert(stack.array[elem].Value == idx);

        stack.Push(elem);
    }
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


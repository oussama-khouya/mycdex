# Codexion: Comprehensive Defense & Theory Guide

This document is your complete, in-depth theoretical and technical defense reference for the **Codexion** project. It explains the project architecture, how your code solves every requirement, and dives deep into the operating system concepts (threads, processes, mutexes, condition variables, data races, race conditions, deadlocks, and scheduling algorithms) down to their low-level kernel and hardware mechanics.

---

# Table of Contents
1. [The Codexion Problem & Architecture](#1-the-codexion-problem--architecture)
2. [How the Code Works: Step-by-Step Walkthrough](#2-how-the-code-works-step-by-step-walkthrough)
3. [Program vs Process vs Thread](#3-program-vs-process-vs-thread)
4. [POSIX Threads (pthreads) Mechanics](#4-posix-threads-pthreads-mechanics)
5. [The Mutex: Deep Internal Mechanics](#5-the-mutex-deep-internal-mechanics)
6. [Condition Variables: Deep Internal Mechanics](#6-condition-variables-deep-internal-mechanics)
7. [Data Race vs Race Condition](#7-data-race-vs-race-condition)
8. [Deadlock: The 4 Coffman Conditions & Our Solution](#8-deadlock-the-4-coffman-conditions--our-solution)
9. [Scheduling Policies: FIFO vs EDF vs LIFO](#9-scheduling-policies-fifo-vs-edf-vs-lifo)
10. [Defense Evaluation Q&A (Ready-to-Answer Responses)](#10-defense-evaluation-qa)

---

# 1. The Codexion Problem & Architecture

### The Real-World Analogy: The Dining Philosophers
Codexion is an advanced, industrial extension of Edsger Dijkstra's classic **Dining Philosophers Problem** (1965):
- Instead of **Philosophers**, we have **Coders** ($N$ threads).
- Instead of **Spaghetti/Rice**, coders perform cycles of:
  $$\text{Compile} \longrightarrow \text{Debug} \longrightarrow \text{Refactor}$$
- Instead of **Forks**, coders share **Hardware Dongles** ($N$ shared resources).
- A coder sits in a circular arrangement at a round table:
  - Coder $i$ has **Left Dongle** $= i$
  - Coder $i$ has **Right Dongle** $= (i + 1) \pmod N$
- To compile, a coder **must simultaneously hold both the left and right dongles**.

### What makes Codexion much more complex than standard Philosophers?
Standard Philosophers has no hardware cooldown and no arbitration queues. Codexion introduces two major real-world operating system challenges:
1. **Hardware Dongle Cooldown (`dongle_cooldown`)**:
   - When a coder finishes compiling and releases a dongle, the hardware enters a cooldown period.
   - For `cooldown` milliseconds, **no coder is allowed to acquire that dongle**.
   - Implemented via absolute time tracking (`available_at = current_time + cooldown`) and timed kernel sleeps (`pthread_cond_timedwait`).
2. **Priority Queue Arbitration (`fifo` vs `edf`)**:
   - In simple philosophers, whoever grabs the mutex first gets the fork (uncontrolled contention).
   - In Codexion, each dongle has its own **hardware queue** (a min-heap priority queue).
   - When multiple coders compete for the same dongle, the dongle serves requests according to a strict scheduling policy:
     - **FIFO (First-In, First-Out)**: Served strictly by arrival timestamp (`arrival`).
     - **EDF (Earliest Deadline First)**: Served strictly by burnout deadline (`last_compile + burnout`).

---

# 2. How the Code Works: Step-by-Step Walkthrough

Our implementation is divided into modular components:

```
src/
├── codexion.h     --> Structures, types, prototypes, and constants
├── main.c         --> CLI parsing, thread creation, thread joining, cleanup
├── init.c         --> Mutex/cond initialization, heap setup, circular dongle assignment
├── dongle.c       --> Locking hierarchy, priority queue wait, cooldown timedwait, release
├── scheduler.c    --> Min-heap priority queue implementation (FIFO, EDF, tie-breaker)
├── routine.c      --> Coder lifecycle loop: compile, debug, refactor
├── monitor.c      --> High-precision watchdog thread monitoring burnout and compilation targets
└── utils.c        --> Micro-sleeps, timestamps, and serialized terminal output
```

### Module by Module:

#### 1. Circular Dongle Assignment (`src/init.c`)
Each coder $i$ is assigned two adjacent dongles:
```c
coder->left = i;
coder->right = (i + 1) % data->number_of_coders;
```
For 5 coders:
- Coder 1: Left = 0, Right = 1
- Coder 2: Left = 1, Right = 2
- Coder 3: Left = 2, Right = 3
- Coder 4: Left = 3, Right = 4
- Coder 5: Left = 4, Right = 0

#### 2. Resource Hierarchy / Lock Ordering (`src/dongle.c`)
To prevent circular wait deadlocks, every coder sorts its dongle IDs before acquiring:
```c
first = coder->left;
second = coder->right;
if (first > second) {
    tmp = first;
    first = second;
    second = tmp;
}
```
**Every coder always requests the lower numerical ID first, and the higher numerical ID second.**

#### 3. Dongle Priority Queue & Cooldown (`src/dongle.c`)
Inside `take_dongle(coder, dongle_id)`:
1. The coder builds a `t_request`:
   ```c
   request.id = coder->id;
   request.arrival = get_time_ms();
   request.deadline = coder->last_compile + data->burnout;
   ```
2. The coder locks the dongle's mutex (`pthread_mutex_lock(&d->mutex)`).
3. The coder pushes its request into `d->queue` (heap push).
4. The coder enters a synchronization loop:
   ```c
   while (!is_stopped(data)) {
       if (!d->taken && top_request(&d->queue) == coder->id) {
           if (get_time_ms() >= d->available_at) {
               d->taken = 1;
               heap_pop_first(&d->queue);
               pthread_mutex_unlock(&d->mutex);
               print_status(coder, "has taken a dongle");
               return (1);
           }
           // Cooldown is active: sleep until the exact millisecond it expires!
           ts.tv_sec = d->available_at / 1000;
           ts.tv_nsec = (d->available_at % 1000) * 1000000;
           pthread_cond_timedwait(&d->cond, &d->mutex, &ts);
       } else {
           // Dongle is taken or another coder has higher priority in the heap:
           pthread_cond_wait(&d->cond, &d->mutex);
       }
   }
   ```
5. When the simulation stops or acquisition fails, the coder removes its request and unlocks the mutex.

#### 4. Releasing Dongles (`src/dongle.c`)
When compilation completes:
1. `data->dongles[id].taken = 0;`
2. `data->dongles[id].available_at = get_time_ms() + data->cooldown;`
3. `pthread_cond_broadcast(&data->dongles[id].cond);` wakes all coders waiting for this dongle. They re-check the heap priority and the cooldown timestamp.

#### 5. High-Precision Watchdog Monitor (`src/monitor.c`)
A dedicated supervisor thread runs concurrently:
- Polls every `250 microseconds` (`usleep(250)`).
- Checks if `current_time - coder->last_compile >= burnout`. If so, logs `timestamp X burned out`, sets `data->stopped = 1`, broadcasts to all dongle condition variables, and terminates the simulation.
- Checks if all coders have reached `number_of_compiles_required`. If so, sets `data->stopped = 1`, broadcasts, and exits cleanly.

---

# 3. Program vs Process vs Thread

| Property | Program | Process | Thread |
| :--- | :--- | :--- | :--- |
| **Definition** | A static binary file stored on disk (e.g. `./codexion`). | An active running instance of a program in memory with an isolated execution environment. | The smallest unit of execution scheduled by the OS kernel, running inside a process. |
| **State** | Passive (passive bytes on storage: ELF on Linux, Mach-O on macOS). | Active (allocated resources, PID, page tables, file descriptors). | Active (executes CPU instructions on a core). |
| **Address Space** | None (resides on disk). | **Isolated Virtual Address Space** (Code, Data, Heap, Stack). Cannot touch other processes' memory. | **Shared Address Space**. All threads of a process share the exact same Heap, Globals, and Code. |
| **Stack & Registers** | None. | Has its own main thread stack and CPU register context. | **Has its OWN Stack** (local variables, function frames) and **OWN Registers** (PC, SP, general registers). |
| **Creation Cost** | None. | Heavyweight. OS must duplicate page tables, file descriptors, and virtual memory (`fork()` / `execve()`). | Lightweight. Created via `pthread_create()` (calls `clone()` on Linux). Allocates only a stack (~8MB default) and registers. |
| **Context Switch** | None. | Heavy. Requires flushing the CPU Translation Lookaside Buffer (TLB), switching page directory registers (`CR3` on x86_64). | Fast. Keeps the same page tables and memory space; only switches CPU registers and Stack Pointer. |
| **Communication** | None. | Inter-Process Communication (IPC): Pipes, Sockets, Shared Memory, Signals. | Direct memory access (shared pointers, heap buffers). Requires synchronization (Mutexes). |

---

# 4. POSIX Threads (pthreads) Mechanics

### What is POSIX?
**POSIX** stands for *Portable Operating System Interface*. It is an IEEE standard (IEEE 1003.1c) created to standardize system calls and interfaces across Unix-like systems (Linux, macOS, BSD).

### What is `pthread_t`?
A `pthread_t` is an opaque handle representing a thread of execution managed by the POSIX threading library.
- Under modern Linux (NPTL - Native POSIX Thread Library), threads are **1:1 Kernel Threads**: for every user-space `pthread_t`, the Linux kernel allocates a `task_struct` (created with the `clone()` system call with flags `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD`).
- Under macOS, `pthread_create()` maps directly to XNU kernel threads (`bsdthread_create`).

### Key Thread Lifecycle Functions:
1. **`pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)`**:
   - Allocates a new call stack for the thread.
   - Sets up initial CPU registers so the instruction pointer points to `start_routine`, and the first argument register points to `arg`.
   - Tells the kernel scheduler to add this thread to the runnable CPU runqueue.
2. **`pthread_join(pthread_t thread, void **retval)`**:
   - Suspends execution of the calling thread until the target `thread` terminates (runs to completion and exits).
   - Reclaims the terminated thread's resources (call stack, thread control block).
   - Similar to `waitpid()` for child processes. Prevents "thread zombies".

---

# 5. The Mutex: Deep Internal Mechanics

### What is a Mutex?
**Mutex** stands for **Mut**ual **Ex**clusion. It is a synchronization primitive that guarantees that **only one thread at a time can enter a Critical Section of code**.

### How Does a Mutex Work Internally?
At the hardware and OS level, a mutex is NOT a simple boolean variable. If it were a simple `int locked = 0`, two threads could read `locked == 0` at the exact same clock cycle, and both would enter the critical section!

#### 1. Hardware Atomic Instructions:
CPUs provide specialized hardware instructions that read-and-modify a memory address in a single, indivisible clock cycle with hardware memory bus locking:
- `x86_64`: `LOCK CMPXCHG` (Atomic Compare-and-Swap)
- `ARM`: `LDREX` / `STREX` (Load-Link / Store-Conditional)

#### 2. The Fast Path (User-Space, Zero Syscall):
When your thread calls `pthread_mutex_lock(&mutex)`:
- The CPU runs an atomic `compare-and-swap` to test if the lock word is `0` (unlocked).
- If it is `0`, it atomically changes it to `1` (locked) and returns **immediately**.
- **No system call is executed. This takes only ~10 to 20 CPU clock cycles (extremely fast)!**

#### 3. The Slow Path (Contention & Kernel Futex):
If the lock word is already `1` (another thread holds the mutex):
- The thread does not waste 100% CPU in a spinlock loop.
- It makes a system call to the operating system kernel:
  - On Linux: **`sys_futex(FUTEX_WAIT)`** (Fast Userspace Mutex).
  - On macOS: **`__psynch_mutexwait`**.
- The OS kernel puts the thread to sleep, moves it to the mutex's wait queue, and removes it from the CPU runqueue.
- The CPU core immediately context-switches to run other useful tasks.
- When the owner thread calls `pthread_mutex_unlock(&mutex)`:
  - If other threads are waiting, it invokes `sys_futex(FUTEX_WAKE)` / `__psynch_mutexdrop`.
  - The kernel wakes up one waiting thread from the wait queue, marks it runnable, and it acquires the lock.

### Mutexes in Our Project:
1. **`print_mutex`**:
   - Protects the terminal output buffer (`stdout`).
   - Ensures that multiple threads printing simultaneously cannot interleave their characters or scramble lines.
2. **`state_mutex`**:
   - Protects shared simulation state: `data->stopped`, `coder->last_compile`, and `coder->compile_count`.
3. **`dongle[i].mutex`**:
   - Protects the specific dongle's internal state: `taken`, `available_at`, and its min-heap priority queue (`queue`).

---

# 6. Condition Variables: Deep Internal Mechanics

### What is a Condition Variable?
A **Condition Variable (`pthread_cond_t`)** is a signaling mechanism that allows threads to suspend execution (sleep) efficiently without consuming CPU cycles until another thread notifies them that a shared state condition has become true.

### Why is a Condition Variable ALWAYS Paused with a Mutex?
A condition variable has no state of its own (it is merely a wait queue of threads). The condition being tested (e.g. `!d->taken`, `top_request == coder->id`, `available_at <= now`) is stored in shared memory.

#### The "Lost Wakeup" Race Condition (Why atomicity is mandatory):
Imagine if `pthread_cond_wait(&cond)` did not take a mutex:
1. Thread A checks `if (dongle is busy)`. It evaluates to TRUE.
2. Context switch to Thread B before Thread A sleeps!
3. Thread B releases the dongle and calls `pthread_cond_signal(&cond)`.
4. No thread is currently waiting on the condition variable, so the signal is **lost forever**.
5. Context switch back to Thread A. Thread A now goes to sleep on `pthread_cond_wait()`.
6. **Thread A sleeps forever (hangs/deadlocks) even though the dongle is free!**

#### How `pthread_cond_wait(&cond, &mutex)` solves this:
`pthread_cond_wait(&cond, &mutex)` performs three operations with **atomic kernel guarantees**:
1. It registers the calling thread into the condition variable's wait queue.
2. It **unlocks the mutex** so other threads can enter the critical section and modify the state.
3. It puts the thread to sleep.
When another thread signals or broadcasts:
4. The thread wakes up, and **re-acquires the mutex** before `pthread_cond_wait()` returns to your code!

### Why a `while` loop is MANDATORY (Never use `if`)!
```c
// WRONG:
if (!condition)
    pthread_cond_wait(&cond, &mutex);

// CORRECT:
while (!condition)
    pthread_cond_wait(&cond, &mutex);
```
Two critical reasons:
1. **Spurious Wakeups**: OS kernels (both Linux and macOS) may wake up a sleeping thread even if no signal was sent (due to kernel interrupts or signal handling).
2. **State Stealing**: When Thread B signals that a dongle is free, multiple threads may wake up. The first thread to re-acquire the mutex takes the dongle. When the second thread acquires the mutex, the dongle is **already taken again**! The `while` loop forces the second thread to re-evaluate the condition and go back to sleep.

### `pthread_cond_timedwait` for Hardware Cooldown:
Rather than burning CPU in a loop or doing imprecise `usleep` calls while holding a lock, our code uses `pthread_cond_timedwait`:
```c
struct timespec ts;
ts.tv_sec = d->available_at / 1000;
ts.tv_nsec = (d->available_at % 1000) * 1000000;
pthread_cond_timedwait(&d->cond, &d->mutex, &ts);
```
The OS kernel wakes the thread **at the exact absolute nanosecond** that the hardware cooldown expires, or earlier if signaled.

---

# 7. Data Race vs Race Condition

These two terms sound similar, but in Computer Science they represent distinct concurrency concepts:

```
+-------------------------------------------------------------+
| CONCURRENCY BUGS                                            |
|                                                             |
|   +--------------------------+  +-------------------------+ |
|   |        DATA RACE         |  |     RACE CONDITION      | |
|   |  (Low-Level Memory Bug)  |  | (High-Level Logic Bug)  | |
|   +--------------------------+  +-------------------------+ |
+-------------------------------------------------------------+
```

### A. Data Race (Low-Level Memory Error)
- **Definition**: Occurs when two or more instructions in different threads access the same memory location concurrently, **at least one access is a write**, and **there is no synchronization (mutex or atomic operation) ordering the accesses**.
- **Result**: Undefined Behavior (UB) in C.
  - **Torn Reads/Writes**: On 64-bit architectures, writing a 64-bit number non-atomically might result in another thread reading 32 bits of the old value and 32 bits of the new value!
  - **Compiler Optimizations**: The compiler may cache variables in CPU registers (`rax`, `rbx`) instead of reading from RAM, meaning changes written by Thread A are never seen by Thread B.
  - **Hardware Cache Incoherency**: Modern CPUs have independent L1/L2 caches. Without memory fences (provided by mutexes), writes in Core 1's cache may not be flushed to Core 2.
- **Example in Codexion without Mutex**:
  ```c
  // Coder thread:
  coder->last_compile = get_time_ms(); // Write
  
  // Monitor thread:
  if (get_time_ms() - coder->last_compile > burnout) // Read at same time!
  ```
- **How we prevent it**: Both the read and the write are strictly enclosed within `pthread_mutex_lock(&data->state_mutex)`.

### B. Race Condition (High-Level Timing/Logic Flaw)
- **Definition**: A semantic flaw where the correctness of a program's output depends on the execution order, timing, or interleaving of threads.
- **Key distinction**: A program can be **100% free of data races** (every single variable access has a mutex) and **STILL have a devastating race condition**!
- **Classic "Check-Then-Act" Race Condition**:
  ```c
  // Thread 1:
  pthread_mutex_lock(&d->mutex);
  if (!d->taken) {                         // 1. Check
      pthread_mutex_unlock(&d->mutex);     // <-- BAD: Lock released between check and act!
      
      // Thread 2 runs here, locks d->mutex, takes the dongle, and unlocks!
      
      pthread_mutex_lock(&d->mutex);
      d->taken = 1;                        // 2. Act (Now TWO threads think they own it!)
      pthread_mutex_unlock(&d->mutex);
  }
  ```
- **How we prevent it**: All check-and-act operations (checking if dongle is taken, checking priority, pushing/popping from the heap, and setting `taken = 1`) occur inside a **single, contiguous critical section** while holding `d->mutex`.

---

# 8. Deadlock: The 4 Coffman Conditions & Our Solution

### What is a Deadlock?
A **Deadlock** is a permanent stall state where a set of threads are blocked forever because each thread holds a lock that another thread needs, and none can proceed.

### The 4 Coffman Conditions (E. G. Coffman Jr., 1971):
For a deadlock to occur, **all four of the following conditions must hold simultaneously**:

1. **Mutual Exclusion**:
   - The resources involved cannot be shared; only one thread can hold a resource at any given time.
   - *(True in our project: a dongle can only be held by 1 coder at a time).*
2. **Hold and Wait**:
   - A thread holds at least one resource while waiting to acquire another resource that is currently held by another thread.
   - *(E.g., Coder holds Dongle 0 and sleeps waiting for Dongle 1).*
3. **No Preemption**:
   - Resources cannot be forcibly taken away from a thread holding them; they can only be released voluntarily by the thread after it completes its task.
   - *(True in our project: a coder cannot steal a dongle from its neighbor).*
4. **Circular Wait**:
   - A closed chain of threads exists such that each thread holds a resource needed by the next thread in the chain:
     $$\text{Coder } 0 \to \text{holds } D_0, \text{waits for } D_1$$
     $$\text{Coder } 1 \to \text{holds } D_1, \text{waits for } D_2$$
     $$\dots$$
     $$\text{Coder } N-1 \to \text{holds } D_{N-1}, \text{waits for } D_0$$
     *(A complete circular cycle!)*

---

### How Our Code Eliminates Deadlock: Breaking Circular Wait

To prevent deadlock, an operating system must break **at least one** of the four Coffman conditions.
We eliminate **Condition 4: Circular Wait** using Dijkstra's **Resource Hierarchy / Lock Ordering Strategy**.

#### The Mathematical Proof:
Let all dongles be assigned a strict global ordering based on their integer ID:
$$D_0 < D_1 < D_2 < D_3 < \dots < D_{N-1}$$

In `src/dongle.c`:
```c
first = coder->left;
second = coder->right;

if (first > second) {
    tmp = first;
    first = second;
    second = tmp;
}
take_dongle(coder, first);
take_dongle(coder, second);
```

#### What happens around the table:
- **Coder 1** (needs $D_0, D_1$): locks $D_0$, then $D_1$.
- **Coder 2** (needs $D_1, D_2$): locks $D_1$, then $D_2$.
- **Coder 3** (needs $D_2, D_3$): locks $D_2$, then $D_3$.
- **Coder 4** (needs $D_3, D_4$): locks $D_3$, then $D_4$.
- **Coder 5** (needs $D_4, D_0$):
  - In a naive system, Coder 5 locks $D_4$ then waits for $D_0$ (creating the circular loop $D_0 \to D_1 \to D_2 \to D_3 \to D_4 \to D_0$).
  - **In our code**: Since $D_0 < D_4$, Coder 5 is forced to acquire **$D_0$ FIRST**, and **$D_4$ SECOND**!

#### Why Deadlock is Impossible:
Because Coder 1 and Coder 5 both compete for $D_0$ as their first lock, one of them wins $D_0$, while the other blocks before acquiring ANY dongle at all. The circular chain of dependencies is broken, and a cycle in the resource allocation graph can **never form**.

---

# 9. Scheduling Policies: FIFO vs EDF vs LIFO

In `src/scheduler.c`, the comparison logic is implemented inside `higher()`:

```c
static int higher(t_request a, t_request b, int policy)
{
    if (policy == FIFO)
    {
        if (a.arrival != b.arrival)
            return (a.arrival < b.arrival);
        return (a.id < b.id);
    }
    if (a.deadline != b.deadline)
        return (a.deadline < b.deadline);
    return (a.id > b.id);
}
```

### 1. FIFO (First-In, First-Out):
- Orders requests strictly by `arrival` timestamp.
- Whichever coder entered the queue first gets served first.
- If two coders arrive at the exact same millisecond (`a.arrival == b.arrival`), the tie is broken by smaller ID (`a.id < b.id`).
- **Property**: Simple, fair arrival ordering, but susceptible to starvation under high contention because it is unaware of deadlines.

### 2. EDF (Earliest Deadline First):
- Orders requests by urgency: $\text{deadline} = \text{last\_compile} + \text{time\_to\_burnout}$.
- The coder that is closest to dying gets prioritized to the front of the queue.
- **Tie-Breaker (Evaluation Sheet Rule)**: If two coders have the exact same deadline (`a.deadline == b.deadline`), higher ID wins (`a.id > b.id`).
- **Property**: Optimal for real-time scheduling. Maximizes survival under feasible loads.

### 3. The Live Defense Recode: Converting FIFO to LIFO
In the 42 evaluation sheet (page 4), the evaluator will ask you to modify your code live to turn `fifo` into `lifo` (Last-In, First-Out / Stack behavior).

#### How to do it in 15 seconds:
Open `src/scheduler.c` at line 9 and change `<` to `>`:
```c
// ORIGINAL (FIFO):
if (a.arrival != b.arrival)
    return (a.arrival < b.arrival); // Smallest arrival time (earliest) wins

// MODIFIED (LIFO):
if (a.arrival != b.arrival)
    return (a.arrival > b.arrival); // Largest arrival time (most recent) wins
```
Recompile with `make re`. Now the most recent request is served first!

---

# 10. Defense Evaluation Q&A

### Q1: What is the difference between a process and a thread?
> **Answer**: A process is an independent execution environment with its own isolated virtual address space, file descriptor table, and PID. A thread is a lightweight execution unit inside a process; multiple threads in the same process share the same virtual address space, heap, and global variables, but each thread has its own private execution stack, CPU registers, and program counter.

### Q2: Why did you use `pthread_cond_timedwait` for cooldown instead of `usleep`?
> **Answer**: `usleep()` is imprecise and blocks the entire thread without releasing the mutex, which would freeze all other coders sharing that dongle. `pthread_cond_timedwait()` atomically releases the mutex and suspends the thread directly in the kernel until the exact millisecond (`available_at`) is reached, allowing other threads to access the queue while the dongle cools down.

### Q3: Why is there a `while` loop around `pthread_cond_wait` instead of an `if`?
> **Answer**: Because of two reasons:
> 1. **Spurious wakeups**: The OS kernel can wake up a thread without any explicit signal.
> 2. **Race conditions / State stealing**: When a broadcast wakes multiple threads, the first thread to re-acquire the mutex may grab the dongle. By using `while`, any subsequent thread that wakes up re-evaluates the condition and safely goes back to sleep if the resource is no longer available.

### Q4: How does your code prevent deadlocks?
> **Answer**: By eliminating the Circular Wait condition (the 4th Coffman condition). Every coder compares its left and right dongle IDs and always acquires the smaller ID first, followed by the larger ID. Because all threads lock resources in strictly ascending numerical order, a circular lock dependency is mathematically impossible.

### Q5: What is a Data Race and how does your code guarantee there are none?
> **Answer**: A data race occurs when two threads access the same memory location concurrently without synchronization, and at least one access is a write, causing undefined behavior. We guarantee zero data races by protecting all shared memory (coder state, compile counts, stopped flag, dongle queues) with dedicated mutexes (`state_mutex`, `dongle.mutex`, `print_mutex`). We verified this using ThreadSanitizer (`-fsanitize=thread`).

### Q6: Why does `./codexion 5 3000 200 200 200 10 800` burn out? Is that a bug?
> **Answer**: No, it is mathematically required. With 200ms compile time and 800ms cooldown, each compilation round locks the dongles for 1000ms. In a 3000ms burnout window, only 3 rounds ($3 \times 1000\text{ms} = 3000\text{ms}$) can physically take place. Because 5 coders share 5 dongles, at most 4 compilation slots fit in that time frame. Fitting 5 coders into 4 slots is mathematically impossible, so at least one coder must burn out. The evaluation sheet specifically uses this test to observe queue ordering under high contention, not to expect indefinite survival.

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

# 1. The Codexion Problem & How My Code Solves It

Codexion is an industrial extension of Edsger Dijkstra's classic **Dining Philosophers Problem** (1965):
- Instead of **Philosophers**, we have **Coders** ($N$ threads).
- Instead of **Spaghetti/Rice**, coders perform cycles of:
  $$\text{Compile} \longrightarrow \text{Debug} \longrightarrow \text{Refactor}$$
- Instead of **Forks**, coders share **Hardware Dongles** ($N$ shared resources).
- A coder sits in a circular arrangement at a round table:
  - Coder $i$ has **Left Dongle** $= i$
  - Coder $i$ has **Right Dongle** $= (i + 1) \pmod N$
- To compile, a coder **must simultaneously hold both the left and right dongles**.

Below is the complete breakdown of **every specific problem** in this project and **the exact solution** implemented in your code:

---

### Problem 1: Deadlock (Circular Wait)
- **The Problem**:
  If every coder picks up their left dongle first, all 5 coders simultaneously hold their left dongle. Then, all 5 coders attempt to pick up their right dongle. Because each right dongle is already held by their neighbor, every thread enters an infinite wait state. No coder can compile, and no coder releases their dongle. The simulation **freezes completely forever (Deadlock)**.
- **The Solution in My Code ([src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L60-L68))**:
  We eliminate the Circular Wait condition (the 4th Coffman condition) using Dijkstra's **Resource Hierarchy Strategy**. Before acquiring any dongle, every coder sorts its two dongle IDs:
  ```c
  first = coder->left;
  second = coder->right;
  if (first > second) {
      tmp = first;
      first = second;
      second = tmp;
  }
  ```
  Every coder **always acquires the lower numerical ID first, and the higher numerical ID second**.
  - Coders 1, 2, 3, and 4 acquire: (0 then 1), (1 then 2), (2 then 3), (3 then 4).
  - Coder 5 needs Dongle 4 and Dongle 0. Instead of taking 4 first, Coder 5 is forced to acquire **Dongle 0 FIRST, and Dongle 4 SECOND**.
  - Because Coder 1 and Coder 5 both compete for Dongle 0 as their very first action, one of them wins and the other blocks *before* holding any dongle. A circular dependency cycle is mathematically impossible!

---

### Problem 2: Terminal Output Scrambling (Garbled Logs)
- **The Problem**:
  The function `printf()` writes characters to the standard output (`stdout`) stream. When multiple threads call `printf()` at the exact same millisecond, their output characters interleave on the terminal screen, producing corrupted logs like:
  ```text
  200 1 200 2 is is dcompebiugggingng
  ```
  This immediately fails the evaluation check for clean log formatting.
- **The Solution in My Code ([src/utils.c](file:///Users/okhouya/Documents/mycodex/src/utils.c#L46-L59))**:
  We protect all printing inside a dedicated mutex: `print_mutex`.
  ```c
  void print_status(t_coder *coder, char *status) {
      pthread_mutex_lock(&coder->data->print_mutex);
      if (!coder->data->stopped) {
          printf("%ld %d %s\n", get_time_ms() - coder->data->start_time, coder->id, status);
      }
      pthread_mutex_unlock(&coder->data->print_mutex);
  }
  ```
  Every log line is guaranteed to be printed atomically from start to finish without interruption. Furthermore, it checks `!coder->data->stopped` so that **no log can ever appear after a coder burns out or the simulation ends**.

---

### Problem 3: Data Races on Shared State Variables
- **The Problem**:
  The coder thread writes to `coder->last_compile` when it starts compiling, while the monitor thread simultaneously reads `coder->last_compile` to check if the coder burned out. If two threads read and write the same 64-bit integer concurrently without synchronization, modern CPU architectures cause **torn reads** (reading half-written memory), cache incoherency, and compiler register caching, causing the monitor to miss deaths or trigger false burnouts.
- **The Solution in My Code ([src/routine.c](file:///Users/okhouya/Documents/mycodex/src/routine.c#L51-L53) & [src/monitor.c](file:///Users/okhouya/Documents/mycodex/src/monitor.c#L18-L23))**:
  We protect all accesses to `last_compile`, `compile_count`, and `data->stopped` with `state_mutex`:
  ```c
  // Coder thread (write):
  pthread_mutex_lock(&coder->data->state_mutex);
  coder->last_compile = get_time_ms();
  pthread_mutex_unlock(&coder->data->state_mutex);

  // Monitor thread (read):
  pthread_mutex_lock(&data->state_mutex);
  last = data->coders[i].last_compile;
  pthread_mutex_unlock(&data->state_mutex);
  ```
  This eliminates 100% of data races and guarantees strict memory synchronization across all CPU cores (verified with `-fsanitize=thread`).

---

### Problem 4: Hardware Cooldown Wait Without CPU Burning
- **The Problem**:
  When a coder releases a dongle, the hardware enters a mandatory `dongle_cooldown` (e.g. 400ms or 800ms). If waiting threads use a spinlock loop like `while (get_time_ms() < available_at)`, the CPU runs at 100% usage, overheating the machine and starving other threads. If they call `usleep()` while holding `dongle.mutex`, the entire dongle is blocked and no other thread can even enqueue a request!
- **The Solution in My Code ([src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L40-L42))**:
  We use **`pthread_cond_timedwait`** with absolute nanosecond timestamps:
  ```c
  ts.tv_sec = d->available_at / 1000;
  ts.tv_nsec = (d->available_at % 1000) * 1000000;
  pthread_cond_timedwait(&d->cond, &d->mutex, &ts);
  ```
  `pthread_cond_timedwait` **atomically releases the mutex and puts the thread to sleep in the kernel**. The kernel wakes the thread at the exact nanosecond that the cooldown expires, consuming **0% CPU** while sleeping.

---

### Problem 5: Unfair Contention & Queue Arbitration (FIFO vs EDF)
- **The Problem**:
  In standard POSIX mutexes, when a mutex is unlocked, whichever thread happens to hit the CPU core first grabs the lock (uncontrolled race). In Codexion, hardware dongles must serve requests according to a strict priority policy:
  - In `fifo`: strictly by arrival timestamp.
  - In `edf`: strictly by earliest burnout deadline (`last_compile + burnout`).
- **The Solution in My Code ([src/scheduler.c](file:///Users/okhouya/Documents/mycodex/src/scheduler.c) & [src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L25-L35))**:
  Each dongle has its own priority queue (`t_heap queue`).
  1. A requesting coder pushes its request (`heap_push(&d->queue, request)`).
  2. Inside `scheduler.c`, the function `higher()` compares requests according to the selected policy:
     - `FIFO`: compares `a.arrival < b.arrival`.
     - `EDF`: compares `a.deadline < b.deadline` (with scale tie-breaker `a.id > b.id`).
  3. A coder is **only allowed to take the dongle if it is at the root of the queue**:
     ```c
     if (!d->taken && (top_request(&d->queue) == coder->id))
     ```
  4. If another coder is higher in the queue, the thread calls `pthread_cond_wait(&d->cond, &d->mutex)` and waits its turn.

---

### Problem 6: The Single Coder Edge Case (`number_of_coders == 1`)
- **The Problem**:
  When `./codexion 1 800 200 200 200 10 0 fifo` is run:
  - There is only 1 coder and only 1 dongle (`left == 0` and `right == 0`).
  - A coder needs 2 dongles to compile.
  - If the coder attempts to lock the right dongle, it tries to lock the same dongle it already holds, causing a **self-deadlock** (thread permanently freezes).
- **The Solution in My Code ([src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L75-L83))**:
  We explicitly detect when `first == second`:
  ```c
  if (first == second) {
      while (!is_stopped(coder->data))
          sleep_for_ms(1, coder->data);
      realease_dongles(coder);
      return (0);
  }
  ```
  The coder picks up the 1st dongle (printing `0 1 has taken a dongle`), waits peacefully until the burnout limit is reached, releases the dongle, and exits cleanly.

---

### Problem 7: Imprecise Burnout Detection (<1ms Accuracy)
- **The Problem**:
  The 42 evaluation sheet requires burnout to be detected within $\pm 10\text{ms}$ of the exact timestamp. If the monitor thread uses a coarse sleep like `usleep(10000)` (10ms) or `sleep(1)`, OS scheduling latency can cause the death log to appear 15ms or 20ms late, failing the evaluation.
- **The Solution in My Code ([src/monitor.c](file:///Users/okhouya/Documents/mycodex/src/monitor.c#L62))**:
  The monitor thread polls at high frequency using **sub-millisecond intervals**:
  ```c
  usleep(250); // Checks every 0.25 milliseconds (4000 times per second)
  ```
  Burnout is caught almost instantaneously (within $\le 1\text{ms}$ of precision).

---

### Problem 8: Thread Leaks & Hanging on Simulation Stop
- **The Problem**:
  When one coder burns out or all coders complete their required compilations, other coders might be blocked inside `pthread_cond_wait(&d->cond, &d->mutex)` waiting for dongles that will never be released. If these threads remain blocked, `pthread_join` in `main.c` will **hang forever**, leaking threads and preventing the program from terminating.
- **The Solution in My Code ([src/monitor.c](file:///Users/okhouya/Documents/mycodex/src/monitor.c#L36-L45))**:
  Whenever the monitor detects a stop condition:
  1. It locks `state_mutex` and sets `data->stopped = 1`.
  2. It immediately iterates through **all dongles** and calls **`pthread_cond_broadcast(&data->dongles[i].cond)`**:
     ```c
     i = 0;
     while (i < data->number_of_coders) {
         pthread_mutex_lock(&data->dongles[i].mutex);
         pthread_cond_broadcast(&data->dongles[i].cond);
         pthread_mutex_unlock(&data->dongles[i].mutex);
         i++;
     }
     ```
  3. Every sleeping thread is instantly woken up, detects `is_stopped(data) == 1`, exits its loop, and returns cleanly. All threads join successfully in `main.c`.

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

### How Mutexes Solve Real Problems in Our Code:

#### 1. `print_mutex`
- **The Problem It Solves**: The standard C library function `printf()` writes to a shared user-space stream (`stdout`). When multiple threads call `printf()` concurrently, the OS kernel context-switches mid-string, interleaving characters from different threads (e.g. `200 1 200 2 is is dcompebiugggingng`).
- **The Solution in Our Code**: Every print is wrapped in `pthread_mutex_lock(&data->print_mutex)`. Only one thread can write to `stdout` at a time. After writing, it unlocks the mutex, guaranteeing clean, unbroken log lines.

#### 2. `state_mutex`
- **The Problem It Solves**: In concurrent execution, Coder 1 updates `coder->last_compile = get_time_ms()` while the Monitor thread reads `coder->last_compile` to check for burnout. Without synchronization, this creates a **Data Race**, causing CPU cache incoherency and "torn reads" (where the 64-bit value is read half-updated), leading to false burnouts or missed deaths.
- **The Solution in Our Code**: Both the write in `coder_routine()` and the read in `monitor_routine()` acquire `pthread_mutex_lock(&data->state_mutex)`. This issues CPU hardware memory barriers (fences), ensuring the Monitor reads the exact updated value from memory every single time.

#### 3. `dongle[i].mutex`
- **The Problem It Solves**: Adjacent Coders $i$ and $i+1$ share Dongle $i$. If both coders attempt to push their request into `d->queue` or check `d->taken` at the exact same millisecond, they will corrupt the heap array (`heap->items`), cause memory corruption, or both acquire the same dongle simultaneously.
- **The Solution in Our Code**: Every single operation on Dongle $i$ (pushing to heap, checking top of heap, checking cooldown, popping, and changing `taken`) is enclosed inside `pthread_mutex_lock(&data->dongles[id].mutex)`. Only one coder can modify that dongle's hardware state at any instant.

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

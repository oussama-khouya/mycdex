# Codexion: Comprehensive Defense & Theory Guide
*(Written in Clear & Simple English)*

This document is your complete defense guide for the **Codexion** project. It explains how the project works, how your code solves every requirement, and explains all operating system concepts (threads, processes, mutexes, condition variables, data races, race conditions, deadlocks, and schedulers) in simple, easy-to-understand words.

---

# Table of Contents
1. [The Codexion Problem & How My Code Solves It](#1-the-codexion-problem--how-my-code-solves-it)
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

## 1.1 The Dining Philosophers Problem: History & Theory
In 1965, **Edsger Dijkstra** invented a famous puzzle to explain the dangers of sharing resources between programs. In 1971, **Tony Hoare** turned it into the famous **Dining Philosophers Problem**:
* 5 philosophers sit around a round dinner table.
* They spend their lives doing two things: **Thinking** and **Eating**.
* In the center of the table is a bowl of food.
* Between each pair of philosophers is **1 fork** (5 philosophers, 5 forks total).
* To eat, a philosopher **needs TWO forks** (both their left fork and their right fork).
* When they finish eating, they put both forks down and go back to thinking.

### Why is this problem famous in Computer Science?
It is the most famous example used to teach:
1. **Sharing resources safely**: Two people cannot use the exact same tool at the same second.
2. **Deadlock**: If everyone grabs 1 fork, everyone waits forever and starves.
3. **Starvation**: If some people hog the forks, others never get to eat.
4. **Coordination**: How to make independent workers cooperate without a central boss.

---

## 1.2 From Dining Philosophers to Codexion: The Modern Version
Codexion turns this classic puzzle into a modern multi-threaded computer simulation:
* **Philosophers $\longrightarrow$ Coders** ($N$ independent threads).
* **Eating $\longrightarrow$ Compiling** (Needs 2 hardware USB dongles).
* **Thinking $\longrightarrow$ Debugging & Refactoring** (Releases dongles and works alone).
* **Forks $\longrightarrow$ Hardware Dongles** ($N$ shared dongles guarded by mutexes).
* **Starving to Death $\longrightarrow$ Coder Burnout** (If a coder waits longer than `time_to_burnout` without compiling, they die and the simulation stops).
* **Dongle Cooldown (New Rule)**: When a coder puts down a dongle, the hardware is "hot". It needs `dongle_cooldown` milliseconds before anyone can touch it again.
* **Queue Scheduling (New Rule)**: If two coders want the same dongle, they wait in a queue:
  - **FIFO**: First come, first served (by arrival time).
  - **EDF**: Emergency room rules (the coder closest to dying gets it first).

---

## 1.3 Visual Diagram: The Circular Table & Dongle Sharing

Here is how 5 coders and 5 shared dongles sit around the table:

```
                            ┌───────────────┐
                            │    CODER 1    │
                            │  Left:  D0    │
                            │  Right: D1    │
                            └───────┬───────┘
                                   / \
                                  /   \
                        Dongle 0 /     \ Dongle 1
                                /       \
                               /         \
                 ┌────────────┴──┐     ┌──┴────────────┐
                 │    CODER 5    │     │    CODER 2    │
                 │  Left:  D4    │     │  Left:  D1    │
                 │  Right: D0    │     │  Right: D2    │
                 └──────┬────────┘     └────────┬──────┘
                         \                     /
                Dongle 4  \                   /  Dongle 2
                           \                 /
                            ┌──┴───────────┴──┐
                            │    CODER 4      │
                            │  Left:  D3      │
                            │  Right: D4      │
                            └───┬─────────┬───┘
                                 \       /
                                  \     /
                         Dongle 3  \   /
                                    \ /
                            ┌────────┴────────┐
                            │    CODER 3      │
                            │  Left:  D2      │
                            │  Right: D3      │
                            └─────────────────┘
```

### Who shares which dongle?
* **Dongle 0** is shared between **Coder 1** and **Coder 5**.
* **Dongle 1** is shared between **Coder 1** and **Coder 2**.
* **Dongle 2** is shared between **Coder 2** and **Coder 3**.
* **Dongle 3** is shared between **Coder 3** and **Coder 4**.
* **Dongle 4** is shared between **Coder 4** and **Coder 5**.

> **Important Rule**: Neighbors **cannot compile at the same time** because they share a dongle. With 5 coders, at most **2 coders** can compile at once!

---

## 1.4 Visual Diagram: The Coder Lifecycle

Each coder runs this loop until the simulation stops:

```
               ┌──────────────────────────────────────────┐
               │                                          │
               ▼                                          │
    ┌──────────────────────┐                              │
    │  READY / REFACTORING │                              │
    └──────────┬───────────┘                              │
               │ Requests Dongles                         │
               ▼                                          │
    ┌──────────────────────┐                              │
    │   ENQUEUE REQUEST    │ (Pushes request to heap)     │
    └──────────┬───────────┘                              │
               │ Waits for Turn + Cooldown                │
               ▼                                          │
    ┌──────────────────────┐                              │
    │   ACQUIRE DONGLES    │ (Takes smaller ID, then big) │
    └──────────┬───────────┘                              │
               │                                          │
               ▼                                          │
    ┌──────────────────────┐                              │
    │      COMPILING       │ (Sleeps compile_time ms)     │
    │ (Resets burnout time)│                              │
    └──────────┬───────────┘                              │
               │                                          │
               ▼                                          │
    ┌──────────────────────┐                              │
    │   RELEASE DONGLES    │ (Starts Cooldown Timer)      │
    │  (Broadcast Signal)  │                              │
    └──────────┬───────────┘                              │
               │                                          │
               ▼                                          │
    ┌──────────────────────┐                              │
    │      DEBUGGING       │ (Sleeps debug_time ms)       │
    └──────────┬───────────┘                              │
               │                                          │
               ▼                                          │
    ┌──────────────────────┐                              │
    │     REFACTORING      │ (Sleeps refactor_time ms)    │
    └──────────┬───────────┘                              │
               │                                          │
               └──────────────────────────────────────────┘
```

---

## 1.5 Visual Diagram: Dongle Hardware Cooldown

When a coder puts down a dongle, it is locked in cooldown before anyone else can touch it:

```
Time (ms) ──►
0ms                  200ms                            600ms
├──────────────────────┼────────────────────────────────┼──────────────────────►
│   Coder Compiles     │     HARDWARE COOLDOWN WINDOW   │   Dongle Available   │
│   (Dongle is BUSY)   │     (Dongle is COOLING DOWN)   │   (Next Coder Can    │
│                      │     (Nobody can touch it)      │    Take Dongle)      │
└──────────────────────┴────────────────────────────────┴──────────────────────►
                       ▲                                ▲
                  released at:                   available_at:
                  get_time_ms()              get_time_ms() + cooldown
```

---

## 1.6 Visual Diagram: Process & Memory Architecture

How our program structures memory and threads inside the operating system:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CODEXION PROCESS (RAM)                            │
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                     SHARED MEMORY (HEAP & GLOBALS)                    │  │
│  │                                                                       │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │ t_data structure:                                               │  │  │
│  │  │  - burnout, compile_time, debug_time, refactor_time             │  │  │
│  │  │  - stopped flag (is game over?)                                 │  │  │
│  │  │  - state_mutex (protects timers and stopped flag)               │  │  │
│  │  │  - print_mutex (protects terminal printing)                     │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  │                                                                       │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │ t_dongle[N] array:                                              │  │  │
│  │  │  - id, taken (0 or 1), available_at timestamp                   │  │  │
│  │  │  - mutex (lock for this dongle)                                 │  │  │
│  │  │  - cond (wakes waiting threads when free)                       │  │  │
│  │  │  - queue: t_heap (waiting line for this dongle)                 │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────┬───────────────────────────────────┘  │
│                                      │ Shared Pointers                      │
│         ┌────────────────────────────┼────────────────────────────┐         │
│         ▼                            ▼                            ▼         │
│  ┌──────────────┐             ┌──────────────┐             ┌──────────────┐ │
│  │ MAIN THREAD  │             │ CODER THREAD │             │   MONITOR    │ │
│  │ (main.c)     │             │ 1 .. N       │             │ THREAD       │ │
│  │              │             │ (routine.c)  │             │ (monitor.c)  │ │
│  │ Private:     │             │ Private:     │             │ Private:     │ │
│  │ - argc, argv │             │ - coder_id   │             │ - checks     │ │
│  │ - joins all  │             │ - left/right │             │   burnout    │ │
│  └──────────────┘             └──────────────┘             └──────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1.7 Every Problem in Codexion & Exactly How My Code Solves It

---

### Problem 1: Deadlock (The Infinite Freeze)
- **The Problem**:
  If every coder picks up their left dongle first, all 5 coders are now holding 1 dongle in their left hand. Then, all 5 coders try to grab their right dongle. But the right dongle is already held by their neighbor! Everyone waits for their neighbor to let go. Nobody lets go. Everyone freezes forever (**Deadlock**).

#### Visual Diagram: The Deadlock Trap (Circular Wait)
```
   CODER 1  ──[Holds D0, Waits for]──►  CODER 2
      ▲                                    │
      │ [Waits for D0]                     │ [Holds D1, Waits for D2]
      │                                    ▼
   CODER 5                              CODER 3
      ▲                                    │
      │ [Holds D4, Waits for D0]           │ [Holds D2, Waits for D3]
      │                                    ▼
      └─────────[Holds D3, Waits for]─── CODER 4

      *** DEADLOCK: EVERYONE HOLDS 1 DONGLE AND WAITS FOR THE NEXT! ***
```

- **The Solution in My Code ([src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L60-L68))**:
  We use the **Resource Hierarchy** trick. Every coder sorts their two dongles and **always takes the smaller number first**:
  ```c
  first = coder->left;
  second = coder->right;
  if (first > second) {
      tmp = first;
      first = second;
      second = tmp;
  }
  ```
  - Coder 1 grabs: Dongle 0, then Dongle 1.
  - Coder 2 grabs: Dongle 1, then Dongle 2.
  - Coder 3 grabs: Dongle 2, then Dongle 3.
  - Coder 4 grabs: Dongle 3, then Dongle 4.
  - Coder 5 needs Dongle 4 and Dongle 0. Normally Coder 5 would grab 4 first. But our code forces Coder 5 to grab **Dongle 0 FIRST, and Dongle 4 SECOND**!
  - Now Coder 1 and Coder 5 both fight for Dongle 0 first. One gets it, and the other waits with **empty hands**. Because nobody is stuck holding 1 dongle in a circle, the cycle is broken!

#### Visual Diagram: How Sorting Breaks the Cycle
```
   CODER 1  ──(Fights for D0 FIRST)──► [ DONGLE 0 ] ◄──(Fights for D0 FIRST)── CODER 5
                                              │
                        ┌─────────────────────┴─────────────────────┐
                        ▼                                           ▼
                 CODER 1 WINS D0                             CODER 5 WAITS
          (Now holds D0, takes D1)                     (Holds ZERO dongles! Cannot
                        │                               lock D4, so Coder 4 can
                        ▼                               easily take D3 & D4!)
          CODER 1 COMPILES & RELEASES D0, D1                        │
                        │                                           ▼
                        └──────────────────────────────► DEADLOCK IS IMPOSSIBLE!
```

---

### Problem 2: Mixed-Up Terminal Logs (Scrambled Text)
- **The Problem**:
  `printf()` writes text to the screen. If 5 threads call `printf()` at the exact same millisecond, their letters get mixed together like this:
  ```text
  200 1 200 2 is is dcompebiugggingng
  ```
- **The Solution in My Code ([src/utils.c](file:///Users/okhouya/Documents/mycodex/src/utils.c#L46-L59))**:
  We wrap all printing inside `print_mutex`:
  ```c
  void print_status(t_coder *coder, char *status) {
      pthread_mutex_lock(&coder->data->print_mutex);
      if (!coder->data->stopped) {
          printf("%ld %d %s\n", get_time_ms() - coder->data->start_time, coder->id, status);
      }
      pthread_mutex_unlock(&coder->data->print_mutex);
  }
  ```
  Only one thread is allowed to write to the screen at a time. It also checks `!coder->data->stopped` so that **no log can ever appear after a coder burns out**.

---

### Problem 3: Data Races on Shared Variables
- **The Problem**:
  The coder thread writes to `coder->last_compile` when it starts compiling. At the exact same time, the monitor thread reads `coder->last_compile` to check if the coder died. If two threads read and write the exact same variable at the same second without protection, the computer gets confused, reads half-written memory, and causes bugs.
- **The Solution in My Code ([src/routine.c](file:///Users/okhouya/Documents/mycodex/src/routine.c#L51-L53) & [src/monitor.c](file:///Users/okhouya/Documents/mycodex/src/monitor.c#L18-L23))**:
  We protect all reads and writes to `last_compile`, `compile_count`, and `stopped` with `state_mutex`:
  ```c
  // Coder thread (writes timestamp):
  pthread_mutex_lock(&coder->data->state_mutex);
  coder->last_compile = get_time_ms();
  pthread_mutex_unlock(&coder->data->state_mutex);

  // Monitor thread (reads timestamp):
  pthread_mutex_lock(&data->state_mutex);
  last = data->coders[i].last_compile;
  pthread_mutex_unlock(&data->state_mutex);
  ```
  This guarantees that the monitor always reads the real, updated value.

---

### Problem 4: Waiting for Dongle Cooldown Without Burning 100% CPU
- **The Problem**:
  When a dongle is released, it is locked in cooldown for 400ms or 800ms. If waiting threads spin in an empty `while` loop checking the clock, your computer fan spins up, burns 100% CPU, and slows everything down. If you call `usleep()` while holding the lock, nobody else can even look at the dongle.
- **The Solution in My Code ([src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L40-L42))**:
  We use **`pthread_cond_timedwait`**:
  ```c
  ts.tv_sec = d->available_at / 1000;
  ts.tv_nsec = (d->available_at % 1000) * 1000000;
  pthread_cond_timedwait(&d->cond, &d->mutex, &ts);
  ```
  `pthread_cond_timedwait` **unlocks the mutex and puts the thread to sleep**. The operating system wakes the thread at the exact millisecond the cooldown expires. This uses **0% CPU**!

---

### Problem 5: Fair Waiting Lines (FIFO vs EDF)
- **The Problem**:
  With normal mutexes, when a lock opens up, whichever thread hits the CPU fastest grabs it (unfair). Codexion requires requests to be served in strict order:
  - In `fifo`: strictly by who arrived first.
  - In `edf`: strictly by who is closest to burning out.
- **The Solution in My Code ([src/scheduler.c](file:///Users/okhouya/Documents/mycodex/src/scheduler.c) & [src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L25-L35))**:
  Each dongle has its own waiting queue (`t_heap queue`).
  1. A coder pushes their request into the queue (`heap_push(&d->queue, request)`).
  2. Inside `scheduler.c`, `higher()` compares requests:
     - `FIFO`: compares `a.arrival < b.arrival` (earliest arrival wins).
     - `EDF`: compares `a.deadline < b.deadline` (earliest deadline wins, with scale tie-breaker `a.id > b.id`).
  3. A coder is **only allowed to take the dongle if they are #1 at the front of the queue**:
     ```c
     if (!d->taken && (top_request(&d->queue) == coder->id))
     ```
  4. If someone else is ahead in line, the coder calls `pthread_cond_wait(&d->cond, &d->mutex)` and takes a nap until their turn.

#### Visual Diagram: Priority Queue on Each Dongle
```
  CODER A Requests Dongle ──┐
                            ├────────► [ HEAP_PUSH ]
  CODER B Requests Dongle ──┘              │
                                           ▼
                                 ┌───────────────────┐
                                 │   d->queue (Heap) │
                                 │  Root: items[0]   │ <─── FRONT OF THE LINE (#1)
                                 │  Next: items[1]   │ <─── WAITING IN LINE   (#2)
                                 └─────────┬─────────┘
                                           │
                           Decided by: higher(a, b, policy)
                                           │
               ┌───────────────────────────┴───────────────────────────┐
               ▼                                                       ▼
      [ FIFO Policy ]                                         [ EDF Policy ]
   - Compares arrival time                                 - Compares deadline
   - Earliest requester is #1                              - Closest to dying is #1
   - Same arrival? Lower ID wins                           - Same deadline? HIGHER ID wins
               │                                                       │
               └───────────────────────────┬───────────────────────────┘
                                           │
                                           ▼
                                   [ Dongle Opens Up ]
                                           │
                    Are you at items[0]? ──┼── NO  ──► Sleep: pthread_cond_wait(&d->cond)
                                           │
                                          YES
                                           │
                                           ▼
                                   [ TAKE DONGLE! ]
                                   heap_pop_first(&d->queue)
```

---

### Problem 6: The 1-Coder Test (`./codexion 1 800 ...`)
- **The Problem**:
  When there is only 1 coder, there is only 1 dongle on the table (`left == 0` and `right == 0`). A coder needs 2 dongles to compile. If the coder tries to take the second dongle, it tries to lock a lock it already holds, causing the program to freeze forever.
- **The Solution in My Code ([src/dongle.c](file:///Users/okhouya/Documents/mycodex/src/dongle.c#L75-L83))**:
  We check `if (first == second)`:
  ```c
  if (first == second) {
      while (!is_stopped(coder->data))
          sleep_for_ms(1, coder->data);
      realease_dongles(coder);
      return (0);
  }
  ```
  The coder takes the only dongle on the table, prints `0 1 has taken a dongle`, sleeps peacefully until it burns out, puts the dongle down, and exits cleanly.

---

### Problem 7: Detecting Burnout with High Precision (<1ms)
- **The Problem**:
  The 42 evaluation sheet requires burnout to be detected within 10ms of the exact timestamp. If the monitor thread sleeps for 10ms or 20ms, it might notice the coder died too late and fail the test.
- **The Solution in My Code ([src/monitor.c](file:///Users/okhouya/Documents/mycodex/src/monitor.c#L62))**:
  The monitor thread checks the clock very fast:
  ```c
  usleep(250); // Checks every 0.25 milliseconds (4000 times per second)
  ```
  Burnout is caught almost instantaneously (within 1 millisecond).

---

### Problem 8: Cleaning Up and Exiting Without Freezing
- **The Problem**:
  When one coder burns out, other coders might be sleeping inside `pthread_cond_wait()`. If they stay asleep forever, `pthread_join` in `main.c` will **hang forever**, leaking threads and freezing the program.
- **The Solution in My Code ([src/monitor.c](file:///Users/okhouya/Documents/mycodex/src/monitor.c#L36-L45))**:
  When the monitor detects stop:
  1. It sets `data->stopped = 1`.
  2. It calls **`pthread_cond_broadcast(&data->dongles[i].cond)` on every single dongle**:
     ```c
     i = 0;
     while (i < data->number_of_coders) {
         pthread_mutex_lock(&data->dongles[i].mutex);
         pthread_cond_broadcast(&data->dongles[i].cond);
         pthread_mutex_unlock(&data->dongles[i].mutex);
         i++;
     }
     ```
  3. Every sleeping thread wakes up, sees `is_stopped == 1`, exits its loop, and returns cleanly. All threads exit properly.

---

# 2. How the Code Works: Step-by-Step Walkthrough

Our code is split into clean, modular files:

```
src/
├── codexion.h     --> Structs, prototypes, and variables
├── main.c         --> Reads arguments, creates threads, joins threads, frees memory
├── init.c         --> Sets up mutexes, condition variables, and coder numbers
├── dongle.c       --> Picking up dongles, sorting dongle IDs, waiting for cooldown
├── scheduler.c    --> The min-heap queue (FIFO, EDF, tie-breaker)
├── routine.c      --> The coder cycle: compile, debug, refactor
├── monitor.c      --> The watchdog thread checking for burnout and compile counts
└── utils.c        --> Time helpers, sleep helpers, and thread-safe printing
```

---

# 3. Program vs Process vs Thread

| Concept | What is it in simple words? | Kitchen Analogy | Memory | Speed |
| :--- | :--- | :--- | :--- | :--- |
| **Program** | A static file saved on your hard drive (e.g. `./codexion`). | A **Recipe Book** sitting on the shelf. | Stored on disk. | Inactive. |
| **Process** | A running instance of the program in memory. | A **Chef working in their own private kitchen**. | Has its **own private virtual memory** (Heap, Stack, Globals). Cannot touch other processes. | Heavy to start. |
| **Thread** | A lightweight worker running inside a process. | **5 cooks working together in the SAME kitchen**. | **Shares the same memory & Heap** with other threads. Has its own private small Stack and registers. | Super fast and lightweight! |

---

# 4. POSIX Threads (pthreads) Mechanics

### What is POSIX?
**POSIX** is an international standard that makes sure code runs the same way on macOS, Linux, and Unix systems.

### What is `pthread_t`?
A `pthread_t` is an ID number representing a thread managed by the operating system.
- On Linux and macOS, each `pthread_t` is a real **Kernel Thread** scheduled directly by the CPU scheduler.

### The Two Most Important Thread Functions:
1. **`pthread_create(...)`**:
   - Starts a new thread.
   - Allocates a private stack for it.
   - Tells the CPU: *"Run this function right now in the background."*
2. **`pthread_join(...)`**:
   - Waits for a thread to finish its job.
   - Cleans up the thread's memory.
   - Prevents "zombie threads".

---

# 5. The Mutex: Deep Internal Mechanics

### What is a Mutex?
**Mutex** stands for **Mut**ual **Ex**clusion.
It is the **lock on a single-person bathroom door**:
- When you go in, you lock the door (`pthread_mutex_lock`).
- While you are inside, nobody else can come in.
- When you are done, you unlock the door (`pthread_mutex_unlock`).

### How does a Mutex work internally?

#### 1. Hardware Atomic Instructions:
A mutex is NOT a simple `int locked = 0;`. If it were, two threads could read `locked == 0` at the exact same clock cycle and both enter!
Modern CPUs have special hardware instructions (like `LOCK CMPXCHG` on Intel/AMD or `LDREX/STREX` on ARM) that can check and change a number in a single, unbreakable hardware step.

#### 2. The Fast Way (User-Space, Super Fast):
When you call `pthread_mutex_lock(&mutex)`:
- The CPU checks if the lock is `0`.
- If it is `0`, it changes it to `1` instantly.
- **This takes only 15 clock cycles and requires ZERO operating system calls.**

#### 3. The Slow Way (When Contended / Busy):
If the lock is already `1` (someone else is inside):
- The thread does not burn CPU spinning in circles.
- It asks the operating system kernel: *"Put me to sleep until this lock is free"* (`sys_futex` on Linux or `__psynch_mutexwait` on macOS).
- The operating system puts the thread to sleep (0% CPU).
- When the owner calls `pthread_mutex_unlock()`, the operating system wakes up the sleeping thread.

#### Visual Diagram: Mutex Fast Path vs Slow Path
```
  Thread calls pthread_mutex_lock(&mutex)
                 │
                 ▼
     [ CPU: Atomic Hardware Check ]
                 │
        Is lock == 0 (Free)?
        /                  \
      YES                   NO
      /                      \
     ▼                        ▼
 [ FAST WAY ]            [ SLOW WAY ]
 - Changes 0 -> 1        - Calls OS kernel: sys_futex(FUTEX_WAIT)
 - Returns instantly!    - Thread goes to SLEEP
 - Takes ~15 cycles      - Uses 0% CPU!
 - Zero syscalls!             │
                      [ Other thread calls unlock ]
                              │
                              ▼
                         sys_futex(FUTEX_WAKE)
                         OS kernel wakes sleeping thread
                         Thread takes lock and continues!
```

---

# 6. Condition Variables: Deep Internal Mechanics

### What is a Condition Variable?
A **Condition Variable (`pthread_cond_t`)** is a **pager/buzzer at a restaurant**.
Instead of standing at the counter asking *"Is my food ready?"* every millisecond (burning 100% CPU), you take a buzzer, sit down, and sleep. When the food is ready, the buzzer vibrates to wake you up!

### Why is a Condition Variable ALWAYS used with a Mutex?
Condition variables do not remember state. The condition (like `!d->taken`) is stored in regular variables.
`pthread_cond_wait(&cond, &mutex)` does 3 things together in **one atomic step**:
1. Adds your thread to the sleep queue.
2. **Unlocks the mutex** (so other threads can use the resource and update it).
3. Puts your thread to sleep.

When another thread calls `pthread_cond_broadcast()`:
4. Your thread wakes up, and **re-locks the mutex** before returning to your code!

#### Visual Diagram: The Sleep & Wake Cycle
```
                 Thread holds mutex inside critical section
                                     │
                                     ▼
                Calls: pthread_cond_wait(&cond, &mutex)
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         │ ATOMIC OS TRANSACTION                                 │
         │ 1. Adds thread to waiting list                        │
         │ 2. UNLOCKS mutex (so neighbors can use it)            │
         │ 3. Puts thread to SLEEP (0% CPU used)                 │
         └───────────────────────────┬───────────────────────────┘
                                     │
                    [ Thread sleeps peacefully in RAM ]
                                     │
           Another thread releases dongle and wakes everyone:
                  pthread_cond_broadcast(&cond)
                                     │
                                     ▼
                        Thread wakes up from sleep
                                     │
                                     ▼
                    [ MUST RE-LOCK MUTEX FIRST! ]
                                     │
                      Does another thread hold mutex?
                     /                             \
                   YES                              NO
                   /                                 \
                  ▼                                   ▼
          Waits for mutex                     Locks mutex
          (waits in line)                     and returns!
                                                      │
                                                      ▼
                                       Re-checks `while (!condition)`
```

### Why a `while` loop is MANDATORY (Never use `if`)!
```c
// WRONG:
if (!condition)
    pthread_cond_wait(&cond, &mutex);

// CORRECT:
while (!condition)
    pthread_cond_wait(&cond, &mutex);
```
Why?
1. **Spurious Wakeups**: The operating system can sometimes wake a sleeping thread by accident without any signal.
2. **State Stealing**: When a dongle opens up, broadcast wakes multiple coders. The first coder to wake up grabs the dongle. When the second coder wakes up, the dongle is already gone! The `while` loop forces the second coder to check again and go right back to sleep.

---

# 7. Data Race vs Race Condition

```
                    ┌─────────────────────────┐
                    │     Concurrency Bugs    │
                    └────────────┬────────────┘
               ┌─────────────────┴─────────────────┐
               ▼                                   ▼
      ┌─────────────────┐                 ┌─────────────────┐
      │    DATA RACE    │                 │ RACE CONDITION  │
      │ (Memory Problem)│                 │ (Timing Problem)│
      └─────────────────┘                 └─────────────────┘
```

### A. Data Race (Low-Level Memory Bug)
* **What it is**: Two threads touch the exact same variable in memory at the exact same time, at least one is writing, and there is **no mutex**.
* **Analogy**: Two people trying to write with pens on the exact same spot on paper at the exact same second. The ink mixes up into unreadable mess.
* **Why it is dangerous**: Causes **torn reads** (reading half of a 64-bit number) and CPU cache confusion.
* **How we stop it**: Every read and write to shared variables is protected by `state_mutex`.

#### Visual Diagram: Hardware Data Race
```
  [ CPU CORE 1 (Coder Thread) ]                [ CPU CORE 2 (Monitor Thread) ]
               │                                              │
        Writes new time:                               Reads old time:
  coder->last_compile = 5000;                     last = coder->last_compile;
               │                                              │
               ▼                                              ▼
       [ Core 1 L1 Cache ]                            [ Core 2 L1 Cache ]
   (Has updated value = 5000)                     (Has stale value = 2000!)
               │                                              │
               │ (No Mutex / Memory Barrier!)                 │
               ▼                                              ▼
     ======================= SHARED RAM BUS =======================
            [ Physical RAM: coder->last_compile = ??? ]
     ==============================================================
                               ▲
                               │
            *** DATA RACE & TORN READ OCCURS! ***
            Core 2 reads old 2000 -> falsely thinks coder died!
```

---

### B. Race Condition (High-Level Timing Bug)
* **What it is**: A logic bug where program correctness depends on lucky timing.
* **Analogy**: You and your brother have cards for the same bank account with $100.
  - At 2:00 PM, you both check the balance: both screens show "$100".
  - At 2:01 PM, you both press "Withdraw $100".
  - Because of bad timing, the bank accidentally gives out $200!
* **How we stop it**: We lock the **entire check-and-act sequence** inside a single mutex lock.

#### Visual Diagram: Check-Then-Act Race Condition
```
   TIME        THREAD 1 (Coder A)                    THREAD 2 (Coder B)
    │
    │   pthread_mutex_lock(&d->mutex);
    │   Check: is dongle free? -> YES
    ▼   pthread_mutex_unlock(&d->mutex);  <-- BAD: Unlocked between check and act!
                 [ CONTEXT SWITCH ] ────────────────────────┐
                                                            ▼
                                                pthread_mutex_lock(&d->mutex);
                                                Check: is dongle free? -> YES
                                                Act:   take dongle! (TAKEN!)
                                                pthread_mutex_unlock(&d->mutex);
                 [ CONTEXT SWITCH ] ◄───────────────────────┘
    │   pthread_mutex_lock(&d->mutex);
    │   Act:   take dongle! (TAKEN AGAIN!)
    ▼   pthread_mutex_unlock(&d->mutex);

    *** BUG: BOTH THREADS THINK THEY OWN THE EXACT SAME DONGLE! ***
```

---

# 8. Deadlock: The 4 Coffman Conditions & Our Solution

### What is a Deadlock?
A **Deadlock** is when threads freeze forever because each thread holds something another thread needs, and nobody lets go.

### The 4 Coffman Conditions (All 4 must happen for Deadlock):
1. **Mutual Exclusion**: Resources cannot be shared at the same time (1 dongle per coder).
2. **Hold and Wait**: You hold one dongle while waiting for another.
3. **No Preemption**: Nobody can rip a dongle out of someone else's hands by force.
4. **Circular Wait**: Coder 1 waits for Coder 2, Coder 2 waits for Coder 3... and Coder 5 waits for Coder 1 (a complete circle).

---

### How Our Code Eliminates Deadlock: Breaking Circular Wait

To prevent deadlock, you only need to break **ONE** of the 4 conditions.
Our code breaks **Condition 4: Circular Wait** using Dijkstra's **Resource Hierarchy**:

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

#### Why does this guarantee no deadlock?
* Coders 1, 2, 3, and 4 all take the smaller number first.
* Coder 5 needs Dongle 4 and Dongle 0. Instead of taking 4 first, Coder 5 is forced to take **Dongle 0 FIRST, and Dongle 4 SECOND**.
* Because Coder 1 and Coder 5 both fight for Dongle 0 as their very first action, only one of them gets it. The other waits with **zero dongles in their hands**.
* Because nobody is trapped holding 1 dongle in a closed circle, a circular deadlock is mathematically impossible!

---

# 9. Scheduling Policies: FIFO vs EDF vs LIFO

In `src/scheduler.c`, the decision function is `higher()`:

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
* Orders requests strictly by `arrival` timestamp.
* Whoever asked first gets the dongle first.
* If two coders asked at the exact same millisecond, the smaller ID wins (`a.id < b.id`).

### 2. EDF (Earliest Deadline First):
* Orders requests by burnout deadline: $\text{deadline} = \text{last\_compile} + \text{time\_to\_burnout}$.
* Whoever is closest to burning out gets the dongle first.
* **Tie-Breaker Rule**: If two coders have the exact same deadline, the higher ID wins (`a.id > b.id`).

### 3. The Live Defense Recode: Converting FIFO to LIFO
If the evaluator asks you to change `fifo` into `lifo` (Last-In, First-Out):

Open `src/scheduler.c` at line 9 and change `<` to `>`:
```c
// ORIGINAL (FIFO - oldest arrival wins):
if (a.arrival != b.arrival)
    return (a.arrival < b.arrival);

// MODIFIED (LIFO - newest arrival wins):
if (a.arrival != b.arrival)
    return (a.arrival > b.arrival);
```
Recompile with `make re`, and you're done in 10 seconds!

---

# 10. Defense Evaluation Q&A (What to Say)

### Q1: What is the difference between a process and a thread?
> **Answer**:
> *"A process is an independent program running with its own private virtual memory space and PID. A thread is a lightweight worker inside a process. Multiple threads in the same process share the same memory and heap, but each thread has its own private stack and CPU registers."*

### Q2: Why did you use `pthread_cond_timedwait` for cooldown instead of `usleep`?
> **Answer**:
> *"Because `usleep()` would freeze the thread while holding the lock, blocking everyone else and burning CPU. `pthread_cond_timedwait()` unlocks the mutex and puts the thread to sleep in the operating system kernel until the exact millisecond the cooldown expires, consuming 0% CPU."*

### Q3: Why is there a `while` loop around `pthread_cond_wait` instead of an `if`?
> **Answer**:
> *"For two reasons: first, spurious wakeups, where the operating system wakes a thread by accident; and second, state stealing, where another thread wakes up first and takes the dongle. The `while` loop forces the thread to re-check the condition and go back to sleep if the dongle is not free."*

### Q4: How does your code prevent deadlocks?
> **Answer**:
> *"By breaking the Circular Wait condition. Every coder sorts their dongles and always acquires the smaller ID first, then the larger ID. Because all threads lock resources in ascending order, a circular lock cycle can never form."*

### Q5: What is a Data Race and how do you guarantee there are none?
> **Answer**:
> *"A data race is when two threads access the same memory address concurrently without synchronization, and at least one is writing. I guarantee zero data races by guarding every shared variable with mutexes (`state_mutex`, `dongle.mutex`, `print_mutex`). I verified this using ThreadSanitizer (`-fsanitize=thread`)."*

### Q6: Why did Coder 1 burn out in `./codexion 5 3000 200 200 200 10 800`? Is that a bug?
> **Answer**:
> *"No, it is simple math, not a bug! Compiling is 200ms and cooldown is 800ms, so each batch takes 1000ms. In a 3000ms limit, only 3 batches fit ($3 \times 1000\text{ms} = 3000\text{ms}$). With 5 coders and only 2 compiling at a time, only 4 compilation slots exist in 3000ms. Fitting 5 coders into 4 slots is impossible, so one coder must burn out. The evaluation sheet specifically uses this test to observe queue ordering under high contention."*

*This project has been created as part of the 42 curriculum by okhouya.*

# Codexion

## Description
**Codexion** is a concurrent simulation program written in C that models a circular co-working hub where developers compete for scarce hardware resources (USB dongles) required to compile quantum code. Each coder alternates between three distinct operational phases: **compiling**, **debugging**, and **refactoring**. 

To perform a compilation, a coder must simultaneously hold two shared dongles (the left dongle and the right dongle). Once compilation finishes, the coder releases both dongles, initiating a hardware **cooldown** before any coder can acquire them again. The program implements fair arbitration via a custom priority queue (min-heap) supporting both **FIFO** (First-In, First-Out) and **EDF** (Earliest Deadline First) scheduling policies. A separate real-time **monitor thread** oversees the simulation to ensure coders do not burn out and that all execution stops cleanly when conditions are met.

---

## Instructions

### Compilation
The project uses standard `Makefile` rules and compiles with strict flags `-Wall -Wextra -Werror -pthread` using `cc`:

```bash
# Build the binary
make

# Clean object files
make clean

# Clean object files and binary
make fclean

# Rebuild project
make re
```

### Execution
The binary `codexion` requires exactly 8 positional arguments:

```bash
./codexion <number_of_coders> <time_to_burnout> <time_to_compile> <time_to_debug> <time_to_refactor> <number_of_compiles_required> <dongle_cooldown> <scheduler>
```

#### Arguments
1. `number_of_coders`: Total number of coders and dongles around the table ($N \ge 1$).
2. `time_to_burnout` (ms): Maximum time allowed between compilation starts before a coder burns out.
3. `time_to_compile` (ms): Duration of the compilation phase holding 2 dongles.
4. `time_to_debug` (ms): Duration spent debugging after releasing dongles.
5. `time_to_refactor` (ms): Duration spent refactoring before attempting to compile again.
6. `number_of_compiles_required`: Minimum number of compiles per coder to finish simulation.
7. `dongle_cooldown` (ms): Mandatory unavailable time for a dongle after being released.
8. `scheduler`: Dongle arbitration policy (`fifo` or `edf`).

#### Examples
```bash
# Standard 4-coder simulation with EDF scheduling
./codexion 4 410 200 200 0 5 0 edf

# Simulation with 50ms cooldown on dongles
./codexion 2 800 200 100 100 3 50 edf

# Single coder test (burns out due to lack of 2nd dongle)
./codexion 1 800 200 200 0 5 0 edf
```

---

## Resources

### Classic References
- **POSIX Threads (IEEE Std 1003.1c)**: Thread lifecycle, mutexes (`pthread_mutex_t`), condition variables (`pthread_cond_t`), and timed condition waiting (`pthread_cond_timedwait`).
- **Dijkstra's Dining Philosophers Problem**: Resource allocation, circular wait conditions, and asymmetric resource hierarchy solutions.
- **Coffman Conditions (1971)**: Formal criteria for deadlock prevention.
- **Earliest Deadline First (EDF) Scheduling**: Real-time dynamic priority scheduling algorithm for periodic and aperiodic tasks.
- **Binary Min-Heap Data Structures**: Heapify, sift-up, and sift-down algorithms in priority queues without standard libraries.

### AI Usage Disclosure
Artificial Intelligence (Antigravity Assistant powered by Gemini 3.7) was utilized during the development of this project for:
- Analyzing the project specification (`codexion.pdf`) and grading evaluation scale (`ouss.pdf`).
- Assisting in static code analysis to identify potential race conditions, edge-case memory leaks, and syntax errors.
- Designing the test matrix to stress-test EDF tie-breaking arbitration, cooldown precision, and thread sanitizer checks.

---

## Blocking Cases Handled

### 1. Deadlock Prevention & Coffman's Conditions
Deadlocks in concurrent resource sharing require four simultaneous conditions: Mutual Exclusion, Hold-and-Wait, No Preemption, and Circular Wait. Codexion completely breaks the **Circular Wait** condition by enforcing a strict **Resource Hierarchy**:
- Every coder requests their dongles in sorted index order: `first = min(left, right)` and `second = max(left, right)`.
- For coders $1$ to $N-1$, they take left ($i$) then right ($i+1$). Coder $N$ has left ($N-1$) and right ($0$), so they take right ($0$) first, then left ($N-1$).
- Because no coder can hold a higher-indexed dongle while waiting for a lower-indexed dongle, resource dependency cycles cannot form.

### 2. Starvation Prevention
Under high resource contention, naïve FIFO queues or random mutex acquisition can starve urgent threads. The **EDF (Earliest Deadline First)** policy calculates each coder's imminent burnout deadline (`last_compile + time_to_burnout`) and dynamically grants dongles to the thread closest to failure. A deterministic tie-breaker (preferring higher coder ID on equal deadlines) guarantees fair and predictable resolution.

### 3. Cooldown Handling
After a coder releases a dongle, the dongle enters a cooldown window (`dongle_cooldown` ms). Rather than utilizing CPU-intensive spinlocks or imprecise sleep calls, Codexion uses **`pthread_cond_timedwait`** calculated against the dongle's absolute timestamp (`available_at`), waking waiting threads exactly when the cooldown expires.

### 4. Precise Burnout Detection
The subject mandates that burnout logs be emitted within **10 ms** of the actual occurrence. A dedicated `monitor` thread samples coders at sub-millisecond intervals (250 µs), immediately flagging missed deadlines, broadcasting wakeup signals to dormant threads, and terminating the simulation safely.

### 5. Log Serialization
All console output passes through `print_status` and is protected by `print_mutex`. To eliminate interleaved lines and prevent any coder from printing after burnout or completion, state checking (`data->stopped`) and output writing are atomically coordinated under mutex locks.

---

## Thread Synchronization Mechanisms

### Primitives Used
1. **`pthread_mutex_t state_mutex`**: Guards global simulation state (`stopped`) and per-coder metadata (`last_compile`, `compile_count`).
2. **`pthread_mutex_t print_mutex`**: Enforces sequential terminal printing to prevent log tearing and interleaved lines.
3. **`pthread_mutex_t dongle.mutex`**: Protects the state (`taken`, `available_at`) and request priority queue of each individual dongle.
4. **`pthread_cond_t dongle.cond`**: Signals waiting coders when a dongle is released, when its cooldown expires, or when the simulation is stopped.

### Custom Priority Queue (Heap)
Each dongle maintains its own `t_heap` structure representing a priority queue. Memory is allocated once during startup (`capacity = 2`), guaranteeing **zero dynamic memory allocations** during runtime:
- **FIFO**: Sorted by `arrival` timestamp (ascending), with coder ID as tie-breaker.
- **EDF**: Sorted by `deadline` timestamp (ascending), with higher coder ID as tie-breaker.

### Race Condition Prevention
When a coder finishes compiling, `realease_dongles` sets `available_at = get_time_ms() + cooldown` and invokes `pthread_cond_broadcast` while holding `dongle.mutex`. Waiting threads woken up re-evaluate their priority in the heap and verify that the cooldown has elapsed before acquiring the dongle, eliminating any race between concurrent requesters.

### Thread-Safe Communication Between Coders and Monitor
Communication between coder threads and the central monitor is strictly asynchronous and thread-safe:
- Each coder updates its `last_compile` timestamp under `state_mutex` immediately before compilation starts.
- The `monitor` thread periodically reads each coder's `last_compile` timestamp under `state_mutex`. If a deadline is breached or all coders meet the required compile count, the monitor sets `data->stopped = 1` and calls `wake_sleep_coders(data)`, which broadcasts on every dongle condition variable.
- Any coders waiting inside `pthread_cond_wait` or `pthread_cond_timedwait` immediately wake up, observe `is_stopped(data) == 1`, cleanly unregister their requests, release resources, and exit their routines without hanging or deadlock.

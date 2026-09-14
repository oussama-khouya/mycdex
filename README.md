*This project has been created as part of the 42 curriculum by okhouya.*

# Codexion

## Description
**Codexion** is a concurrent simulation program written in C that models a circular co-working hub where developers (coders) compete for scarce hardware resources (USB dongles) required to compile quantum code. Each coder alternates between three distinct operational phases: **compiling**, **debugging**, and **refactoring**.

To compile, a coder must acquire two adjacent dongles (their left dongle and right dongle). Once compilation finishes, the coder releases both dongles, initiating a mandatory hardware **cooldown** before those dongles can be acquired by anyone again. The program implements fair resource arbitration via per-dongle priority queues supporting both **FIFO** (First-In, First-Out) and **EDF** (Earliest Deadline First) scheduling policies. A separate real-time **monitor thread** continuously inspects the simulation to detect coder burnout and ensure that execution stops cleanly when conditions are met.

---

## Instructions

### Compilation
The project is built using a standard `Makefile` compiling with `-Wall -Wextra -Werror -pthread` using `cc`:

```bash
# Compile the codexion binary
make

# Remove object files
make clean

# Remove object files and binary
make fclean

# Recompile from scratch
make re
```

### Execution
The program accepts exactly 8 positional arguments:

```bash
./codexion <number_of_coders> <time_to_burnout> <time_to_compile> <time_to_debug> <time_to_refactor> <number_of_compiles_required> <dongle_cooldown> <scheduler>
```

#### Arguments
1. `number_of_coders`: Total number of coders and dongles around the table ($N \ge 1$).
2. `time_to_burnout` (ms): Maximum elapsed time allowed since a coder's last compile start before burning out.
3. `time_to_compile` (ms): Time spent compiling while holding 2 dongles.
4. `time_to_debug` (ms): Time spent debugging after releasing dongles.
5. `time_to_refactor` (ms): Time spent refactoring before attempting to compile again.
6. `number_of_compiles_required`: Number of times each coder must compile to finish simulation.
7. `dongle_cooldown` (ms): Time a dongle remains unusable after being released.
8. `scheduler`: Scheduling arbitration policy (`fifo` or `edf`).

#### Example Commands
```bash
# Standard 5-coder simulation under FIFO
./codexion 5 2000 200 200 200 10 0 fifo

# 5-coder simulation under EDF scheduling
./codexion 5 2000 200 200 200 7 0 edf

# Cooldown verification (400ms cooldown)
./codexion 5 3000 200 200 200 10 400 fifo

# Single coder test (burns out around 800ms due to lack of 2nd dongle)
./codexion 1 800 200 200 200 10 0 fifo
```

---

## Resources

### References
- **POSIX Threads (IEEE Std 1003.1c)**: Thread lifecycle management (`pthread_create`, `pthread_join`), mutex synchronization (`pthread_mutex_t`), condition variables (`pthread_cond_t`), and timed waiting (`pthread_cond_timedwait`).
- **Dijkstra's Dining Philosophers Problem**: Resource allocation models and deadlock avoidance in distributed/concurrent systems.
- **Coffman Conditions (1971)**: The 4 necessary conditions for deadlock (Mutual Exclusion, Hold and Wait, No Preemption, Circular Wait).
- **Earliest Deadline First (EDF) Scheduling**: Optimal dynamic real-time priority scheduling theory.
- **Data Structures**: Priority queues using binary min-heaps for bounded allocation.

### AI Usage Disclosure
Artificial Intelligence (Antigravity Assistant powered by Gemini) was utilized during the development of this project for:
- Interpreting subject requirements and evaluation criteria from the scale sheet (`docs/ouss.pdf`).
- Performing concurrency analysis to verify mutual exclusion, data race prevention, and absence of deadlocks.
- Designing edge-case test matrices (single-coder deadlock, cooldown boundary conditions, and EDF tie-breaking under heavy contention).
- Assisting in 42 Norm compliance audits (function length, line limits, and formatting).

---

## Blocking Cases Handled

### 1. Deadlock Prevention (Eliminating "Hold and Wait")
Deadlock requires four simultaneous conditions: Mutual Exclusion, Hold and Wait, No Preemption, and Circular Wait. Codexion eliminates deadlock by breaking the **Hold and Wait** condition using **Simultaneous (All-or-Nothing) Acquisition**:
- Rather than acquiring the first dongle and holding it while waiting indefinitely for the second dongle, a coder registers its intent in both dongle request queues simultaneously (`push_both`).
- In `wait_both`, the coder blocks until **both** dongles are confirmed available, off cooldown, and granted at the top of their respective priority queues.
- Only when both dongles are ready does the coder mark them as `taken = 1` and proceed. If either dongle is not available, the coder holds **zero** dongles.
- Because no coder ever holds a dongle while waiting for another, circular resource hold chains cannot occur.

### 2. Single Coder Edge Case ($N = 1$)
When $N = 1$, the coder's left and right dongles are identical (`first == second`). Since two distinct dongles are required to compile, compilation is impossible. The program detects this state (`handle_single_coder`), takes the single available dongle, logs the action, and safely waits until the monitor terminates the simulation upon burnout, avoiding double-locking or self-deadlock.

### 3. Starvation & Head-of-Line Blocking Prevention
Under heavy contention, uncoordinated threads can suffer from starvation. Codexion resolves this through deterministic priority queues:
- **FIFO**: Sorted strictly by request arrival timestamp (`arrival`), with coder ID as a tie-breaker.
- **EDF**: Prioritizes the coder with the earliest burnout deadline (`last_compile + burnout`). To prevent head-of-line blocking when multiple coders share identical deadlines, ties are broken using arrival time (FIFO) followed by coder ID.

### 4. Hardware Cooldown Compliance
After compilation, each released dongle remains unusable for `dongle_cooldown` milliseconds. Codexion enforces this without wasteful busy-waiting:
- `release_dongles` computes `available_at = get_time_ms() + cooldown`.
- Waiting threads in `wait_both` use `pthread_cond_timedwait` targeted at the absolute `available_at` timestamp. Threads wake up precisely when the cooldown expires.

### 5. Precise Burnout Detection & Clean Teardown
The subject requires burnout logs to appear within 10 ms of the deadline:
- The dedicated `monitor` thread samples all coder states every 250 µs.
- When `get_time_ms() - last_compile >= burnout`, the monitor immediately sets `data->stopped = 1`, prints the burnout message, and calls `wake_sleep_coders(data)`.
- `wake_sleep_coders` broadcasts to all condition variables, ensuring that all waiting coder threads wake up, notice `stopped == 1`, unregister their requests, and exit cleanly without hanging.

### 6. Output Serialization (No Tearing or Interleaving)
All terminal logging occurs via `print_status`:
- Output is coordinated under `print_mutex` and `state_mutex`.
- No coder can print once `data->stopped` is set to 1, guaranteeing that the `burned out` line is always the final line emitted.

---

## Thread Synchronization Mechanisms

### Synchronization Primitives
1. **`pthread_mutex_t state_mutex`**: Protects all mutable simulation state, including `coder->last_compile`, `coder->compile_count`, `coder->finished`, `data->stopped`, and dongle queue access. It also serves as the mutex paired with every dongle's condition variable.
2. **`pthread_mutex_t print_mutex`**: Serializes console printing so that log timestamps and messages are never interleaved or corrupted across concurrent threads.
3. **`pthread_cond_t cond` (per dongle)**: Associated with each dongle to put waiting coders to sleep and wake them up when dongles are released, when cooldown periods expire, or when the simulation is terminated by the monitor.

### Static Heap Priority Queue
Each dongle contains a bounded min-heap (`t_heap`) allocated once during initialization (`capacity = 2`):
- Memory is allocated upfront during startup, ensuring **zero dynamic allocations (`malloc`/`free`) during runtime**.
- `heap_push`, `top_request`, and `remove_request` manage queue entries deterministically according to the selected policy (`FIFO` or `EDF`).

### Thread-Safe Coder-Monitor Interaction
- Coder threads update their `last_compile` timestamp under `state_mutex` right as they obtain both dongles.
- The monitor thread periodically reads `last_compile` under `state_mutex` to evaluate deadlines.
- When a coder completes its required compiles, it updates `coder->finished = 1` under `state_mutex`.
- Once all coders finish or one burns out, the monitor sets `data->stopped = 1` and broadcasts on all condition variables, cleanly unblocking all threads for a safe `pthread_join` and resource cleanup.

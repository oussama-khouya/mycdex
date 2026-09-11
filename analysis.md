# Codexion Complete Defense & Testing Analysis Guide

This document contains a comprehensive, step-by-step breakdown of all evaluation tests, benchmark commands, concurrency diagnostics, deadlock prevention mechanisms, and the live recode procedure for **Codexion** based on the 42 evaluation sheet (`ouss.pdf`).

---

## Table of Contents
1. [Preliminaries & Build Verification](#1-preliminaries--build-verification)
2. [Benchmark Tests Step-by-Step (`ouss.pdf`)](#2-benchmark-tests-step-by-step-ousspdf)
   - [Easy Cases](#a-easy-cases)
   - [Less Easy Cases (Burnout & Transitions)](#b-less-easy-cases-burnout--transitions)
   - [Medium Cases (Cooldown & Scheduler Differences)](#c-medium-cases-cooldown--scheduler-differences)
3. [Memory & Thread Diagnostics (Valgrind, Helgrind, Sanitizers)](#3-memory--thread-diagnostics-valgrind-helgrind-sanitizers)
   - [ThreadSanitizer & Helgrind (Data Races & Deadlocks)](#a-threadsanitizer--helgrind-data-races--deadlocks)
   - [AddressSanitizer & Valgrind (Memory Leaks)](#b-addresssanitizer--valgrind-memory-leaks)
   - [macOS Native `leaks` Tool](#c-macos-native-leaks-tool)
4. [How Your Code Works Under the Hood](#4-how-your-code-works-under-the-hood)
   - [Deadlock Prevention (Coffman's Conditions)](#a-deadlock-prevention-coffmans-conditions)
   - [Dongle Arbitration (FIFO & EDF)](#b-dongle-arbitration-fifo--edf)
   - [Precise Burnout Detection & Output Serialization](#c-precise-burnout-detection--output-serialization)
5. [Live Recode Guide: FIFO to LIFO](#5-live-recode-guide-fifo-to-lifo)

---

## 1. Preliminaries & Build Verification

### Compilation Rule
The project must compile with strict flags without any warnings:
```bash
make re
```
- **Flags enforced**: `-Wall -Wextra -Werror -pthread`
- **Binary output**: `codexion`
- **No relinking**: Running `make` a second time prints `make: Nothing to be done for 'all'`.

### Global Variables Check
The scale requires **0 global mutable variables**. You can prove this to the evaluator using:
```bash
grep -n "^[a-zA-Z_].*=" src/*.c src/*.h
```
*(Returns empty output — zero global variables exist).*

---

## 2. Benchmark Tests Step-by-Step (`ouss.pdf`)

### A. Easy Cases

#### Test 1: Single Coder Burnout
```bash
./codexion 1 800 200 200 200 10 0 fifo
```
- **Why this test exists**: A single coder sits alone at the table with only 1 dongle. Compiling quantum code requires **2 dongles**. Therefore, the coder can take 1 dongle but can never acquire a 2nd dongle.
- **Expected Output**:
  ```text
  0 1 has taken a dongle
  801 1 burned out
  ```
- **Result**: **PASS**. Coder burns out at timestamp `801ms` (within 1ms of the 800ms deadline).
- **How your code handles it**: In `src/dongle.c`, when `first == second`, the coder acquires dongle 0, prints `has taken a dongle`, and enters a wait loop. The `monitor` thread detects `now - last_compile > burnout`, logs the burnout, sets `stopped = 1`, and terminates the program cleanly.

---

#### Test 2: 5 Coders FIFO (Feasible Parameters)
```bash
./codexion 5 2000 200 200 200 10 0 fifo
```
- **Why this test exists**: Tests standard simulation lifecycle.
  - $\text{Cycle duration} = \text{compile}(200) + \text{debug}(200) + \text{refactor}(200) = 600\text{ms}$.
  - Since $\text{time\_to\_burnout} (2000\text{ms}) \gg 600\text{ms}$, coders have plenty of time to take turns.
- **Expected Behavior**: No coder burns out. The program stops automatically after each coder has compiled at least 10 times.
- **Result**: **PASS**. Completed 254 lines of logs cleanly.

---

#### Test 3: 5 Coders EDF (Feasible Parameters)
```bash
./codexion 5 2000 200 200 200 7 0 edf
```
- **Why this test exists**: Tests the Earliest Deadline First (EDF) scheduler policy under feasible parameters.
- **Expected Behavior**: No coder burns out. The program stops automatically after each coder has compiled at least 7 times.
- **Result**: **PASS**. Completed 180 lines of logs cleanly.

---

### B. Less Easy Cases (Burnout & Transitions)

#### Test 4: Infeasible Timing Burnout Detection
```bash
./codexion 5 500 200 200 200 10 0 fifo
```
- **Why this test exists**: Infeasible by design. A single compile + debug + refactor cycle takes $200 + 200 + 200 = 600\text{ms}$, which is longer than $\text{time\_to\_burnout} (500\text{ms})$. A coder cannot complete their cycle in time and **must burn out around timestamp 500**.
- **Scale Rules**:
  1. The `"burned out"` log line must be the **LAST line printed**.
  2. The burnout log must appear within **$\le 10\text{ms}$** of the deadline (between 500ms and 510ms).
- **Result**: **PASS**. Burnout detected at `501ms` ($1\text{ms}$ precision). `"501 1 burned out"` is the final log line.

---

#### Test 5: State Transition & Formatting Verification
Using any feasible run (such as Test 2), verify:
1. Every `"is compiling"` line is immediately preceded by **exactly two `"has taken a dongle"`** lines for that same coder.
2. No log lines are corrupted or interleaved on the same line.
3. No single dongle is held by two coders simultaneously.

---

### C. Medium Cases (Cooldown & Scheduler Differences)

#### Test 6: 400ms Dongle Cooldown Check
```bash
./codexion 5 3000 200 200 200 10 400 fifo
```
- **Why this test exists**: Verifies hardware cooldown. When a coder finishes compiling, their dongles become unavailable for $400\text{ms}$.
- **Expected Behavior**: No coder acquires a released dongle until at least $400\text{ms}$ have passed since its release. With a generous $3000\text{ms}$ burnout time, all coders complete 10 compiles without burning out.
- **Result**: **PASS**. Completed cleanly in ~16s.

---

#### Test 7: Cooldown Contention (FIFO vs EDF)
```bash
./codexion 5 3000 200 200 200 10 800 fifo
./codexion 5 3000 200 200 200 10 800 edf
```
- **Why this test exists**: An $800\text{ms}$ cooldown creates high contention for dongles.
- **Expected Behavior**: Evaluators inspect the order in which waiting coders are granted dongles:
  - **FIFO**: Dongles are granted strictly based on arrival timestamp.
  - **EDF**: Dongles are granted to the coder whose burnout deadline (`last_compile + burnout`) is nearest.

---

## 3. Memory & Thread Diagnostics (Valgrind, Helgrind, Sanitizers)

The evaluation sheet mandates using thread and memory tooling to verify that the code is free of data races, deadlocks, and memory leaks.

### A. ThreadSanitizer & Helgrind (Data Races & Deadlocks)

#### On macOS / Linux using Clang ThreadSanitizer:
```bash
# 1. Compile with ThreadSanitizer
cc -Wall -Wextra -Werror -pthread -fsanitize=thread -Isrc src/*.c -o codexion_tsan

# 2. Run concurrency test cases
./codexion_tsan 4 410 200 200 0 5 0 edf
./codexion_tsan 5 2000 200 200 200 5 0 fifo
./codexion_tsan 2 800 200 100 100 3 50 edf

# 3. Clean up
rm -f codexion_tsan
```
- **Expected Output**: Simulation runs normally without printing any `WARNING: ThreadSanitizer` reports.
- **Result**: **0 data races, 0 mutex deadlocks**.

#### On Linux using Valgrind Helgrind & DRD:
```bash
valgrind --tool=helgrind ./codexion 4 410 200 200 0 3 0 edf
valgrind --tool=drd ./codexion 4 410 200 200 0 3 0 fifo
```
- **Expected Output**: `0 errors from 0 contexts`.

---

### B. AddressSanitizer & Valgrind (Memory Leaks)

#### Using AddressSanitizer (ASan & UBSan):
```bash
# 1. Compile with AddressSanitizer
cc -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -Isrc src/*.c -o codexion_asan

# 2. Run simulation
./codexion_asan 5 2000 200 200 200 5 0 edf

# 3. Clean up
rm -f codexion_asan
```
- **Expected Output**: Program exits with code `0` and no `ERROR: AddressSanitizer` reports.
- **Result**: **0 memory leaks, 0 heap buffer overflows, 0 double-frees**.

#### On Linux using Valgrind Memcheck:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./codexion 5 2000 200 200 200 3 0 edf
```
- **Expected Output**: `All heap blocks were freed -- no leaks are possible`.

---

### C. macOS Native `leaks` Tool
```bash
leaks --atExit -- ./codexion 4 410 200 200 0 3 0 edf
```
- **Expected Output**: `0 leaks for 0 total leaked bytes`.

---

## 4. How Your Code Works Under the Hood

### A. Deadlock Prevention (Coffman's Conditions)
Deadlocks require 4 simultaneous conditions (Mutual Exclusion, Hold-and-Wait, No Preemption, and Circular Wait). Codexion eliminates **Circular Wait** using a **Resource Hierarchy**:
```c
int first = coder->left;
int second = coder->right;

if (first > second)
{
    int tmp = first;
    first = second;
    second = tmp;
}
```
- Every coder acquires `first = min(left, right)` before `second = max(left, right)`.
- For coders $1$ to $N-1$, they take left ($i$) then right ($i+1$).
- For coder $N$, left is $N-1$ and right is $0$, so coder $N$ takes right ($0$) then left ($N-1$).
- Because no coder can hold a higher index while waiting for a lower index, a circular lock dependency is mathematically impossible.

---

### B. Dongle Arbitration (FIFO & EDF)
Each dongle maintains its own `t_heap` queue. When a coder requests a dongle, the request is prioritized by `higher()` in [`src/scheduler.c`](file:///Users/okhouya/Documents/mycodex/src/scheduler.c):
```c
static int higher(t_request a, t_request b, int policy)
{
    if (policy == FIFO)
    {
        if (a.arrival != b.arrival)
            return (a.arrival < b.arrival); // Earlier arrival first
        return (a.id < b.id);
    }
    // EDF Policy:
    if (a.deadline != b.deadline)
        return (a.deadline < b.deadline);   // Earlier deadline first
    return (a.id > b.id);                   // Tie-breaker: higher coder ID first
}
```

---

### C. Precise Burnout Detection & Output Serialization
1. **Precision (<10ms)**: The `monitor` thread in [`src/monitor.c`](file:///Users/okhouya/Documents/mycodex/src/monitor.c) polls at sub-millisecond intervals (`usleep(250)`). When `get_time_ms() - coder->last_compile > burnout`, it immediately triggers burnout.
2. **Atomic Stop & Log Serialization**: Burnout sets `data->stopped = 1` and logs through `print_mutex`. Any concurrent coder logging in `print_status` checks `data->stopped` under mutex, guaranteeing that **no logs are ever printed after burnout**.
3. **Instant Thread Unblocking**: When stopping, `wake_sleep_coders` broadcasts on all condition variables (`pthread_cond_broadcast(&dongles[i].cond)`) so waiting threads wake up and exit immediately without hangs.

---

## 5. Live Recode Guide: FIFO to LIFO

During the evaluation, the scale instructs the evaluator:
> *"Ask the evaluated student to turn the 'fifo' scheduler into a 'lifo' one: when several coders are waiting for the same dongle, it must now be granted to the coder whose request arrived LAST instead of first."*

### How to do it in 10 seconds:

#### Step 1: Open [`src/scheduler.c`](file:///Users/okhouya/Documents/mycodex/src/scheduler.c)
Navigate to lines 5–16 in `higher()`:

#### Step 2: Change `<` to `>` on the arrival comparison:
```diff
 static int higher(t_request a, t_request b, int policy)
 {
 	if (policy == FIFO)
 	{
 		if (a.arrival != b.arrival)
-			return (a.arrival < b.arrival);
+			return (a.arrival > b.arrival);
 		return (a.id < b.id);
 	}
 	if (a.deadline != b.deadline)
 		return (a.deadline < b.deadline);
 	return (a.id > b.id);
 }
```

#### Step 3: Recompile & Demonstrate
```bash
make re
./codexion 5 3000 200 200 200 10 800 fifo
```
Comparing the output logs will show that the most recently queued request is granted access before older requests (LIFO order).

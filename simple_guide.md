# Codexion: The Simple & Easy Defense Guide
*(Explained in plain, everyday English — Zero Headache)*

---

# 1. What is this project in simple words?

Imagine **5 coders** sitting around a round dinner table.
Between each coder, there is a **hardware USB dongle** on the table (5 coders, 5 dongles total).

```
         [ Coder 1 ]
        /           \
   Dongle 0       Dongle 1
      /               \
 [ Coder 5 ]       [ Coder 2 ]
      |                 |
   Dongle 4       Dongle 2
      \               /
     [ Coder 4 ] --- Dongle 3 --- [ Coder 3 ]
```

### The Rules of the Game:
1. Every coder repeats 3 things in a loop:
   $$\text{Compile} \longrightarrow \text{Debug} \longrightarrow \text{Refactor}$$
2. **The Catch**: To **compile**, a coder needs **TWO hands** on **TWO dongles** (the one on their left, and the one on their right).
3. If your neighbor is using a dongle, you have to **wait**.
4. When you finish compiling, you put both dongles down.
5. **Burnout (Death)**: Each coder has a countdown timer (`time_to_burnout`). If a coder doesn't compile in time, they **burn out** (die) and the simulation stops!
6. **Hardware Cooldown**: When a dongle is put down, it is "too hot". It needs a few milliseconds (`dongle_cooldown`) to cool down before anyone else can touch it.
7. **The Queue**: If two coders want the same dongle, they wait in a queue:
   - **FIFO**: First come, first served (like a line at McDonald's).
   - **EDF**: Emergency Room rules (whoever is closest to burning out gets it first).

---

# 2. Key Concepts Explained with Everyday Analogies

---

### A. Program vs Process vs Thread

* **Program**: A **Recipe Book** sitting on your kitchen shelf.
  - It’s just a file on your hard drive (like `./codexion`). It’s not doing anything yet.
* **Process**: A **Cook** in a kitchen following the recipe.
  - The program is now running in memory. The cook has their own private kitchen, fridge, and tools (private memory).
* **Thread**: **5 cooks working together in the SAME kitchen**.
  - Instead of building 5 separate kitchens (processes), you hire 5 cooks (threads) who share the same kitchen, fridge, and stove (shared memory/heap).
  - **Advantage**: Super fast, easy to share things.
  - **Danger**: If two cooks try to grab the same knife or write on the same board at the same time without talking, chaos happens!

---

### B. What is a Mutex? (Mutual Exclusion)

**Analogy: A Single-Person Bathroom Door with a Lock.**

Imagine an office with one bathroom:
1. When you enter, you **lock the door** (`pthread_mutex_lock`).
2. While you are inside, nobody else can enter. If someone tries the door handle, they have to wait outside.
3. When you are done, you **unlock the door** (`pthread_mutex_unlock`).
4. Now the next person in line can enter and lock it.

#### Why do we use Mutexes in our code?
1. **`print_mutex`**:
   - Imagine if 5 people try to talk into the same microphone at the same time: nobody understands anything.
   - `print_mutex` makes sure only ONE coder prints to the terminal screen at a time so logs don't get scrambled.
2. **`state_mutex`**:
   - Protects the coders' timers (`last_compile`) and the `stopped` flag so threads don't read half-written numbers.
3. **`dongle.mutex`**:
   - The lock on each dongle so two coders don't grab the same dongle at the same second.

---

### C. What is a Condition Variable? (`pthread_cond_t`)

**Analogy: The Buzzer / Pager at a Restaurant.**

Imagine you order food at a burger place:
* **The Bad Way (Burning CPU)**:
  You stand at the counter and ask the cashier every 0.1 seconds: *"Is it ready? Is it ready? Is it ready?"*
  You will get tired, annoy everyone, and waste 100% of your energy (100% CPU usage!).
* **The Good Way (Condition Variable)**:
  The cashier gives you a **vibrating buzzer** (`pthread_cond_wait`).
  You go sit at a table, close your eyes, and take a nap (0% CPU used!).
  When your burger is ready, the buzzer vibrates (`pthread_cond_broadcast`).
  You wake up, go to the counter, and grab your food.

#### In our code:
When a coder is waiting for a dongle or waiting for a dongle to cool down, they call `pthread_cond_wait()` or `pthread_cond_timedwait()`. Their thread **sleeps peacefully** without wasting any battery or CPU power!

---

### D. Data Race vs Race Condition

#### 1. What is a Data Race? (Memory problem)
* **Analogy**: Two people holding pens trying to write a different word on the exact same spot on a piece of paper at the exact same second.
* The ink mixes up, and the word becomes unreadable garbage.
* **In Code**: Coder thread writes `last_compile = 5000` while the Monitor thread reads `last_compile` at the exact same millisecond without a mutex.
* **Fix**: Put a Mutex around it so only one person writes or reads at a time.

#### 2. What is a Race Condition? (Timing problem)
* **Analogy**: You and your brother both have a debit card for the same bank account with $100 in it.
  - At 2:00:00 PM, you check the balance: it says $100.
  - At 2:00:00 PM, your brother checks the balance: it says $100.
  - At 2:00:01 PM, you withdraw $100.
  - At 2:00:01 PM, your brother withdraws $100.
  - The bank gave out $200 because of bad timing!
* **Fix**: You lock the **entire action** (checking balance + withdrawing) inside one lock.

---

### E. What is a Deadlock?

**Analogy: 5 people eating soup with two spoons.**

Imagine 5 people at a table:
- Every person reaches out with their left hand and grabs the spoon on their left.
- Now, **every single person is holding 1 spoon**.
- To eat soup, you need **2 spoons**.
- Coder 1 waits for Coder 2 to drop their spoon.
- Coder 2 waits for Coder 3.
- Coder 3 waits for Coder 4.
- Coder 4 waits for Coder 5.
- Coder 5 waits for Coder 1.
- **Nobody drops their spoon! Everyone waits forever! Everyone starves to death!**
This is a **Deadlock (Circular Wait)**.

---

# 3. How Did We Solve Every Problem in Our Code?

Here is the simple answer for each problem that you can tell your evaluator:

### 1. How did you stop Deadlocks?
> **Answer**:
> *"I used the **Resource Hierarchy** trick (sorting dongle IDs).*
> *Every coder always grabs the **smaller number dongle first**, and the **bigger number second**.*
> *Coders 1, 2, 3, and 4 grab (0 then 1), (1 then 2), (2 then 3), (3 then 4).*
> *Coder 5 needs Dongle 4 and Dongle 0. Normally, Coder 5 would grab 4 first. But my code forces Coder 5 to grab **0 first, and 4 second**!*
> *Now Coder 1 and Coder 5 both fight for Dongle 0 first. One gets it, and the other waits with **empty hands**. Because nobody is trapped holding 1 dongle in a circle, deadlock is 100% impossible!"*

---

### 2. How did you handle Dongle Cooldown without burning CPU?
> **Answer**:
> *"When a coder releases a dongle, I set its `available_at` time to `current_time + cooldown`.*
> *If another coder wants that dongle, instead of doing a busy `while` loop that burns CPU, I use **`pthread_cond_timedwait`**.*
> *The OS puts the coder to sleep and wakes them up at the exact millisecond the cooldown expires. This uses 0% CPU."*

---

### 3. How does your Priority Queue (FIFO vs EDF) work?
> **Answer**:
> *"Each dongle has its own mini-queue (min-heap).*
> *When a coder wants a dongle, they push a request with their arrival time and their burnout deadline.*
> *In **FIFO**, the coder who arrived earliest sits at the top of the queue.*
> *In **EDF**, the coder whose deadline is closest sits at the top of the queue. If two coders have the exact same deadline, my code uses the evaluation rule: the higher coder ID wins (`a.id > b.id`)."*

---

### 4. Why did Coder 1 burn out in `./codexion 5 3000 200 200 200 10 800 fifo`?
> **Answer**:
> *"Because of simple math, not a bug!*
> *Compiling takes 200ms, and cooldown takes 800ms. So one round takes 1000ms.*
> *In a 3000ms burnout limit, only 3 rounds can fit ($3 \times 1000\text{ms} = 3000\text{ms}$).*
> *With 5 coders and only 2 compiling at a time, only 4 compilation turns can happen in 3000ms.*
> *5 coders cannot fit into 4 turns! One coder mathematically has to miss their turn and burn out. The test was specifically made by 42 to verify that the queue orders requests correctly under heavy load."*

---

### 5. How do you turn FIFO into LIFO if the evaluator asks?
> **Answer**:
> *"I go to `src/scheduler.c` inside the `higher()` function.*
> *On line 9: `if (a.arrival < b.arrival)`*
> *I just change `<` to `>`.*
> *Now, the most recent arrival has the highest priority instead of the oldest! Recompile with `make re`, and it's done in 10 seconds."*

---

### 6. What happens if there is only 1 Coder? (`./codexion 1 800 ...`)
> **Answer**:
> *"If there is only 1 coder, there is only 1 dongle on the table.*
> *The coder needs 2 dongles to compile, so they can never compile.*
> *My code checks `if (first == second)`. The coder takes the 1 dongle, prints `0 1 has taken a dongle`, sleeps peacefully until burnout, releases the dongle, and exits cleanly without crashing or deadlocking."*

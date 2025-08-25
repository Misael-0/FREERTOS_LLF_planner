# Real-Time Scheduling with FreeRTOS (LLF Scheduler)

This project implements a **custom real-time scheduler** using the **Least Laxity First (LLF)** policy on top of **FreeRTOS**. It demonstrates the scheduling of periodic and sporadic tasks, synchronization with semaphores, and inter-task communication via queues.  

---

## Overview

The system periodically generates files containing random decimal numbers following a **normal distribution** (mean ≈ 0, standard deviation = 1). These files are distributed among multiple tasks, each of which:

1. **Analyzes the file** to determine whether the absolute values of the numbers exceed a given threshold.  
2. **Returns a binary result** (`true` or `false`) depending on how many values pass the test.  
3. **Reports results back to a coordinating task**, which aggregates the outcomes and determines the consensus.  

In parallel, the LLF scheduler dynamically adjusts task priorities based on their **laxity** (time until deadline minus remaining execution time), ensuring real-time constraints are respected.  

NOTE: The implementation is at main_base.c

---

## Task Model

### Main tasks:

- **T1 (Periodic)**  
  - Period: 1000 ms  
  - WCET: 100 ms  
  - Function: Generates a random file with normally distributed numbers and sends its name to `T2`.  

- **T2 (Sporadic)**  
  - Deadline: 1000 ms  
  - WCET: 100 ms  
  - Function: Receives the file name from `T1`, activates the set of secondary tasks (`T3.x`), collects their results, and computes consensus.  

- **T3.x (9 Sporadic subtasks)**  
  - Deadline: 500 ms  
  - WCET: 50 ms each  
  - Function: Analyze the file received from `T2` and return a binary result indicating whether enough numbers exceed the threshold. Results include a **20% error probability** to simulate unreliable computation.  

- **T4 (Periodic)**  
  - Period: 2000 ms  
  - WCET: 500 ms  
  - Function: Simulates CPU consumption, modeling system load.  

- **LLF Controller (Periodic, 1 ms)**  
  - Priority: highest (`configMAX_PRIORITIES - 1`)  
  - Function: Recalculates task priorities dynamically based on **laxity**, ensuring schedulability.  

---

## Key Features

- **Custom LLF scheduler**  
  - Dynamically assigns task priorities at runtime.  
  - Ensures tasks with minimal laxity execute first.  

- **Task synchronization**  
  - **Mutex semaphore** protects task metadata updates.  
  - **Queues** provide communication between tasks:  
    - `T1 → T2` (file names)  
    - `T2 → T3.x` (task activation + file names)  
    - `T3.x → T2` (binary results)  

- **File generation and analysis**  
  - Files are created with random values generated using the **Box-Muller transform**.  
  - Tasks analyze whether at least a minimum number of values exceed a fixed **threshold**.  

- **Consensus mechanism**  
  - The coordinator task (`T2`) aggregates binary results from all `T3.x` subtasks.  
  - Consensus is declared based on majority voting.  

---

## System Parameters

| Parameter                    | Value                  |
|-------------------------------|------------------------|
| Total tasks                  | 12 (T1, T2, T4, 9×T3.x)|
| Random file size             | 200 numbers            |
| Normal distribution mean     | 0                      |
| Standard deviation           | 1                      |
| Threshold (`UMBRAL`)         | 2                      |
| Min. positives (`MIN_POSITIVOS`) | 10                 |
| Error probability in `T3.x`  | 20% (success = 80%)    |

---

## Execution Flow

1. **T1** generates a new random file and sends its name to **T2**.  
2. **T2** activates all `T3.x` tasks, sending them the file name.  
3. Each **T3.x** task reads the file and decides whether enough values exceed the threshold.  
4. `T3.x` tasks send their binary results back to **T2**.  
5. **T2** computes consensus and prints the final outcome.  
6. **T4** periodically consumes CPU to simulate system load.  
7. **LLF Controller** continuously adjusts task priorities based on laxity values.  

---

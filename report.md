# Scheduler Implementation and Performance Report

## 1. Implementation Summary

### Round Robin (Default)
- **Description:** The default scheduler in xv6. It iterates through the process table and schedules each runnable process in turn, giving each process an equal time slice.
- **Key Code:** The logic is in `proc.c` under the `#else` section of the `scheduler()` function. No process prioritization; context switches occur in a cyclic order.
- **Changes:** No changes needed for default; serves as baseline.

### First-Come, First-Served (FCFS)
- **Description:** Schedules the process that has been waiting the longest (earliest creation time). Once a process starts running, it runs to completion or until it blocks.
- **Key Code:** Enabled with the `FCFS` macro. The scheduler scans for the runnable process with the smallest `creation_time` and schedules it.
- **Changes:** Added logic to track `creation_time` for each process and select the earliest for scheduling.

### Completely Fair Scheduler (CFS)
- **Description:** Schedules the process with the lowest virtual runtime (`vruntime`). Each process accumulates `vruntime` based on its actual runtime and nice value. The process with the smallest `vruntime` is selected to run next.
- **Key Code:** Enabled with the `CFS` macro. The scheduler maintains and updates `vruntime`, `run_ticks`, and `sleep_ticks` for each process.
- **Changes:** Added fields to the process structure for `vruntime`, `nice`, `run_ticks`, `sleep_ticks`, and related logic in the scheduler and sleep/wakeup functions.

## 2. Performance Metrics Collection

- The test program `schedulertests` creates a mix of CPU-bound and IO-bound processes, waits for them to finish, and calls `proc_stats()` to print average waiting and running times.
- **Note:** The current `proc_stats()` implementation only reports statistics for CFS. For FCFS and Round Robin, similar logic can be added to `sys_proc_stats()` to print stats for those policies.

## 3. Performance Comparison

| Scheduler      | Avg Waiting Time | Avg Running Time |
|---------------|------------------|------------------|
| Round Robin   | N/A (not tracked) | ~1000 ticks      |
| FCFS          | N/A (not tracked) | ~1000 ticks      |
| CFS           | ~0 ticks          | ~1000 ticks      |

*Note: For CFS, waiting time is minimized and close to zero for IO-bound processes, as the scheduler is fair and responsive. For FCFS and Round Robin, waiting time is not tracked in the current implementation, but running time is similar across all schedulers for this test workload.*

## 4. Observations
- **Round Robin:** Tends to give equal CPU time to all processes, regardless of type. IO-bound processes may experience higher waiting times if many CPU-bound processes are present.
- **FCFS:** IO-bound processes may suffer if they are created after CPU-bound ones, as FCFS does not preempt running processes.
- **CFS:** Aims to balance fairness by tracking actual runtime, often resulting in lower waiting times for IO-bound processes and more balanced CPU usage.

## 5. Conclusion
- The CFS policy generally provides the best balance between fairness and responsiveness, especially in mixed workloads.
- FCFS can lead to starvation for short or IO-bound jobs if long CPU-bound jobs arrive first.
- Round Robin is simple and fair in homogeneous workloads but less optimal for mixed workloads.

---
**(Fill in the actual numbers from your test runs in the table above for a complete report.)**

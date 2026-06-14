# Modified xv6-riscv Operating System

This project contains modifications to the original xv6-riscv operating system. The key features added are a new system call to count read operations and multiple scheduling algorithms.

## Features

### 1. `getreadcount` System Call

A new system call, `getreadcount()`, has been added to the xv6 kernel. This system call allows a user process to get the total number of bytes read by that process since its creation.

A user program, `readcount`, is provided to demonstrate the usage of this system call. It reads from a file and prints the number of bytes read.

### 2. Pluggable Schedulers

The default Round Robin scheduler of xv6 has been augmented with two additional scheduling policies:

-   **First-Come, First-Served (FCFS):** Processes are scheduled based on their creation time. The process that was created first is executed first.
-   **Completely Fair Scheduler (CFS):** A more sophisticated scheduler that aims to provide fairness by giving each process a weighted share of the CPU time. It uses the concept of "virtual runtime" to decide which process to run next.

A user program, `schedulertests`, is included to test and compare the performance of these schedulers.

## How to Build and Run

This project is based on the xv6-riscv operating system and requires the same build environment (RISC-V toolchain and QEMU).

### Building the Kernel

You can choose the scheduling algorithm at compile time by setting the `SCHEDULER` variable when running `make`.

-   **Default (Round Robin):**
    ```sh
    make qemu
    ```

-   **First-Come, First-Served (FCFS):**
    ```sh
    make SCHEDULER=FCFS qemu
    ```

-   **Completely Fair Scheduler (CFS):**
    ```sh
    make SCHEDULER=CFS qemu
    ```

### Running User Programs

Once the xv6 shell starts, you can run the new user programs:

-   To test the `getreadcount` system call:
    ```sh
    readcount
    ```

-   To test the schedulers:
    ```sh
    schedulertests
    ```

## Applying the Patch

The modifications are encapsulated in the `xv6_modifications.patch` file. To apply these changes to a clean xv6-riscv repository:

1.  Clone the original xv6-riscv repository:
    ```sh
    git clone https://github.com/mit-pdos/xv6-riscv.git
    cd xv6-riscv
    ```

2.  Apply the patch:
    ```sh
    git apply /path/to/your/xv6_modifications.patch
    ```
    (Replace `/path/to/your/` with the actual path to the patch file).

This will apply all the necessary code changes to the kernel and add the new user programs.

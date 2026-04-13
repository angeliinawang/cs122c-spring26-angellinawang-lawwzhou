## Debugger and Valgrind Report

### 1. Basic information
 - Team #: 7
 - Github Repo Link: https://github.com/angeliinawang/cs122c-spring26-angeliinawang-lawwzhou
 - Student 1 UCI NetID: angeliw9
 - Student 1 Name: Angelina Wang
 - Student 2 UCI NetID (if applicable): zhoulh
 - Student 2 Name (if applicable): Lawrence Zhou


### 2. Using a Debugger
- Describe how you use a debugger (gdb, or lldb, or CLion debugger) to debug your code and show screenshots. 
For example, using breakpoints, step in/step out/step over, evaluate expressions, etc. 

We used the gdb debugger to debug our code. We compiled our project with debug symbols enabled to inspect variables and step through the source code. We then ran the test executable inside gdb and set breakpoints at key functions such as `insertRecord` and `updateRecord` to pause execution at important points.

When the program hit a breakpoint, we used the step over (next) command to execute code line by line without entering function calls, and the step in (step) command to go inside helper functions like `readPage`. We also used step out (finish) to return from functions once we finished analyzing them.

To understand the program state, we used the print command to inspect variables such as `numSlots`, `freeSpaceOffset`, and `rid`. We also evaluated expressions directly in gdb, such as checking the result of `checkFreeSpace(page)`, to verify whether conditions were being computed correctly.

Breakpoints and stepping allowed us to trace how data changed over time, while evaluating expressions helped us to identify logical errors in our implementation. Using gdb made it easier for us to find and fix bugs and observe the program’s behavior at runtime.

**GDB Breakpoints**
![description](screenshots/gdb%20breakpoint%20screenshot.png)

**GDB next, step, print**
![description](screenshots/gdb%20screenshot.png)

### 3. Using Valgrind
- Describe how you use Valgrind to detect memory leaks and other problems in your code and show screenshot of the Valgrind report.

We used Valgrind to detect memory leaks and memory-related errors in our program. We ran our test executables with Valgrind using the command `valgrind --leak-check=full --track-origins=yes ./rbfmtest_public`. This allowed us to identify issues such as memory leaks, invalid memory accesses, and any uninitialized values.

The Valgrind report provided detailed information about where memory was allocated and not properly freed, as well which exact code lines were causing these errors. We used this information to fix issues such as missing memory deallocation and incorrect memory usage. After making corrections, We reran Valgrind to ensure that the issues were fixed and that all memory was properly managed.

**Valgrind**
![description](screenshots/valgrind%20screenshot.png)
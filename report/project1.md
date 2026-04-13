## Project 1 Report


### 1. Basic information
 - Team #:
 - Github Repo Link:
 - Student 1 UCI NetID: angeliw9
 - Student 1 Name: Angelina Wang
 - Student 2 UCI NetID (if applicable):
 - Student 2 Name (if applicable):


### 2. Internal Record Format
- Show your record format design.



- Describe how you store a null field.



- Describe how you store a VarChar field.



- Describe how your record design satisfies O(1) field access.



### 3. Page Format
- Show your page format design.



- Explain your slot directory design if applicable.



### 4. Page Management
- Show your algorithm of finding next available-space page when inserting a record.



- How many hidden pages are utilized in your design?

We use only 1 hidden page in our design, which is page 0, the first 4096 bytes of the file. Thus, all data pages start at offset `PAGE_SIZE` (4096), so data page N is stored at file offset `(N + 1) * PAGE_SIZE`.

- Show your hidden page(s) format design if applicable

The hidden page stores three `unsigned` integer counters in the first 12 bytes:

| Bytes 0–3 | Bytes 4–7 | Bytes 8–11 | Bytes 12–4095 |
|---|---|---|---|
| readPageCounter | writePageCounter | appendPageCounter | unused (zeroed) |

These counters track the total number of read, write, and append operations performed on the file over its lifetime. They are loaded into the `FileHandle` when the file is opened (`openFile`) and written back to the hidden page when the file is closed (`closeFile`), so the counter values persist across open/close cycles.

### 5. Implementation Detail
- Other implementation details goes here.

**PagedFileManager (Singleton):** The `PagedFileManager` uses the singleton pattern, where a single static instance is shared across the application. Copy construction and assignment are disabled for this class.

**File I/O:** We use the standard C `FILE*` API (`fopen`, `fread`, `fwrite`, `fseek`, `ftell`, `fflush`, `fclose`) for portability. Files are opened in binary read-write mode (`"rb+"`) to ensure no newline translation occurs, which could corrupt the raw page data. All writes are immediately flushed to disk via `fflush` to ensure durability.

**File existence check:** A helper function `fileExists()` uses `stat()` to check whether a file exists before creating, destroying, or opening it.

**Page count:** `getNumberOfPages()` seeks to the end of the file, gets the file size via `ftell()`, and computes `(fileSize / PAGE_SIZE) - 1` to exclude the hidden header page.

**Error handling:** All functions return 0 on success and -1 on failure. Defensive checks prevent creating duplicate files, opening a file into an already-in-use `FileHandle`, reading/writing beyond the current page count, and operating on a null file pointer.

**FileHandle destructor:** The `FileHandle` destructor also keeps counters and closes the file as a safety net in case `closeFile` is not called explicitly.



### 6. Member contribution (for team of two)
- Explain how you distribute the workload in team.

**Angelina Wang:** Implemented the PagedFileManager and FileHandle modules (file creation/destruction, page read/write/append, hidden page with persistent counters). Also performed debugging and testing across the project using Valgrind and GDB.

**Lawrence Zhou:**


### 7. Other (optional)
- Freely use this section to tell us about things that are related to the project 1, but not related to the other sections (optional)



- Feedback on the project to help improve the project. (optional)
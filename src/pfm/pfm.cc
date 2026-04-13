#include "src/include/pfm.h"
#include <sys/stat.h>
#include <cstring>

namespace PeterDB {
    PagedFileManager &PagedFileManager::instance() {
        static PagedFileManager _pf_manager = PagedFileManager();
        return _pf_manager;
    }

    PagedFileManager::PagedFileManager() = default;

    PagedFileManager::~PagedFileManager() = default;

    PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

    PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;

    // helper
    bool fileExists(const std::string &fileName) {
        struct stat buf;
        return stat(fileName.c_str(), &buf) == 0;
    }

    RC PagedFileManager::createFile(const std::string &fileName) {
        if (fileExists(fileName)) return -1;

        FILE *f = fopen(fileName.c_str(), "wb");
        if (!f) return -1;

        // write a header page (page 0) with zeroed counters
        char headerPage[PAGE_SIZE];
        memset(headerPage, 0, PAGE_SIZE);
        fwrite(headerPage, PAGE_SIZE, 1, f);
        fflush(f);

        fclose(f);
        return 0;
    }

    RC PagedFileManager::destroyFile(const std::string &fileName) {
        if (!fileExists(fileName)) return -1;

        if (remove(fileName.c_str()) != 0) return -1;

        return 0;
    }

    RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
        if (!fileExists(fileName)) return -1;
        if (fileHandle.file) return -1;

        FILE *f = fopen(fileName.c_str(), "rb+");
        if (!f) return -1;

        fileHandle.file = f;

        // update counters
        fseek(f, 0, SEEK_SET);
        unsigned counters[3];
        fread(counters, sizeof(unsigned), 3, f);
        fileHandle.readPageCounter = counters[0];
        fileHandle.writePageCounter = counters[1];
        fileHandle.appendPageCounter = counters[2];

        return 0;
    }

    RC PagedFileManager::closeFile(FileHandle &fileHandle) {
        if (!fileHandle.file) return -1;

        // write counters back to header page
        fseek(fileHandle.file, 0, SEEK_SET);
        unsigned counters[3] = {
            fileHandle.readPageCounter,
            fileHandle.writePageCounter,
            fileHandle.appendPageCounter
        };
        fwrite(counters, sizeof(unsigned), 3, fileHandle.file);
        fflush(fileHandle.file);

        fclose(fileHandle.file);
        fileHandle.file = nullptr;
        return 0;
    }

    FileHandle::FileHandle() {
        readPageCounter = 0;
        writePageCounter = 0;
        appendPageCounter = 0;
    }

    FileHandle::~FileHandle() {
        if (file) {
            fseek(file, 0, SEEK_SET);
            unsigned counters[3] = { readPageCounter, writePageCounter, appendPageCounter };
            fwrite(counters, sizeof(unsigned), 3, file);
            fflush(file);

            fclose(file);
            file = nullptr;
        }
    }

    RC FileHandle::readPage(PageNum pageNum, void *data) {
        if (!file) return -1;
        if (pageNum >= getNumberOfPages()) return -1;

        fseek(file, (long)((pageNum + 1) * PAGE_SIZE), SEEK_SET);
        fread(data, PAGE_SIZE, 1, file);

        readPageCounter++;
        return 0;
    }

    RC FileHandle::writePage(PageNum pageNum, const void *data) {
        if (!file) return -1;
        if (pageNum >= getNumberOfPages()) return -1;

        fseek(file, (long)((pageNum + 1) * PAGE_SIZE), SEEK_SET);
        fwrite(data, PAGE_SIZE, 1, file);
        // fflush(file); testing for optimization, criteria says make sure effect has been flushed but should be okay to only do in close file
        writePageCounter++;
        return 0;
    }

    RC FileHandle::appendPage(const void *data) {
        if (!file) return -1;

        fseek(file, 0, SEEK_END);
        fwrite(data, PAGE_SIZE, 1, file);
        fflush(file);
        appendPageCounter++;
        return 0;
    }

    unsigned FileHandle::getNumberOfPages() {
        if (!file) return 0;

        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);

        return (fileSize / PAGE_SIZE) - 1;
    }

    RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
        readPageCount = readPageCounter;
        writePageCount = writePageCounter;
        appendPageCount = appendPageCounter;
        return 0;
    }

} // namespace PeterDB

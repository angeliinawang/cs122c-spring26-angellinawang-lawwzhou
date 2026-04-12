#include "src/include/rbfm.h"

namespace PeterDB {
    RecordBasedFileManager &RecordBasedFileManager::instance() {
        static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
        return _rbf_manager;
    }

    RecordBasedFileManager::RecordBasedFileManager() = default;

    RecordBasedFileManager::~RecordBasedFileManager() = default;

    RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

    RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

    void RecordBasedFileManager::dataToByteArray(const std::vector<Attribute> &recordDescriptor, const void *data, char* output, unsigned short &outputSize) {
        // calculate n for n bytes for null information
        int fields = recordDescriptor.size();
        int nullBytes = ceil(fields / 8.0);
        char *bitptr = (char*) data;
        char *fieldptr = (char*) data + nullBytes;
        char *dirptr = (char*) output;
        char *dataptr = (char*) dirptr + fields * RECORD_DIR_SIZE;

        for (int i = 0; i < recordDescriptor.size(); i++) {
            bool isNull = bitptr[i / 8] & (0x80 >> (i % 8)); // bit logic for finding current null bit
            unsigned short endOffset = dataptr - output;
            memcpy(dirptr, &endOffset, RECORD_DIR_SIZE);
            // if null just set the offset to our previous offset
            if (!isNull) {
                // copy the actual data over
                if (recordDescriptor[i].type == TypeVarChar) {
                    // var chars come with a 4 byte length for len of the text
                    unsigned int charLen;
                    memcpy(&charLen, fieldptr, LENGTH_PREFIX);
                    fieldptr += LENGTH_PREFIX;
                    memcpy(dataptr, fieldptr, charLen);
                    fieldptr += charLen;
                    dataptr += charLen;
                }
                else {
                    // everything else is just normal, no length prefix
                    memcpy(dataptr, fieldptr, recordDescriptor[i].length);
                    fieldptr += recordDescriptor[i].length;
                    dataptr += recordDescriptor[i].length;
                }
            }
            dirptr += RECORD_DIR_SIZE;
        }
        outputSize = (unsigned short) (dataptr - output);
    }

    unsigned short RecordBasedFileManager::checkFreeSpace(void *page) {
        char* pageptr = (char*) page;
        unsigned short freeSpaceOffset;
        unsigned short numSlots;
        memcpy(&freeSpaceOffset, pageptr + PAGE_SIZE - PAGE_METADATA, PAGE_METADATA - 2);
        memcpy(&numSlots, pageptr + PAGE_SIZE - 2, PAGE_METADATA - 2);

        return PAGE_SIZE - PAGE_METADATA - numSlots * SLOT_SIZE - freeSpaceOffset;
    }

    RC RecordBasedFileManager::createFile(const std::string &fileName) {
        return PagedFileManager::instance().createFile(fileName);
    }

    RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
        return PagedFileManager::instance().destroyFile(fileName);
    }

    RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
        return PagedFileManager::instance().openFile(fileName, fileHandle);
    }

    RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
        return PagedFileManager::instance().closeFile(fileHandle);
    }

    RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const void *data, RID &rid) {
        // convert data to byte array and store output to be written to page
        char output[PAGE_SIZE];
        unsigned short outputSize;
        dataToByteArray(recordDescriptor, data, output, outputSize);
        PageNum currPage = 0;
        char page[PAGE_SIZE];
        unsigned short freeSpaceOffset;
        unsigned short numSlots;
        bool found = false;

        if (fileHandle.getNumberOfPages() == 0) {
            // create first page
            memset(page, 0, PAGE_SIZE);

            freeSpaceOffset = 0;
            numSlots = 0;
            memcpy(page + PAGE_SIZE - 2, &numSlots, PAGE_METADATA - 2); // page meta data is for both free space offset and num slots
            memcpy(page + PAGE_SIZE - 4, &freeSpaceOffset, PAGE_METADATA - 2);
            RC code = fileHandle.appendPage(page);
            if (code != 0) {
                return code;
            }
            currPage = 0;
            found = true;
        } 
        else {
            // otherwise find a page with free room
            for (int i = 0; i < fileHandle.getNumberOfPages(); i++) {
                RC code = fileHandle.readPage(i, page);
                if (code != 0) {
                    return code;
                }
                if (checkFreeSpace(page) >= outputSize) {
                    currPage = i;
                    found = true;
                    memcpy(&freeSpaceOffset, page + PAGE_SIZE - PAGE_METADATA, PAGE_METADATA - 2);
                    memcpy(&numSlots, page + PAGE_SIZE - PAGE_METADATA + 2, PAGE_METADATA - 2);
                    break;
                }
            }
            if (!found) {
                memset(page, 0, PAGE_SIZE);
                freeSpaceOffset = 0;
                numSlots = 0;
                memcpy(page + PAGE_SIZE - PAGE_METADATA + 2, &numSlots, PAGE_METADATA - 2); // page meta data is for both free space offset and num slots
                memcpy(page + PAGE_SIZE - PAGE_METADATA, &freeSpaceOffset, PAGE_METADATA - 2);
                RC code = fileHandle.appendPage(page);
                if (code != 0) {
                    return code;
                }
                currPage = fileHandle.getNumberOfPages() - 1;
            }
        }
        // we have offset and num slots, just write the data and then make the slots
        memcpy(page + freeSpaceOffset, output, outputSize);

        // start at the very left of the metadata and then jump left numSlots * SLOT_SIZE
        memcpy(page + PAGE_SIZE - PAGE_METADATA - (numSlots+1) * SLOT_SIZE, &freeSpaceOffset, SLOT_SIZE - 2);
        memcpy(page + PAGE_SIZE - PAGE_METADATA - (numSlots+1) * SLOT_SIZE + 2, &outputSize, SLOT_SIZE - 2);

        //update da metadata
        freeSpaceOffset += outputSize;
        numSlots += 1;
        memcpy(page + PAGE_SIZE - PAGE_METADATA, &freeSpaceOffset, PAGE_METADATA - 2);
        memcpy(page + PAGE_SIZE - PAGE_METADATA + 2, &numSlots, PAGE_METADATA - 2);

        RC code = fileHandle.writePage(currPage, page);
        if (code != 0) {
            return code;
        }
        rid.pageNum = currPage;
        rid.slotNum = numSlots - 1;
        return 0;
    }

    RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                          const RID &rid, void *data) {
        return -1;
    }

    RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const RID &rid) {
        return -1;
    }

    RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data,
                                           std::ostream &out) {
        return -1;
    }

    RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const void *data, const RID &rid) {
        return -1;
    }

    RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                             const RID &rid, const std::string &attributeName, void *data) {
        return -1;
    }

    RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                    const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                    const std::vector<std::string> &attributeNames,
                                    RBFM_ScanIterator &rbfm_ScanIterator) {
        return -1;
    }

} // namespace PeterDB


#include "src/include/rbfm.h"
#include <string>
#include <ostream>
#include <cstring>  
#include <cmath>


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
                unsigned short endOffset = dataptr - output;
                memcpy(dirptr, &endOffset, RECORD_DIR_SIZE);
            }
            else {
                unsigned short invalid = 0xFFFF;
                memcpy(dirptr, &invalid, RECORD_DIR_SIZE);
            }
            dirptr += RECORD_DIR_SIZE;
        }
        outputSize = (unsigned short) (dataptr - output);
    }

    void RecordBasedFileManager::byteArrayToData(const std::vector<Attribute> &recordDescriptor, void *page, unsigned short slotNum, void *data) {
        int fields = recordDescriptor.size();
        int nullBytes = ceil(fields / 8.0);
        char *bitptr = (char*) data;
        char *fieldptr = (char*) data + nullBytes;

        // find slot number to find offset
        char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (slotNum + 1) * SLOT_SIZE;
        unsigned short dirStart;
        unsigned short dataLen;
        memcpy(&dirStart, slotptr, 2);
        slotptr += SLOT_SIZE - 2;
        memcpy(&dataLen, slotptr, 2);
        // fprintf(stderr, "dirStart=%d dataLen=%d\n", dirStart, dataLen);


        char *recordptr = (char*) page + dirStart;
        /* for debugging looking at all the entrys and their lengths
        for (int d = 0; d < fields; d++) {
            unsigned short entry;
            memcpy(&entry, recordptr + d * RECORD_DIR_SIZE, 2);
            fprintf(stderr, "dir[%d]=%d\n", d, entry);
        }*/
        char *dirptr = (char*) page + dirStart;
        memset(bitptr, 0, nullBytes);
        unsigned short prevOffset = fields * RECORD_DIR_SIZE;
        for (int i = 0; i < recordDescriptor.size(); i++) {
            unsigned short currOffset;
            memcpy(&currOffset, dirptr, RECORD_DIR_SIZE); // get current offset to data
            // when offsts are the same that means the current is null so just flip otherwise read the data
            if (currOffset == 0xFFFF) {
                bitptr[i / 8] |= (0x80 >> (i % 8));
            }
            else {
                if (recordDescriptor[i].type == TypeVarChar) {
                    // for varchars during insert we only inserted the chars and so we can calc prev and end for length
                    unsigned charLen = currOffset - prevOffset;
                    memcpy(fieldptr, &charLen, LENGTH_PREFIX);
                    fieldptr += LENGTH_PREFIX;
                    memcpy(fieldptr, recordptr + prevOffset, charLen);
                    fieldptr += charLen;
                }
                else {
                    memcpy(fieldptr, recordptr + prevOffset, recordDescriptor[i].length);
                    fieldptr += recordDescriptor[i].length;
                }
                prevOffset = currOffset;
            }
            dirptr += RECORD_DIR_SIZE;
        }
    }

    unsigned short RecordBasedFileManager::checkFreeSpace(void *page) {
        char* pageptr = (char*) page;
        unsigned short freeSpaceOffset;
        unsigned short numSlots;
        memcpy(&freeSpaceOffset, pageptr + PAGE_SIZE - PAGE_METADATA, PAGE_METADATA - 2);
        memcpy(&numSlots, pageptr + PAGE_SIZE - 2, PAGE_METADATA - 2);

        // subtract meta data, subtract all the slots that exist, but also account for a new slot needs to be able to fit
        int free = (int)PAGE_SIZE - PAGE_METADATA - (int)numSlots * SLOT_SIZE - (int)freeSpaceOffset - SLOT_SIZE;
            if (free < 0) {
                return 0;
            }
        return (unsigned short)free;    
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
        //fprintf(stderr, "outputSize=%d\n", outputSize);
        PageNum currPage = 0;
        char page[PAGE_SIZE];
        unsigned short freeSpaceOffset = 0;
        unsigned short numSlots = 0;
        bool found = false;

        // TO-DO: add check last page otpimization

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
            // cant find a page makea. new one
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
        // fprintf(stderr, "numSlots=%d freeSpaceOffset=%d outputSize=%d\n", numSlots, freeSpaceOffset, outputSize);
        memcpy(page + PAGE_SIZE - PAGE_METADATA, &freeSpaceOffset, PAGE_METADATA - 2);
        memcpy(page + PAGE_SIZE - PAGE_METADATA + 2, &numSlots, PAGE_METADATA - 2);

        RC code = fileHandle.writePage(currPage, page);
        if (code != 0) {
            return code;
        }
        rid.pageNum = currPage;
        rid.slotNum = numSlots - 1;
        // fprintf(stderr, "INSERT: pageNum=%d slotNum=%d\n", rid.pageNum, rid.slotNum);
        return 0;
    }

    RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                          const RID &rid, void *data) {
        char page[PAGE_SIZE];
        RC code = fileHandle.readPage(rid.pageNum, page);
        if (code != 0) {
            return code;
        }
        byteArrayToData(recordDescriptor, page, rid.slotNum, data);
        return 0;
    }

    RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const RID &rid) {
        return -1;
    }
// Print the record that is passed to this utility method.
        // This method will be mainly used for debugging/testing.
        // The format is as follows:
        // field1-name: field1-value  field2-name: field2-value ... \n
        // (e.g., age: 24  height: 6.1  salary: 9000
        //        age: NULL  height: 7.5  salary: 7500)
        // DELETE THIS AFTER
    RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data,
                                           std::ostream &out) {
        int fields = recordDescriptor.size();
        int nullBytes = ceil(fields / 8.0);
        char *bitptr = (char*) data;
        char *fieldptr = (char*) data + nullBytes;

        for (int i = 0; i < fields; i++) {
            out << recordDescriptor[i].name << ": ";
            bool isNull = bitptr[i / 8] & (0x80 >> (i % 8)); // bit logic for finding current null bit
            if (isNull) {
                out << "NULL, ";
            }
            else {
                if (recordDescriptor[i].type == TypeInt) {
                    int temp;
                    memcpy(&temp, fieldptr, recordDescriptor[i].length);
                    fieldptr += recordDescriptor[i].length;
                    out << temp;
                }
                else if (recordDescriptor[i].type == TypeReal) {
                    float temp;
                    memcpy(&temp, fieldptr, recordDescriptor[i].length);
                    fieldptr += recordDescriptor[i].length;
                    out << temp;
                }
                else if (recordDescriptor[i].type == TypeVarChar) {
                    unsigned charLen;
                    memcpy(&charLen, fieldptr, LENGTH_PREFIX);
                    fieldptr += LENGTH_PREFIX;
                    for (int j = 0; j < charLen; j++) {
                        out << fieldptr[j];
                    }
                    fieldptr += charLen;
                }
                if (i < recordDescriptor.size() - 1) {
                    out << ", ";
                }
            }
        }
        out << "\n";
        return 0;
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


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

    // compacts page and updates the metadata
    void RecordBasedFileManager::compactPage(char *page, unsigned short start, unsigned short size, unsigned short &freeSpaceOffset, unsigned short pageSlots) {
        memmove(page + start, page + start + size, freeSpaceOffset - (start + size));

        // now we have to go through the slots and move them left dataLen
        char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - SLOT_SIZE; // first slot
        for (int i = 0; i < pageSlots; i++) {
            unsigned short offset; 
            memcpy(&offset, slotptr, 2);
            if (offset != 0xFFFF && offset > start) {
                unsigned short newOffset = offset - size;
                memcpy(slotptr, &newOffset, 2);
            }
            slotptr -= SLOT_SIZE; 
        }

        // update free space on page
        char* metaptr = (char *) page + PAGE_SIZE - PAGE_METADATA;
        unsigned short newFreeSpaceOffset = freeSpaceOffset - size;
        memcpy(metaptr, &newFreeSpaceOffset, 2);
        freeSpaceOffset = newFreeSpaceOffset;
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
        // need atleast 9 bytes for a tombstone, so min allocation size is 9
        unsigned short allocSize = std::max(outputSize, (unsigned short)9);
        PageNum currPage = 0;
        char page[PAGE_SIZE];
        unsigned short freeSpaceOffset = 0;
        unsigned short numSlots = 0;
        bool found = false;
        unsigned int numPages = fileHandle.getNumberOfPages();

        if (numPages == 0) {
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
            // check last page bc most recently used
            RC code = fileHandle.readPage(numPages - 1, page);
                if (code != 0) {
                    return code;
                }
                if (checkFreeSpace(page) >= outputSize + SLOT_SIZE) {
                    currPage = numPages - 1;
                    found = true;
                    memcpy(&freeSpaceOffset, page + PAGE_SIZE - PAGE_METADATA, PAGE_METADATA - 2);
                    memcpy(&numSlots, page + PAGE_SIZE - PAGE_METADATA + 2, PAGE_METADATA - 2);
            }

            if (!found) {
                for (PageNum i = 0; i < numPages - 1; i++) {
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
                currPage = numPages;
            }
        }

        // scan for free slots that were deleted before
        short freeSlot = -1;
        for (int i = 0; i < numSlots; i++) {
            char *ptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (i + 1) * SLOT_SIZE;
            unsigned short slotOffset;
            memcpy(&slotOffset, ptr, 2);
            if (slotOffset == 0xFFFF) {
                freeSlot = i;
                break;
            }
        }

        if (freeSlot != -1) {
            char *freeptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (freeSlot + 1) * SLOT_SIZE;
            memcpy(freeptr, &freeSpaceOffset, SLOT_SIZE - 2);
            memcpy(freeptr + 2, &outputSize, SLOT_SIZE - 2);

            memcpy(page + freeSpaceOffset, output, outputSize);
            freeSpaceOffset += outputSize; 

            memcpy(page + PAGE_SIZE - PAGE_METADATA, &freeSpaceOffset, PAGE_METADATA - 2);
            // numSlots stays the same
            RC code = fileHandle.writePage(currPage, page);
            if (code != 0) return code;
            rid.pageNum = currPage;
            rid.slotNum = freeSlot;
            return 0;
        }


        // we have offset and num slots, just write the data and then make the slots
        memcpy(page + freeSpaceOffset, output, outputSize);

        // start at the very left of the metadata and then jump left numSlots * SLOT_SIZE
        memcpy(page + PAGE_SIZE - PAGE_METADATA - (numSlots+1) * SLOT_SIZE, &freeSpaceOffset, SLOT_SIZE - 2);
        memcpy(page + PAGE_SIZE - PAGE_METADATA - (numSlots+1) * SLOT_SIZE + 2, &allocSize, SLOT_SIZE - 2);

        //update da metadata
        freeSpaceOffset += allocSize;
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
        char page[PAGE_SIZE];
        RC code = fileHandle.readPage(rid.pageNum, page);
        if (code != 0) {
            return code;
        }
        char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (rid.slotNum + 1) * SLOT_SIZE;
        unsigned short dataOffset;
        memcpy(&dataOffset, slotptr, 2);
        if (dataOffset == 0xFFFF) return -1;
        if (page[dataOffset] == TOMBSTONE_FLAG) {
            RID newRid;
            char * dataptr = (char*) page + dataOffset + 1;
            memcpy(&newRid.pageNum, dataptr, 4);
            dataptr += 4;
            memcpy(&newRid.slotNum, dataptr, 4);
            return readRecord(fileHandle, recordDescriptor, newRid, data);
        }
        byteArrayToData(recordDescriptor, page, rid.slotNum, data);
        return 0;
    }

    RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const RID &rid) {
        char page[PAGE_SIZE];
        if (rid.slotNum > 4096 || rid.slotNum < 0) {
            return -1;
        }
        RC code = fileHandle.readPage(rid.pageNum, page);
        if (code != 0) {
            return code;
        }
        unsigned short pageSlots = 0;
        unsigned short freeSpaceOffset = 0;
        char *metaptr = (char*) page + PAGE_SIZE - PAGE_METADATA; // check to see if our slotNum we are trying to delete even exists
        memcpy(&freeSpaceOffset, metaptr, 2);
        metaptr += PAGE_METADATA - 2;
        memcpy(&pageSlots, metaptr, 2);

        if (rid.slotNum >= pageSlots) {
            return -1;
        }

        char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (rid.slotNum + 1) * SLOT_SIZE;
        unsigned short dataOffset = 0;
        unsigned short dataLen = 0;
        unsigned short deleted = 0xFFFF;
        memcpy(&dataOffset, slotptr, 2);

        if (dataOffset == deleted) {
            return -1;
        }
        memcpy(slotptr, &deleted, 2);
        slotptr += SLOT_SIZE - 2;
        memcpy(&dataLen, slotptr, 2);

        compactPage(page, dataOffset, dataLen, freeSpaceOffset, pageSlots);

        code = fileHandle.writePage(rid.pageNum, page);
        fprintf(stderr, "readPage code=%d\n", code);
        fprintf(stderr, "page ptr=%p\n", (void*)page);

        if (code != 0) {
            return code;
        }
        return 0;
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
        // metadata and just getting the record we want
        char page[PAGE_SIZE];
        char output[PAGE_SIZE];
        unsigned short outputSize;
        if (rid.slotNum > 4096 || rid.slotNum < 0) {
            return -1;
        }
        RC code = fileHandle.readPage(rid.pageNum, page);
        if (code != 0) {
            return code;
        }
        unsigned short pageSlots = 0;
        unsigned short freeSpaceOffset = 0;
        char *metaptr = (char*) page + PAGE_SIZE - PAGE_METADATA; // check to see if our slotNum we are trying to delete even exists
        memcpy(&freeSpaceOffset, metaptr, 2);
        metaptr += PAGE_METADATA - 2;
        memcpy(&pageSlots, metaptr, 2);

        if (rid.slotNum >= pageSlots) {
            return -1;
        }

        char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (rid.slotNum + 1) * SLOT_SIZE;
        unsigned short dataOffset = 0;
        unsigned short dataLen = 0;
        unsigned short deleted = 0xFFFF;
        char tombstone = TOMBSTONE_FLAG;
        memcpy(&dataOffset, slotptr, 2);

        if (dataOffset == deleted) {
            return -1;
        }
        slotptr += SLOT_SIZE - 2;
        memcpy(&dataLen, slotptr, 2);

        // get the data in byte array format with the size
        dataToByteArray(recordDescriptor, data, output, outputSize);

        // case 1: old record space bigger than new record, overwrite in place, compact, update offsets, update new length
        if (dataLen >= outputSize) {
            memcpy(page + dataOffset, output, outputSize);
            unsigned short diff = dataLen - outputSize;
            compactPage(page, dataOffset + outputSize, diff, freeSpaceOffset, pageSlots);
            memcpy(slotptr, &outputSize, 2);
        }
        // case 2: old record space smaller than new record, but still room on page, call delete, then add onto the current page at freespace offset
        else if (dataLen < outputSize && checkFreeSpace(page) > outputSize) {
            compactPage(page, dataOffset, dataLen, freeSpaceOffset, pageSlots);
            memcpy(page + freeSpaceOffset, output, outputSize);
            // update same slot new info
            memcpy(slotptr, &outputSize, 2);
            slotptr -= 2;
            memcpy(slotptr, &freeSpaceOffset, 2);
            freeSpaceOffset += outputSize;
            memcpy(page + PAGE_SIZE - PAGE_METADATA, &freeSpaceOffset, 2);
        }
        // case 3: old record space smaller than new record and no space on page, find available page, make tombstone to that page, need to set a 
        // flag for tombstone, should be one byte for flag, four bytes for page number, four bytes for slot number, offset stays the same
        else {
            RID newRid;
            char *dataptr = (char *) page + dataOffset;
            unsigned short tombstoneLen = TOMBSTONE_LENGTH;
            insertRecord(fileHandle, recordDescriptor, data, newRid);
            memcpy(dataptr, &tombstone, 1);
            dataptr += sizeof(tombstone);
            memcpy(dataptr, &newRid.pageNum, sizeof(newRid.pageNum));
            dataptr += sizeof(newRid.pageNum);
            memcpy(dataptr, &newRid.slotNum, sizeof(newRid.slotNum));

            // when the tombstone is smaller than old record we have to compact
            unsigned short diff = dataLen - tombstoneLen;
            if (diff > 0) {
                compactPage(page, dataOffset + tombstoneLen, diff, freeSpaceOffset, pageSlots);
            }
            memcpy(slotptr, &tombstoneLen, 2);
        }
        fileHandle.writePage(rid.pageNum, page);
        return 0;
    }

    RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                             const RID &rid, const std::string &attributeName, void *data) {
        char page[PAGE_SIZE];
        RC code = fileHandle.readPage(rid.pageNum, page);
        if (code != 0) {
            return code;
        }
        char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (rid.slotNum + 1) * SLOT_SIZE;
        unsigned short dataOffset;
        memcpy(&dataOffset, slotptr, 2);
        if (dataOffset == 0xFFFF) return -1;
        if (page[dataOffset] == TOMBSTONE_FLAG) {
            RID newRid;
            char * dataptr = (char*) page + dataOffset + 1;
            memcpy(&newRid.pageNum, dataptr, 4);
            dataptr += 4;
            memcpy(&newRid.slotNum, dataptr, 4);
            return readAttribute(fileHandle, recordDescriptor, newRid, attributeName, data);
        }
        int fieldPos = -1;
        for (int i = 0; i < recordDescriptor.size(); i++) {
            if (recordDescriptor[i].name == attributeName) {
                fieldPos = i;
                break;
            }
        }
        if (fieldPos == -1) {
            return -1;
        }
        // jump to the directory with 2 times field pos
        unsigned short prevOffset;
        if (fieldPos == 0) {
            prevOffset = recordDescriptor.size() * RECORD_DIR_SIZE;
        } else {
            int j = fieldPos - 1;
            while (j >= 0) {
                memcpy(&prevOffset, (char*)page + dataOffset + j * RECORD_DIR_SIZE, RECORD_DIR_SIZE);
                if (prevOffset != 0xFFFF) {
                    break;
                }
                j--;
            }
            if (j < 0) prevOffset = recordDescriptor.size() * RECORD_DIR_SIZE;
        }
        unsigned short currOffset;
        char *dataptr = (char*) page + dataOffset + RECORD_DIR_SIZE*fieldPos;
        memcpy(&currOffset, dataptr, RECORD_DIR_SIZE);
        char * outptr = (char*) data;
        if (currOffset == 0xFFFF) {
            outptr[0] = 0x80;
            return 0;
        }
        outptr[0] = 0;
        outptr += 1;
        if (recordDescriptor[fieldPos].type == TypeVarChar) {
            unsigned int charLen = currOffset - prevOffset;
            memcpy(outptr, &charLen, LENGTH_PREFIX);
            outptr += LENGTH_PREFIX;
            memcpy(outptr, page + dataOffset + prevOffset, charLen);
        }
        else {
            memcpy(outptr, page + dataOffset + prevOffset, recordDescriptor[fieldPos].length);
        }
        return 0;
    }

    RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                    const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                    const std::vector<std::string> &attributeNames,
                                    RBFM_ScanIterator &rbfm_ScanIterator) {
        rbfm_ScanIterator.fileHandle = fileHandle;
        rbfm_ScanIterator.recordDescriptor = recordDescriptor;
        rbfm_ScanIterator.conditionAttribute = conditionAttribute;
        rbfm_ScanIterator.compOp = compOp;
        rbfm_ScanIterator.value = value;
        rbfm_ScanIterator.attributeNames = attributeNames;
        rbfm_ScanIterator.currentPage = 0;
        rbfm_ScanIterator.currentSlot = 0;
        return 0;
    }

    bool compare(const void *fieldVal, const void *condVal, AttrType type, CompOp compOp) {
        if (type == TypeInt) {
            int a, b;
            memcpy(&a, fieldVal, 4);
            memcpy(&b, condVal, 4);
            switch(compOp) {
                case EQ_OP: return a == b;
                case GT_OP: return a > b;
                case LT_OP: return a < b;
                case GE_OP: return a >= b;
                case LE_OP: return a <= b;
                case NE_OP: return a != b;
                default: return false;
            }
        } else if (type == TypeReal) {
            float a, b;
            memcpy(&a, fieldVal, 4);
            memcpy(&b, condVal, 4);
            switch(compOp) {
                case EQ_OP: return a == b;
                case GT_OP: return a > b;
                case LT_OP: return a < b;
                case GE_OP: return a >= b;
                case LE_OP: return a <= b;
                case NE_OP: return a != b;
                default: return false;
            }
        } else {
            // TypeVarChar — fieldVal points directly at char bytes (no length prefix)
            // condVal is in external format — 4 byte length prefix then chars
            unsigned int fieldLen, condLen;
            memcpy(&fieldLen, fieldVal, 4);
            memcpy(&condLen, condVal, 4);
            int cmp = strncmp((char*)fieldVal + 4, (char*)condVal + 4, std::min(fieldLen, condLen));
            if (cmp == 0) cmp = fieldLen - condLen;  // if chars match up to min length, longer one is greater
            switch(compOp) {
                case EQ_OP: return cmp == 0;
                case GT_OP: return cmp > 0;
                case LT_OP: return cmp < 0;
                case GE_OP: return cmp >= 0;
                case LE_OP: return cmp <= 0;
                case NE_OP: return cmp != 0;
                default: return false;
            }
        }
    }

    RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data) {
        char page[PAGE_SIZE];
        unsigned short deleted = 0xFFFF;
        char tombstone = TOMBSTONE_FLAG;
        int fields = attributeNames.size();
        int nullBytes = ceil(fields / 8.0);
        while (currentPage < fileHandle.getNumberOfPages()) {
            RC code = fileHandle.readPage(currentPage, page);
            if (code != 0) {
                return code;
            }
            unsigned short pageSlots = 0;
            unsigned short freeSpaceOffset = 0;
            char *metaptr = (char*) page + PAGE_SIZE - PAGE_METADATA; 
            memcpy(&freeSpaceOffset, metaptr, 2);
            metaptr += PAGE_METADATA - 2;
            memcpy(&pageSlots, metaptr, 2);

            while (currentSlot < pageSlots) {
                char *slotptr = (char*) page + PAGE_SIZE - PAGE_METADATA - (currentSlot + 1) * SLOT_SIZE;
                unsigned short dataOffset = 0;
                unsigned short dataLen = 0;

                memcpy(&dataOffset, slotptr, 2);

                if (dataOffset == deleted) {
                    currentSlot++;
                    continue;
                }
                if (page[dataOffset] == TOMBSTONE_FLAG) {
                    currentSlot++;
                    continue;
                }
                slotptr += SLOT_SIZE - 2;
                memcpy(&dataLen, slotptr, 2);

                bool pass = false;
                int conditionIndex = -1;
                // case 1: no op means we just return the next available record
                if (compOp == NO_OP || conditionAttribute == "") {
                    pass = true;
                // else we start comparing to see if this record is good to project
                } else {
                    for (int i = 0; i < recordDescriptor.size(); i++) {
                        if (recordDescriptor[i].name == conditionAttribute) {
                            conditionIndex = i;
                            break;
                        }
                    }
                    if (conditionIndex == -1) {
                        currentSlot++;
                        continue;
                    }
                    unsigned short prevOffset;
                    if (conditionIndex == 0) {
                        prevOffset = recordDescriptor.size() * RECORD_DIR_SIZE;
                    } else {
                        int j = conditionIndex - 1;
                        while (j >= 0) {
                            memcpy(&prevOffset, (char*)page + dataOffset + j * RECORD_DIR_SIZE, RECORD_DIR_SIZE);
                            if (prevOffset != 0xFFFF) {
                                break;
                            }
                            j--;
                        }
                        if (j < 0) prevOffset = recordDescriptor.size() * RECORD_DIR_SIZE;
                    }
                    unsigned short currOffset;
                    char *dataptr = (char*) page + dataOffset + RECORD_DIR_SIZE*conditionIndex;
                    memcpy(&currOffset, dataptr, RECORD_DIR_SIZE);
                    if (currOffset == 0xFFFF) {
                        pass = false;
                    }
                    else {
                        char *fieldptr = (char*)page + dataOffset + prevOffset;
                        if (recordDescriptor[conditionIndex].type == TypeInt) {
                            int fieldVal;
                            int condVal;
                            memcpy(&fieldVal, fieldptr, recordDescriptor[conditionIndex].length);
                            memcpy(&condVal, value, sizeof(int));
                            pass = compare(&fieldVal, &condVal, TypeInt, compOp);
                        }
                        else if (recordDescriptor[conditionIndex].type == TypeReal) {
                            float fieldVal;
                            float condVal;
                            memcpy(&fieldVal, fieldptr, recordDescriptor[conditionIndex].length);
                            memcpy(&condVal, value, sizeof(float));
                            pass = compare(&fieldVal, &condVal, TypeReal, compOp);
                        }
                        else {
                            unsigned int fieldLen = currOffset - prevOffset;
                            char tempField[fieldLen + 4];
                            memcpy(tempField, &fieldLen, 4);
                            memcpy(tempField + 4, fieldptr, fieldLen);
                            pass = compare(tempField, value, TypeVarChar, compOp);

                        }
                    }
                }
                // this record is good so we can project only the fields that we need
                if (pass) {
                    rid.pageNum = currentPage;
                    rid.slotNum = currentSlot;
                    char *bitptr = (char*) data;
                    char *outptr = (char *) data + nullBytes;
                    memset(bitptr, 0, nullBytes);
                    for (int i = 0; i < attributeNames.size(); i++) {
                        // projecting attribute need to find each of their positions then read
                        for (int j = 0; j < recordDescriptor.size(); j++) {
                            if (attributeNames[i] == recordDescriptor[j].name) {
                                unsigned short prevOffset;
                                if (j == 0) {
                                    prevOffset = recordDescriptor.size() * RECORD_DIR_SIZE;
                                } else {
                                    int k = j - 1;
                                    while (k >= 0) {
                                        memcpy(&prevOffset, (char*)page + dataOffset + k * RECORD_DIR_SIZE, RECORD_DIR_SIZE);
                                        if (prevOffset != 0xFFFF) {
                                            break;
                                        }
                                        k--;
                                    }
                                    if (k < 0) prevOffset = recordDescriptor.size() * RECORD_DIR_SIZE;
                                }
                                unsigned short currOffset;
                                char *dataptr = (char*) page + dataOffset + RECORD_DIR_SIZE*j;
                                memcpy(&currOffset, dataptr, RECORD_DIR_SIZE);
                                if (currOffset == 0xFFFF) {
                                    bitptr[i / 8] |= (0x80 >> (i % 8));
                                } else {
                                    if (recordDescriptor[j].type == TypeVarChar) {
                                        unsigned int charLen = currOffset - prevOffset;
                                        memcpy(outptr, &charLen, LENGTH_PREFIX);
                                        outptr += LENGTH_PREFIX;
                                        memcpy(outptr, page + dataOffset + prevOffset, charLen);
                                        outptr += charLen;
                                    }
                                    else {
                                        memcpy(outptr, page + dataOffset + prevOffset, recordDescriptor[j].length);
                                        outptr += recordDescriptor[j].length;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    currentSlot += 1;
                    return 0;
                }
                currentSlot += 1;
            }
            // when we're done with a page
            currentPage += 1;
            currentSlot = 0;
        }

        return RBFM_EOF;
    }

    RC RBFM_ScanIterator::close() {
        currentPage = 0;
        currentSlot = 0;
        return 0;
    }

} // namespace PeterDB


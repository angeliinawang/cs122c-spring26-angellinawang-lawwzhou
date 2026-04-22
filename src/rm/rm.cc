#include "src/include/rm.h"

namespace PeterDB {
    RelationManager &RelationManager::instance() {
        static RelationManager _relation_manager = RelationManager();
        return _relation_manager;
    }

    RelationManager::RelationManager() = default;

    RelationManager::~RelationManager() = default;

    RelationManager::RelationManager(const RelationManager &) = default;

    RelationManager &RelationManager::operator=(const RelationManager &) = default;

    RC RelationManager::createCatalog() {
        // check whether catalog exists already, also create two files
        auto &rbfm = RecordBasedFileManager::instance();
        if (rbfm.createFile("Tables") != 0) return -1;
        if (rbfm.createFile("Columns") != 0) {
            rbfm.destroyFile("Tables"); // roll back
            return -1;
        }

        // get schemas
        auto tablesSchema = getTablesSchema();
        auto columnsSchema = getColumnsSchema();

        // open Tables file
        FileHandle tableFH, colFH;
        if (rbfm.openFile("Tables", tableFH) != 0) return -1;
        if (rbfm.openFile("Columns", colFH) != 0) return -1;

        RID rid;
        char buf[PAGE_SIZE];

        // insert row describing Tables table itself
        serializeTablesRow(buf, 1, "Tables", "Tables");
        rbfm.insertRecord(tableFH, tablesSchema, buf, rid);
        // insert row describing Columns table itself
        serializeTablesRow(buf, 2, "Columns", "Columns");
        rbfm.insertRecord(tableFH, tablesSchema, buf, rid);

        // insert rows describing every column of Tables table
        for (int i = 0; i < tablesSchema.size(); i++) {
            const auto &a = tablesSchema[i];
            serializeColumnsRow(buf, 1, a.name, a.type, a.length, i+1);
            rbfm.insertRecord(colFH, columnsSchema, buf, rid);
        }
        // insert rows describing every column of Columns table
        for (int i = 0; i < columnsSchema.size(); i++) {
            const auto &a = columnsSchema[i];
            serializeColumnsRow(buf, 2, a.name, a.type, a.length, i+1);
            rbfm.insertRecord(colFH, columnsSchema, buf, rid);
        }
        
        rbfm.closeFile(tableFH);
        rbfm.closeFile(colFH);
        
        return 0;
    }

    RC RelationManager::deleteCatalog() {
        auto &rbfm = RecordBasedFileManager::instance();
        
        return (rbfm.destroyFile("Tables") != 0 || rbfm.destroyFile("Columns") != 0) ? -1 : 0;

        // also delete user table files, complete after scan() implemented
    }

    RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
        return -1;
    }

    RC RelationManager::deleteTable(const std::string &tableName) {
        return -1;
    }

    RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
        return -1;
    }

    RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
        return -1;
    }

    RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
        return -1;
    }

    RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
        return -1;
    }

    RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
        return -1;
    }

    RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data, std::ostream &out) {
        return -1;
    }

    RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                      void *data) {
        return -1;
    }

    RC RelationManager::scan(const std::string &tableName,
                             const std::string &conditionAttribute,
                             const CompOp compOp,
                             const void *value,
                             const std::vector<std::string> &attributeNames,
                             RM_ScanIterator &rm_ScanIterator) {
        return -1;
    }

    RM_ScanIterator::RM_ScanIterator() = default;

    RM_ScanIterator::~RM_ScanIterator() = default;

    RC RM_ScanIterator::getNextTuple(RID &rid, void *data) { return RM_EOF; }

    RC RM_ScanIterator::close() { return -1; }

    // Extra credit work
    RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
        return -1;
    }

    // Extra credit work
    RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
        return -1;
    }

    // QE IX related
    RC RelationManager::createIndex(const std::string &tableName, const std::string &attributeName){
        return -1;
    }

    RC RelationManager::destroyIndex(const std::string &tableName, const std::string &attributeName){
        return -1;
    }

    // indexScan returns an iterator to allow the caller to go through qualified entries in index
    RC RelationManager::indexScan(const std::string &tableName,
                 const std::string &attributeName,
                 const void *lowKey,
                 const void *highKey,
                 bool lowKeyInclusive,
                 bool highKeyInclusive,
                 RM_IndexScanIterator &rm_IndexScanIterator){
        return -1;
    }


    RM_IndexScanIterator::RM_IndexScanIterator() = default;

    RM_IndexScanIterator::~RM_IndexScanIterator() = default;

    RC RM_IndexScanIterator::getNextEntry(RID &rid, void *key){
        return -1;
    }

    RC RM_IndexScanIterator::close(){
        return -1;
    }

    // private helpers, return attribute lists of Tables/Columns
    std::vector<Attribute> RelationManager::getTablesSchema() {
        return {
            {"table-id", TypeInt, 4},
            {"table-name", TypeVarChar, 50},
            {"file-name", TypeVarChar, 50}
        };
    }

    std::vector<Attribute> RelationManager::getColumnsSchema() {
        return {
            {"table-id", TypeInt, 4},
            {"column-name", TypeVarChar, 50},
            {"column-type", TypeInt, 4},
            {"column-length", TypeInt, 4},
            {"column-position", TypeInt, 4}
        };
    }

    void RelationManager::serializeTablesRow(char* buf, int tableId, const std::string &tableName,
                                const std::string &fileName) {
        char *p = buf;
        *p = 0; p += 1;

        memcpy(p, &tableId, sizeof(int));
        p += sizeof(int);

        unsigned tableLen = tableName.size();
        memcpy(p, &tableLen, sizeof(unsigned));
        p += sizeof(unsigned);
        memcpy(p, tableName.data(), tableLen);
        p += tableLen;

        unsigned fileLen = fileName.size();
        memcpy(p, &fileLen, sizeof(unsigned));
        p += sizeof(unsigned);
        memcpy(p, fileName.data(), fileLen);
    }

    void RelationManager::serializeColumnsRow(char* buf, int tableId, const std::string &colName,
                                int colType, int colLength, int colPosition) {
        char *p = buf;
        *p = 0; p += 1;

        memcpy(p, &tableId, sizeof(int));
        p += sizeof(int); // advance pointer each time

        unsigned nameLen = colName.size();
        memcpy(p, &nameLen, sizeof(unsigned));
        p += sizeof(unsigned);
        memcpy(p, colName.data(), nameLen);
        p += nameLen;

        memcpy(p, &colType, sizeof(int));
        p += sizeof(int);
        memcpy(p, &colLength, sizeof(int));
        p += sizeof(int);
        memcpy(p, &colPosition, sizeof(int));
    }


} // namespace PeterDB
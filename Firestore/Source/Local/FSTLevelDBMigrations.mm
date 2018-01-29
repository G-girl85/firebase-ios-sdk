#include "Firestore/Source/Local/FSTLevelDBMigrations.h"

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#import "Firestore/Protos/objc/firestore/local/Target.pbobjc.h"
#import "Firestore/Source/Local/FSTLevelDBKey.h"
#import "Firestore/Source/Local/FSTLevelDBUtil.h"
#import "Firestore/Source/Util/FSTAssert.h"

using leveldb::DB;
using leveldb::Status;
using leveldb::Slice;
using leveldb::WriteOptions;

static void EnsureTargetGlobal(DB *db) {
  FSTPBTargetGlobal *targetGlobal = [FSTLevelDBUtil readTargetMetadataFromDB:db];
  if (!targetGlobal) {
    targetGlobal = [FSTPBTargetGlobal message];
    NSData *data = [targetGlobal data];
    Slice value((const char *)data.bytes, data.length);
    Status status = db->Put(WriteOptions(), [FSTLevelDBTargetGlobalKey key], value);
    if (!status.ok()) {
      FSTCFail(@"Failed to save  metadata: %s", status.ToString().c_str());
    }
  }
}

static void SaveVersion(DB *db, FSTLevelDBSchemaVersion version) {
  std::string key = [FSTLevelDBVersionKey key];
  std::string version_string = std::to_string(version);
  Status status = db->Put(WriteOptions(), key, version_string);
  if (!status.ok()) {
    FSTCFail(@"Saving schema version failed with status: %s", status.ToString().c_str());
  }
}

@implementation FSTLevelDBMigrations

+ (FSTLevelDBSchemaVersion)schemaVersionForDB:(DB *)db {
  std::string key = [FSTLevelDBVersionKey key];
  std::string version_string;
  Status status = db->Get([FSTLevelDBUtil standardReadOptions], key, &version_string);
  if (status.IsNotFound()) {
    return 0;
  } else {
    return stoi(version_string);
  }
}

+ (void)runMigrationsToVersion:(FSTLevelDBSchemaVersion)version onDB:(leveldb::DB *)db {
  FSTLevelDBSchemaVersion currentVersion = [self schemaVersionForDB:db];
  switch (currentVersion) {
    case 0:
      EnsureTargetGlobal(db);
      // Fallthrough
    default:
      if (currentVersion < version) {
        SaveVersion(db, version);
      }
  }
}

@end
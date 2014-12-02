/* Copyright (c) 2009-2014 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef RAMCLOUD_MASTERSERVICE_H
#define RAMCLOUD_MASTERSERVICE_H

#include "Common.h"
#include "CoordinatorClient.h"
#include "Log.h"
#include "LogCleaner.h"
#include "HashTable.h"
#include "MasterTableMetadata.h"
#include "Object.h"
#include "ObjectFinder.h"
#include "ObjectManager.h"
#include "ReplicaManager.h"
#include "SegmentIterator.h"
#include "SegmentManager.h"
#include "ServerConfig.h"
#include "ServerList.h"
#include "ServerStatistics.pb.h"
#include "Service.h"
#include "SideLog.h"
#include "SpinLock.h"
#include "TabletManager.h"
#include "IndexletManager.h"
#include "WireFormat.h"
#include "UnackedRpcResults.h"

namespace RAMCloud {

// forward declaration
namespace MasterServiceInternal {
class RecoveryTask;
}

/**
 * An object of this class represents a RAMCloud server, which can
 * respond to client RPC requests to manipulate objects stored on the
 * server.
 */
class MasterService : public Service {
  public:
    MasterService(Context* context, const ServerConfig* config);
    virtual ~MasterService();

    void dispatch(WireFormat::Opcode opcode, Rpc* rpc);
    int maxThreads() { return config->master.masterServiceThreadCount; }

    /*
     * The following class is used to temporarily disable the servicing of
     * incoming requests: they will be rejected with STATUS_RETRY until
     * the object is destroyed or its reenable method has been called.  These
     * objects are typically used when a server becomes uncertain that it
     * is still part of the cluster See "Zombies" in designNotes for details.
     */
    class Disabler {
      public:
        explicit Disabler(MasterService* service);
        ~Disabler();
        void reenable();
      PRIVATE:
        /// MasterService that has been disabled.  NULL means either the
        /// service has been reenabled or no service was specified in the
        /// constructor; in either case there's nothing to reenable.
        MasterService* service;
        DISALLOW_COPY_AND_ASSIGN(Disabler);
    };

    /// Shared RAMCloud information.
    Context* context;

    const ServerConfig* config;

    /**
     * The ObjectFinder class that is used to locate servers containing
     * indexlets for data that this server may own.
     */
    ObjectFinder objectFinder;

    /**
     * The ObjectManager class that is responsible for object storage.
     */
    ObjectManager objectManager;

    /**
     * The TabletManager keeps track of ranges of tables that are assigned to
     * this server by the coordinator. Ranges are contiguous spans of the 64-bit
     * key hash space.
     */
    TabletManager tabletManager;

    /**
     * The IndexletManger class that is responsible for index storage.
     */
    IndexletManager indexletManager;

    /**
     * The UnackedRpcResults keeps track of those linearizable rpcs that have
     * not yet been acknowledged by the client.
     */
    UnackedRpcResults unackedRpcResults;

    /**
     * Largest cluster time that this master service either directly or
     * indirectly received from the coordinator.
     */
    Atomic<uint64_t> clusterTime;

#ifdef TESTING
    /// Used to pause the read-increment-write cycle in incrementObject
    /// between the read and the write.  While paused, a second thread can
    /// run a full read-increment-write cycle forcing the first thread to
    /// fail on the conditional write and to retry the cycle.
    static volatile int pauseIncrement;

    /// Used by to indicate to a paused thread that it may finish the
    /// increment operation.
    static volatile int continueIncrement;
#endif

  PRIVATE:
    void dropTabletOwnership(
                const WireFormat::DropTabletOwnership::Request* reqHdr,
                WireFormat::DropTabletOwnership::Response* respHdr,
                Rpc* rpc);
    void dropIndexletOwnership(
                const WireFormat::DropIndexletOwnership::Request* reqHdr,
                WireFormat::DropIndexletOwnership::Response* respHdr,
                Rpc* rpc);
    void enumerate(const WireFormat::Enumerate::Request* reqHdr,
                WireFormat::Enumerate::Response* respHdr,
                Rpc* rpc);
    void getHeadOfLog(const WireFormat::GetHeadOfLog::Request* reqHdr,
                WireFormat::GetHeadOfLog::Response* respHdr,
                Rpc* rpc);
    void getLogMetrics(const WireFormat::GetLogMetrics::Request* reqHdr,
                WireFormat::GetLogMetrics::Response* respHdr,
                Rpc* rpc);
    void getServerStatistics(
                const WireFormat::GetServerStatistics::Request* reqHdr,
                WireFormat::GetServerStatistics::Response* respHdr,
                Rpc* rpc);
    void fillWithTestData(const WireFormat::FillWithTestData::Request* reqHdr,
                WireFormat::FillWithTestData::Response* respHdr,
                Rpc* rpc);
    void increment(const WireFormat::Increment::Request* reqHdr,
                WireFormat::Increment::Response* respHdr,
                Rpc* rpc);
    void incrementObject(Key *key,
                RejectRules rejectRules,
                int64_t *asInt64,
                double *asDouble,
                uint64_t *newVersion,
                Status *status);
    void indexedRead(
                const WireFormat::IndexedRead::Request* reqHdr,
                WireFormat::IndexedRead::Response* respHdr,
                Rpc* rpc);
    void initOnceEnlisted();
    void insertIndexEntry(const WireFormat::InsertIndexEntry::Request* reqHdr,
                WireFormat::InsertIndexEntry::Response* respHdr,
                Rpc* rpc);
    void isReplicaNeeded(const WireFormat::IsReplicaNeeded::Request* reqHdr,
                WireFormat::IsReplicaNeeded::Response* respHdr,
                Rpc* rpc);
    void lookupIndexKeys(const WireFormat::LookupIndexKeys::Request* reqHdr,
                WireFormat::LookupIndexKeys::Response* respHdr,
                Rpc* rpc);
    void migrateTablet(const WireFormat::MigrateTablet::Request* reqHdr,
                WireFormat::MigrateTablet::Response* respHdr,
                Rpc* rpc);
    void multiOp(const WireFormat::MultiOp::Request* reqHdr,
                WireFormat::MultiOp::Response* respHdr,
                Rpc* rpc);
    void multiIncrement(const WireFormat::MultiOp::Request* reqHdr,
                WireFormat::MultiOp::Response* respHdr,
                Rpc* rpc);
    void multiRead(const WireFormat::MultiOp::Request* reqHdr,
                WireFormat::MultiOp::Response* respHdr,
                Rpc* rpc);
    void multiRemove(const WireFormat::MultiOp::Request* reqHdr,
                WireFormat::MultiOp::Response* respHdr,
                Rpc* rpc);
    void multiWrite(const WireFormat::MultiOp::Request* reqHdr,
                WireFormat::MultiOp::Response* respHdr,
                Rpc* rpc);
    void prepForMigration(const WireFormat::PrepForMigration::Request* reqHdr,
                WireFormat::PrepForMigration::Response* respHdr,
                Rpc* rpc);
    void read(const WireFormat::Read::Request* reqHdr,
                WireFormat::Read::Response* respHdr,
                Rpc* rpc);
    void readKeysAndValue(const WireFormat::ReadKeysAndValue::Request* reqHdr,
                WireFormat::ReadKeysAndValue::Response* respHdr,
                Rpc* rpc);
    void receiveMigrationData(
                const WireFormat::ReceiveMigrationData::Request* reqHdr,
                WireFormat::ReceiveMigrationData::Response* respHdr,
                Rpc* rpc);
    void remove(const WireFormat::Remove::Request* reqHdr,
                WireFormat::Remove::Response* respHdr,
                Rpc* rpc);
    void removeIndexEntry(const WireFormat::RemoveIndexEntry::Request* reqHdr,
                WireFormat::RemoveIndexEntry::Response* respHdr,
                Rpc* rpc);
    void requestInsertIndexEntries(Object& object);
    void requestRemoveIndexEntries(Buffer& objectBuffer);
    void splitMasterTablet(const WireFormat::SplitMasterTablet::Request* reqHdr,
                WireFormat::SplitMasterTablet::Response* respHdr,
                Rpc* rpc);
    void takeTabletOwnership(
                const WireFormat::TakeTabletOwnership::Request* reqHdr,
                WireFormat::TakeTabletOwnership::Response* respHdr,
                Rpc* rpc);
    void takeIndexletOwnership(
                const WireFormat::TakeIndexletOwnership::Request* reqHdr,
                WireFormat::TakeIndexletOwnership::Response* respHdr,
                Rpc* rpc);
    void write(const WireFormat::Write::Request* reqHdr,
                WireFormat::Write::Response* respHdr,
                Rpc* rpc);

    /**
     * Counts the number of times disable has been called, minus the number
     * of times enable has been called; a value > 0 means that the service
     * is disabled and should return STATUS_RETRY for all requests. This
     * can happen, for example, if the server is no longer certain that it
     * is a valid member of the cluster (see "Zombies" in designNotes).
     */
    Atomic<int> disableCount;

    /**
     * Used to ensure that init() is invoked before the dispatcher runs.
     */
    bool initCalled;

    /**
     * Used by takeTabletOwnership to avoid sync-ing the log except for the
     * first tablet accepted.
     */
    bool logEverSynced;

    /**
     * The MasterTableMetadata object keeps per table metadata.
     */
    MasterTableMetadata masterTableMetadata;

    /**
     * Determines the maximum size of the response buffer for
     * operations. Normally MAX_RPC_LEN, but can be modified during tests
     * to simplify testing.
     */
    uint32_t maxResponseRpcLen;

///////////////////////////////////////////////////////////////////////////////
/////Recovery related code. This should eventually move into its own file./////
///////////////////////////////////////////////////////////////////////////////

    /**
     * Represents a known segment replica during recovery and the state
     * of fetching it from backups.
     */
    struct Replica {
        enum class State {
            NOT_STARTED = 0,
            WAITING,
            FAILED,
            OK,
        };
        Replica(uint64_t backupId, uint64_t segmentId,
                State state = State::NOT_STARTED);

        /**
         * The backup containing the replica.
         */
        ServerId backupId;

        /**
         * The segment ID for this replica.
         */
        uint64_t segmentId;

        /**
         * Used in recovery routines to keep track of the status of requesting
         * the data from this replica.
         */
        State state;
    };

    static void detectSegmentRecoveryFailure(
                const ServerId masterId,
                const uint64_t partitionId,
                const vector<MasterService::Replica>& replicas);
    void recover(const WireFormat::Recover::Request* reqHdr,
                WireFormat::Recover::Response* respHdr,
                Rpc* rpc);
    void recover(uint64_t recoveryId,
                ServerId masterId,
                uint64_t partitionId,
                vector<Replica>& replicas,
                std::unordered_map<uint64_t, uint64_t>& highestBTreeIdMap);

///////////////////////////////////////////////////////////////////////////////
/////////////////////////End of Recovery related code./////////////////////////
///////////////////////////////////////////////////////////////////////////////

    friend void recoveryCleanup(uint64_t maybeTomb, void *cookie);
    friend void removeObjectIfFromUnknownTablet(uint64_t reference,
                void *cookie);
    friend class RecoverSegmentBenchmark;
    friend class MasterServiceInternal::RecoveryTask;

    DISALLOW_COPY_AND_ASSIGN(MasterService);
};

} // namespace RAMCloud

#endif // RAMCLOUD_MASTERSERVICE_H

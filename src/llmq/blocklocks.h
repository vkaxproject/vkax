// Copyright (c) 2019-2022 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_BLOCKLOCKS_H
#define BITCOIN_LLMQ_BLOCKLOCKS_H

#include <llmq/blsig.h>

#include <crypto/common.h>
#include <llmq/signing.h>
#include <net.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <saltedhasher.h>
#include <streams.h>
#include <sync.h>

#include <atomic>
#include <unordered_set>

class CBlockIndex;
class CScheduler;

namespace llmq
{

class CBlockLocksHandler : public CRecoveredSigsListener
{
    static constexpr int64_t CLEANUP_INTERVAL = 1000 * 30;
    static constexpr int64_t CLEANUP_SEEN_TIMEOUT = 24 * 60 * 60 * 1000;

    // how long to wait for islocks until we consider a block with non-islocked TXs to be safe to sign
    static constexpr int64_t WAIT_FOR_ISLOCK_TIMEOUT = 10 * 60;

private:
    std::unique_ptr<CScheduler> scheduler;
    std::unique_ptr<std::thread> scheduler_thread;
    mutable CCriticalSection cs;
    std::atomic<bool> tryLockBlockTipScheduled{false};
    std::atomic<bool> isEnabled{false};
    std::atomic<bool> isEnforced{false};

    uint256 bestBlockLockHash GUARDED_BY(cs);
    CBlockLockSig bestBlockLock GUARDED_BY(cs);

    CBlockLockSig bestBlockLockWithKnownBlock GUARDED_BY(cs);
    const CBlockIndex* bestBlockLockBlockIndex GUARDED_BY(cs) {nullptr};
    const CBlockIndex* lastNotifyBlockLockBlockIndex GUARDED_BY(cs) {nullptr};

    int32_t lastSignedHeight GUARDED_BY(cs) {-1};
    uint256 lastSignedRequestId GUARDED_BY(cs);
    uint256 lastSignedMsgHash GUARDED_BY(cs);

    // We keep track of txids from recently received blocks so that we can check if all TXs got islocked
    struct BlockHasher
    {
        size_t operator()(const uint256& hash) const { return ReadLE64(hash.begin()); }
    };
    using BlockTxs = std::unordered_map<uint256, std::shared_ptr<std::unordered_set<uint256, StaticSaltedHasher>>, BlockHasher>;
    BlockTxs blockTxs GUARDED_BY(cs);
    std::unordered_map<uint256, int64_t, StaticSaltedHasher> txFirstSeenTime GUARDED_BY(cs);

    std::map<uint256, int64_t> seenBlockLocks GUARDED_BY(cs);

    int64_t lastCleanupTime GUARDED_BY(cs) {0};

public:
    explicit CBlockLocksHandler();
    ~CBlockLocksHandler();

    void Start();
    void Stop();

    bool AlreadyHave(const CInv& inv) const;
    bool GetBlockLockByHash(const uint256& hash, CBlockLockSig& ret) const;
    CBlockLockSig GetBestBlockLock() const;

    void ProcessMessage(CNode* pfrom, const std::string& msg_type, CDataStream& vRecv);
    void ProcessNewBlockLock(NodeId from, const CBlockLockSig& blsig, const uint256& hash);
    void AcceptedBlockHeader(const CBlockIndex* pindexNew);
    void UpdatedBlockTip();
    void TransactionAddedToMempool(const CTransactionRef& tx, int64_t nAcceptTime);
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindex, const std::vector<CTransactionRef>& vtxConflicted);
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexDisconnected);
    void CheckActiveState();
    void TrySignChainTip();
    void EnforceBestBlockLock();
    void HandleNewRecoveredSig(const CRecoveredSig& recoveredSig) override;

    bool HasBlockLock(int nHeight, const uint256& blockHash) const;
    bool HasConflictingBlockLock(int nHeight, const uint256& blockHash) const;

    bool IsTxSafeForMining(const uint256& txid) const;

private:
    // these require locks to be held already
    bool InternalHasBlockLock(int nHeight, const uint256& blockHash) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    bool InternalHasConflictingBlockLock(int nHeight, const uint256& blockHash) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    BlockTxs::mapped_type GetBlockTxs(const uint256& blockHash);

    void Cleanup();
};

extern CBlockLocksHandler* blockLocksHandler;

bool AreBlockLocksEnabled();

} // namespace llmq

#endif // BITCOIN_LLMQ_BLOCKLOCKS_H

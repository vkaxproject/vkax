// Copyright (c) 2019-2022 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/blocklocks.h>
#include <llmq/quorums.h>
#include <llmq/instantsend.h>
#include <llmq/utils.h>

#include <chain.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <masternode/sync.h>
#include <net_processing.h>
#include <scheduler.h>
#include <spork.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util/validation.h>

namespace llmq
{
CBlockLocksHandler* blockLocksHandler;

CBlockLocksHandler::CBlockLocksHandler() :
    scheduler(std::make_unique<CScheduler>())
{
    CScheduler::Function serviceLoop = std::bind(&CScheduler::serviceQueue, scheduler.get());
    scheduler_thread = std::make_unique<std::thread>(std::bind(&TraceThread<CScheduler::Function>, "cl-schdlr", serviceLoop));
}

CBlockLocksHandler::~CBlockLocksHandler()
{
    scheduler->stop();
    scheduler_thread->join();
}

void CBlockLocksHandler::Start()
{
    quorumSigningManager->RegisterRecoveredSigsListener(this);
    scheduler->scheduleEvery([&]() {
        CheckActiveState();
        EnforceBestBlockLock();
        // regularly retry signing the current chaintip as it might have failed before due to missing islocks
        TrySignChainTip();
    }, 5000);
}

void CBlockLocksHandler::Stop()
{
    scheduler->stop();
    quorumSigningManager->UnregisterRecoveredSigsListener(this);
}

bool CBlockLocksHandler::AlreadyHave(const CInv& inv) const
{
    LOCK(cs);
    return seenBlockLocks.count(inv.hash) != 0;
}

bool CBlockLocksHandler::GetBlockLockByHash(const uint256& hash, llmq::CBlockLockSig& ret) const
{
    LOCK(cs);

    if (hash != bestBlockLockHash) {
        // we only propagate the best one and ditch all the old ones
        return false;
    }

    ret = bestBlockLock;
    return true;
}

CBlockLockSig CBlockLocksHandler::GetBestBlockLock() const
{
    LOCK(cs);
    return bestBlockLock;
}

void CBlockLocksHandler::ProcessMessage(CNode* pfrom, const std::string& msg_type, CDataStream& vRecv)
{
    if (!AreBlockLocksEnabled()) {
        return;
    }

    if (msg_type == NetMsgType::BLSIG) {
        CBlockLockSig blsig;
        vRecv >> blsig;

        ProcessNewBlockLock(pfrom->GetId(), blsig, ::SerializeHash(blsig));
    }
}

void CBlockLocksHandler::ProcessNewBlockLock(const NodeId from, const llmq::CBlockLockSig& blsig, const uint256& hash)
{
    CheckActiveState();

    CInv blsigInv(MSG_BLSIG, hash);

    if (from != -1) {
        LOCK(cs_main);
        EraseObjectRequest(from, blsigInv);
    }

    {
        LOCK(cs);
        if (!seenBlockLocks.emplace(hash, GetTimeMillis()).second) {
            return;
        }

        if (!bestBlockLock.IsNull() && blsig.nHeight <= bestBlockLock.nHeight) {
            // no need to process/relay older BLSIGs
            return;
        }
    }

    const uint256 requestId = ::SerializeHash(std::make_pair(BLSIG_REQUESTID_PREFIX, blsig.nHeight));
    if (!llmq::CSigningManager::VerifyRecoveredSig(Params().GetConsensus().llmqTypeBlockLocks, blsig.nHeight, requestId, blsig.blockHash, blsig.sig)) {
        LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- invalid BLSIG (%s), peer=%d\n", __func__, blsig.ToString(), from);
        if (from != -1) {
            LOCK(cs_main);
            Misbehaving(from, 10);
        }
        return;
    }

    CBlockIndex* pindex = WITH_LOCK(cs_main, return LookupBlockIndex(blsig.blockHash));

    {
        LOCK(cs);
        bestBlockLockHash = hash;
        bestBlockLock = blsig;

        if (pindex != nullptr) {

            if (pindex->nHeight != blsig.nHeight) {
                // Should not happen, same as the conflict check from above.
                LogPrintf("CBlockLocksHandler::%s -- height of BLSIG (%s) does not match the specified block's height (%d)\n",
                        __func__, blsig.ToString(), pindex->nHeight);
                // Note: not relaying blsig here
                return;
            }

            bestBlockLockWithKnownBlock = bestBlockLock;
            bestBlockLockBlockIndex = pindex;
        }
        // else if (pindex == nullptr)
        // Note: make sure to still relay blsig further.
    }

    // Note: do not hold cs while calling RelayInv
    AssertLockNotHeld(cs);
    g_connman->RelayInv(blsigInv);

    if (pindex == nullptr) {
        // we don't know the block/header for this BLSIG yet, so bail out for now
        // when the block or the header later comes in, we will enforce the correct chain
        return;
    }

    scheduler->scheduleFromNow([&]() {
        CheckActiveState();
        EnforceBestBlockLock();
    }, 0);

    LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- processed new BLSIG (%s), peer=%d\n",
              __func__, blsig.ToString(), from);
}

void CBlockLocksHandler::AcceptedBlockHeader(const CBlockIndex* pindexNew)
{
    LOCK(cs);

    if (pindexNew->GetBlockHash() == bestBlockLock.blockHash) {
        LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- block header %s came in late, updating and enforcing\n", __func__, pindexNew->GetBlockHash().ToString());

        if (bestBlockLock.nHeight != pindexNew->nHeight) {
            // Should not happen, same as the conflict check from ProcessNewBlockLock.
            LogPrintf("CBlockLocksHandler::%s -- height of BLSIG (%s) does not match the specified block's height (%d)\n",
                      __func__, bestBlockLock.ToString(), pindexNew->nHeight);
            return;
        }

        // when EnforceBestBlockLock is called later, it might end up invalidating other chains but not activating the
        // BLSIG locked chain. This happens when only the header is known but the block is still missing yet. The usual
        // block processing logic will handle this when the block arrives
        bestBlockLockWithKnownBlock = bestBlockLock;
        bestBlockLockBlockIndex = pindexNew;
    }
}

void CBlockLocksHandler::UpdatedBlockTip()
{
    // don't call TrySignChainTip directly but instead let the scheduler call it. This way we ensure that cs_main is
    // never locked and TrySignChainTip is not called twice in parallel. Also avoids recursive calls due to
    // EnforceBestBlockLock switching chains.
    // atomic[If tryLockChainTipScheduled is false, do (set it to true] and schedule signing).
    if (bool expected = false; tryLockBlockTipScheduled.compare_exchange_strong(expected, true)) {
        scheduler->scheduleFromNow([&]() {
            CheckActiveState();
            EnforceBestBlockLock();
            TrySignChainTip();
            tryLockBlockTipScheduled = false;
        }, 0);
    }
}

void CBlockLocksHandler::CheckActiveState()
{
    bool nBLActive;
    {
        LOCK(cs_main);
        nBLActive = ::ChainActive().Tip() && ::ChainActive().Tip()->pprev && ::ChainActive().Tip()->pprev->nHeight >= Params().GetConsensus().nBLHeight;
    }

    bool oldIsEnforced = isEnforced;
    isEnabled = AreBlockLocksEnabled();
    isEnforced = (nBLActive && isEnabled);

    if (!oldIsEnforced && isEnforced) {
        // BlockLocks got activated just recently, but it's possible that it was already running before, leaving
        // us with some stale values which we should not try to enforce anymore (there probably was a good reason
        // to disable spork19)
        LOCK(cs);
        bestBlockLockHash = uint256();
        bestBlockLock = bestBlockLockWithKnownBlock = CBlockLockSig();
        bestBlockLockBlockIndex = lastNotifyBlockLockBlockIndex = nullptr;
    }
}

void CBlockLocksHandler::TrySignChainTip()
{
    Cleanup();

    if (!fMasternodeMode) {
        return;
    }

    if (!masternodeSync.IsBlockchainSynced()) {
        return;
    }

    if (!isEnabled) {
        return;
    }

    const CBlockIndex* pindex;
    {
        LOCK(cs_main);
        pindex = ::ChainActive().Tip();
    }

    if (!pindex->pprev) {
        return;
    }

    // DIP8 defines a process called "Signing attempts" which should run before the BLSIG is finalized
    // To simplify the initial implementation, we skip this process and directly try to create a BLSIG
    // This will fail when multiple blocks compete, but we accept this for the initial implementation.
    // Later, we'll add the multiple attempts process.

    {
        LOCK(cs);

        if (pindex->nHeight == lastSignedHeight) {
            // already signed this one
            return;
        }

        if (bestBlockLock.nHeight >= pindex->nHeight) {
            // already got the same BLSIG or a better one
            return;
        }

        if (InternalHasConflictingBlockLock(pindex->nHeight, pindex->GetBlockHash())) {
            // don't sign if another conflicting BLSIG is already present. EnforceBestBlockLock will later enforce
            // the correct chain.
            return;
        }
    }

    LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- trying to sign %s, height=%d\n", __func__, pindex->GetBlockHash().ToString(), pindex->nHeight);

    // When the new IX system is activated, we only try to BlockLock blocks which include safe transactions. A TX is
    // considered safe when it is islocked or at least known since 10 minutes (from mempool or block). These checks are
    // performed for the tip (which we try to sign) and the previous 5 blocks. If a BlockLocked block is found on the
    // way down, we consider all TXs to be safe.
    if (IsInstantSendEnabled() && RejectConflictingBlocks()) {
        auto pindexWalk = pindex;
        while (pindexWalk) {
            if (pindex->nHeight - pindexWalk->nHeight > 5) {
                // no need to check further down, 6 confs is safe to assume that TXs below this height won't be
                // islocked anymore if they aren't already
                LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- tip and previous 5 blocks all safe\n", __func__);
                break;
            }
            if (HasBlockLock(pindexWalk->nHeight, pindexWalk->GetBlockHash())) {
                // we don't care about islocks for TXs that are BlockLocked already
                LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- blocklock at height %d\n", __func__, pindexWalk->nHeight);
                break;
            }

            auto txids = GetBlockTxs(pindexWalk->GetBlockHash());
            if (!txids) {
                pindexWalk = pindexWalk->pprev;
                continue;
            }

            for (auto& txid : *txids) {
                int64_t txAge = 0;
                {
                    LOCK(cs);
                    auto it = txFirstSeenTime.find(txid);
                    if (it != txFirstSeenTime.end()) {
                        txAge = GetAdjustedTime() - it->second;
                    }
                }

                if (txAge < WAIT_FOR_ISLOCK_TIMEOUT && !quorumInstantSendManager->IsLocked(txid)) {
                    LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- not signing block %s due to TX %s not being islocked and not old enough. age=%d\n", __func__,
                              pindexWalk->GetBlockHash().ToString(), txid.ToString(), txAge);
                    return;
                }
            }

            pindexWalk = pindexWalk->pprev;
        }
    }

    uint256 requestId = ::SerializeHash(std::make_pair(BLSIG_REQUESTID_PREFIX, pindex->nHeight));
    uint256 msgHash = pindex->GetBlockHash();

    {
        LOCK(cs);
        if (bestBlockLock.nHeight >= pindex->nHeight) {
            // might have happened while we didn't hold cs
            return;
        }
        lastSignedHeight = pindex->nHeight;
        lastSignedRequestId = requestId;
        lastSignedMsgHash = msgHash;
    }

    quorumSigningManager->AsyncSignIfMember(Params().GetConsensus().llmqTypeBlockLocks, requestId, msgHash);
}

void CBlockLocksHandler::TransactionAddedToMempool(const CTransactionRef& tx, int64_t nAcceptTime)
{
    if (tx->IsCoinBase() || tx->vin.empty()) {
        return;
    }

    LOCK(cs);
    txFirstSeenTime.emplace(tx->GetHash(), nAcceptTime);
}

void CBlockLocksHandler::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindex, const std::vector<CTransactionRef>& vtxConflicted)
{
    if (!masternodeSync.IsBlockchainSynced()) {
        return;
    }

    // We listen for BlockConnected so that we can collect all TX ids of all included TXs of newly received blocks
    // We need this information later when we try to sign a new tip, so that we can determine if all included TXs are
    // safe.

    LOCK(cs);

    auto it = blockTxs.find(pindex->GetBlockHash());
    if (it == blockTxs.end()) {
        // we must create this entry even if there are no lockable transactions in the block, so that TrySignChainTip
        // later knows about this block
        it = blockTxs.emplace(pindex->GetBlockHash(), std::make_shared<std::unordered_set<uint256, StaticSaltedHasher>>()).first;
    }
    auto& txids = *it->second;

    int64_t curTime = GetAdjustedTime();

    for (const auto& tx : pblock->vtx) {
        if (tx->IsCoinBase() || tx->vin.empty()) {
            continue;
        }

        txids.emplace(tx->GetHash());
        txFirstSeenTime.emplace(tx->GetHash(), curTime);
    }

}

void CBlockLocksHandler::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexDisconnected)
{
    LOCK(cs);
    blockTxs.erase(pindexDisconnected->GetBlockHash());
}

CBlockLocksHandler::BlockTxs::mapped_type CBlockLocksHandler::GetBlockTxs(const uint256& blockHash)
{
    AssertLockNotHeld(cs);
    AssertLockNotHeld(cs_main);

    CBlockLocksHandler::BlockTxs::mapped_type ret;

    {
        LOCK(cs);
        auto it = blockTxs.find(blockHash);
        if (it != blockTxs.end()) {
            ret = it->second;
        }
    }
    if (!ret) {
        // This should only happen when freshly started.
        // If running for some time, SyncTransaction should have been called before which fills blockTxs.
        LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- blockTxs for %s not found. Trying ReadBlockFromDisk\n", __func__,
                 blockHash.ToString());

        uint32_t blockTime;
        {
            LOCK(cs_main);
            auto pindex = LookupBlockIndex(blockHash);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                return nullptr;
            }

            ret = std::make_shared<std::unordered_set<uint256, StaticSaltedHasher>>();
            for (auto& tx : block.vtx) {
                if (tx->IsCoinBase() || tx->vin.empty()) {
                    continue;
                }
                ret->emplace(tx->GetHash());
            }

            blockTime = block.nTime;
        }

        LOCK(cs);
        blockTxs.emplace(blockHash, ret);
        for (auto& txid : *ret) {
            txFirstSeenTime.emplace(txid, blockTime);
        }
    }
    return ret;
}

bool CBlockLocksHandler::IsTxSafeForMining(const uint256& txid) const
{
    if (!RejectConflictingBlocks()) {
        return true;
    }
    if (!isEnabled || !isEnforced) {
        return true;
    }

    if (!IsInstantSendEnabled()) {
        return true;
    }
    if (quorumInstantSendManager->IsLocked(txid)) {
        return true;
    }

    int64_t txAge = 0;
    {
        LOCK(cs);
        auto it = txFirstSeenTime.find(txid);
        if (it != txFirstSeenTime.end()) {
            txAge = GetAdjustedTime() - it->second;
        }
    }

    return txAge >= WAIT_FOR_ISLOCK_TIMEOUT;
}

// WARNING: cs_main and cs should not be held!
// This should also not be called from validation signals, as this might result in recursive calls
void CBlockLocksHandler::EnforceBestBlockLock()
{
    AssertLockNotHeld(cs);
    AssertLockNotHeld(cs_main);

    std::shared_ptr<CBlockLockSig> blsig;
    const CBlockIndex* pindex;
    const CBlockIndex* currentBestBlockLockBlockIndex;
    {
        LOCK(cs);

        if (!isEnforced) {
            return;
        }

        blsig = std::make_shared<CBlockLockSig>(bestBlockLockWithKnownBlock);
        pindex = currentBestBlockLockBlockIndex = this->bestBlockLockBlockIndex;

        if (!currentBestBlockLockBlockIndex) {
            // we don't have the header/block, so we can't do anything right now
            return;
        }
    }

    CValidationState state;
    const auto &params = Params();

    // Go backwards through the chain referenced by blsig until we find a block that is part of the main chain.
    // For each of these blocks, check if there are children that are NOT part of the chain referenced by blsig
    // and mark all of them as conflicting.
    LogPrint(BCLog::BLOCKLOCKS, "CBlockLocksHandler::%s -- enforcing block %s via BLSIG (%s)\n", __func__, pindex->GetBlockHash().ToString(), blsig->ToString());
    EnforceBlock(state, params, pindex);

    bool activateNeeded = WITH_LOCK(::cs_main, return ::ChainActive().Tip()->GetAncestor(currentBestBlockLockBlockIndex->nHeight)) != currentBestBlockLockBlockIndex;

    if (activateNeeded) {
        if(!ActivateBestChain(state, params)) {
            LogPrintf("CBlockLocksHandler::%s -- ActivateBestChain failed: %s\n", __func__, FormatStateMessage(state));
            return;
        }
        LOCK(cs_main);
        if (::ChainActive().Tip()->GetAncestor(currentBestBlockLockBlockIndex->nHeight) != currentBestBlockLockBlockIndex) {
            return;
        }
    }

    {
        LOCK(cs);
        if (lastNotifyBlockLockBlockIndex == currentBestBlockLockBlockIndex) return;
        lastNotifyBlockLockBlockIndex = currentBestBlockLockBlockIndex;
    }

    GetMainSignals().NotifyBlockLock(currentBestBlockLockBlockIndex, blsig);
    uiInterface.NotifyBlockLock(blsig->blockHash.ToString(), blsig->nHeight);
}

void CBlockLocksHandler::HandleNewRecoveredSig(const llmq::CRecoveredSig& recoveredSig)
{
    if (!isEnabled) {
        return;
    }

    CBlockLockSig blsig;
    {
        LOCK(cs);

        if (recoveredSig.id != lastSignedRequestId || recoveredSig.msgHash != lastSignedMsgHash) {
            // this is not what we signed, so lets not create a BLSIG for it
            return;
        }
        if (bestBlockLock.nHeight >= lastSignedHeight) {
            // already got the same or a better BLSIG through the BLSIG message
            return;
        }

        blsig.nHeight = lastSignedHeight;
        blsig.blockHash = lastSignedMsgHash;
        blsig.sig = recoveredSig.sig.Get();
    }
    ProcessNewBlockLock(-1, blsig, ::SerializeHash(blsig));
}

bool CBlockLocksHandler::HasBlockLock(int nHeight, const uint256& blockHash) const
{
    LOCK(cs);
    return InternalHasBlockLock(nHeight, blockHash);
}

bool CBlockLocksHandler::InternalHasBlockLock(int nHeight, const uint256& blockHash) const
{
    AssertLockHeld(cs);

    if (!isEnforced) {
        return false;
    }

    if (!bestBlockLockBlockIndex) {
        return false;
    }

    if (nHeight > bestBlockLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestBlockLockBlockIndex->nHeight) {
        return blockHash == bestBlockLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestBlockLockBlockIndex->GetAncestor(nHeight);
    return pAncestor && pAncestor->GetBlockHash() == blockHash;
}

bool CBlockLocksHandler::HasConflictingBlockLock(int nHeight, const uint256& blockHash) const
{
    LOCK(cs);
    return InternalHasConflictingBlockLock(nHeight, blockHash);
}

bool CBlockLocksHandler::InternalHasConflictingBlockLock(int nHeight, const uint256& blockHash) const
{
    AssertLockHeld(cs);

    if (!isEnforced) {
        return false;
    }

    if (!bestBlockLockBlockIndex) {
        return false;
    }

    if (nHeight > bestBlockLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestBlockLockBlockIndex->nHeight) {
        return blockHash != bestBlockLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestBlockLockBlockIndex->GetAncestor(nHeight);
    assert(pAncestor);
    return pAncestor->GetBlockHash() != blockHash;
}

void CBlockLocksHandler::Cleanup()
{
    if (!masternodeSync.IsBlockchainSynced()) {
        return;
    }

    {
        LOCK(cs);
        if (GetTimeMillis() - lastCleanupTime < CLEANUP_INTERVAL) {
            return;
        }
    }

    // need mempool.cs due to GetTransaction calls
    LOCK2(cs_main, mempool.cs);
    LOCK(cs);

    for (auto it = seenBlockLocks.begin(); it != seenBlockLocks.end(); ) {
        if (GetTimeMillis() - it->second >= CLEANUP_SEEN_TIMEOUT) {
            it = seenBlockLocks.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = blockTxs.begin(); it != blockTxs.end(); ) {
        auto pindex = LookupBlockIndex(it->first);
        if (InternalHasBlockLock(pindex->nHeight, pindex->GetBlockHash())) {
            for (auto& txid : *it->second) {
                txFirstSeenTime.erase(txid);
            }
            it = blockTxs.erase(it);
        } else if (InternalHasConflictingBlockLock(pindex->nHeight, pindex->GetBlockHash())) {
            it = blockTxs.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = txFirstSeenTime.begin(); it != txFirstSeenTime.end(); ) {
        CTransactionRef tx;
        uint256 hashBlock;
        if (!GetTransaction(it->first, tx, Params().GetConsensus(), hashBlock)) {
            // tx has vanished, probably due to conflicts
            it = txFirstSeenTime.erase(it);
        } else if (!hashBlock.IsNull()) {
            auto pindex = LookupBlockIndex(hashBlock);
            if (::ChainActive().Tip()->GetAncestor(pindex->nHeight) == pindex && ::ChainActive().Height() - pindex->nHeight >= 6) {
                // tx got confirmed >= 6 times, so we can stop keeping track of it
                it = txFirstSeenTime.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    lastCleanupTime = GetTimeMillis();
}

bool AreBlockLocksEnabled()
{
    return sporkManager.IsSporkActive(SPORK_25_BLOCKLOCKS_ENABLED);
}

} // namespace llmq

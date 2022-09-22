// Copyright (c) 2018-2022 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/init.h>

#include <llmq/quorums.h>
#include <llmq/blockprocessor.h>
#include <llmq/commitment.h>
#include <llmq/chainlocks.h>
#include <llmq/debug.h>
#include <llmq/dkgsessionmgr.h>
#include <llmq/instantsend.h>
#include <llmq/signing.h>
#include <llmq/signing_shares.h>
#include <llmq/utils.h>
#include <consensus/validation.h>

#include <dbwrapper.h>

namespace llmq
{

CBLSWorker* blsWorker;

void InitLLMQSystem(CEvoDB& evoDb, bool unitTests, bool fWipe)
{
    blsWorker = new CBLSWorker();

    quorumDKGDebugManager = std::make_unique<CDKGDebugManager>();
    quorumBlockProcessor = std::make_unique<CQuorumBlockProcessor>(evoDb, connman);
    quorumDKGSessionManager = std::make_unique<CDKGSessionManager>(connman, *blsWorker, *quorumDKGDebugManager, *quorumBlockProcessor, sporkManager, unitTests, fWipe);
    quorumManager = std::make_unique<CQuorumManager>(evoDb, connman, *blsWorker, *quorumBlockProcessor, *quorumDKGSessionManager);
    quorumSigningManager = std::make_unique<CSigningManager>(connman, *quorumManager, unitTests, fWipe);
    quorumSigSharesManager = std::make_unique<CSigSharesManager>(connman, *quorumManager, *quorumSigningManager);
    chainLocksHandler = std::make_unique<CChainLocksHandler>(mempool, connman, sporkManager, *quorumSigningManager, *quorumSigSharesManager);
    quorumInstantSendManager = std::make_unique<CInstantSendManager>(mempool, connman, sporkManager, *quorumManager, *quorumSigningManager, *quorumSigSharesManager, *chainLocksHandler, unitTests, fWipe);

    // NOTE: we use this only to wipe the old db, do NOT use it for anything else
    // TODO: remove it in some future version
    auto llmqDbTmp = std::make_unique<CDBWrapper>(unitTests ? "" : (GetDataDir() / "llmq"), 1 << 20, unitTests, true);
}

void DestroyLLMQSystem()
{
    quorumInstantSendManager.reset();
    chainLocksHandler.reset();
    quorumSigSharesManager.reset();
    quorumSigningManager.reset();
    quorumManager.reset();
    quorumDKGSessionManager.reset();
    quorumBlockProcessor.reset();
    quorumDKGDebugManager.reset();
    blsWorker.reset();
    LOCK(cs_llmq_vbc);
    llmq_versionbitscache.Clear();
}

void StartLLMQSystem()
{
    if (blsWorker) {
        blsWorker->Start();
    }
    if (quorumDKGSessionManager) {
        quorumDKGSessionManager->StartThreads();
    }
    if (quorumManager) {
        quorumManager->Start();
    }
    if (quorumSigSharesManager) {
        quorumSigSharesManager->RegisterAsRecoveredSigsListener();
        quorumSigSharesManager->StartWorkerThread();
    }
    if (chainLocksHandler) {
        chainLocksHandler->Start();
    }
    if (quorumInstantSendManager) {
        quorumInstantSendManager->Start();
    }
}

void StopLLMQSystem()
{
    if (quorumInstantSendManager) {
        quorumInstantSendManager->Stop();
    }
    if (chainLocksHandler) {
        chainLocksHandler->Stop();
    }
    if (quorumSigSharesManager) {
        quorumSigSharesManager->StopWorkerThread();
        quorumSigSharesManager->UnregisterAsRecoveredSigsListener();
    }
    if (quorumManager) {
        quorumManager->Stop();
    }
    if (quorumDKGSessionManager) {
        quorumDKGSessionManager->StopThreads();
    }
    if (blsWorker) {
        blsWorker->Stop();
    }
}

void InterruptLLMQSystem()
{
    if (quorumSigSharesManager) {
        quorumSigSharesManager->InterruptWorkerThread();
    }
    if (quorumInstantSendManager) {
        quorumInstantSendManager->InterruptWorkerThread();
    }
}

} // namespace llmq

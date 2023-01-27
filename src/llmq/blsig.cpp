// Copyright (c) 2021-2022 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/blsig.h>
#include <tinyformat.h>

namespace llmq {
    const std::string BLSIG_REQUESTID_PREFIX = "blsig";

    bool CBlockLockSig::IsNull() const {
        return nHeight == -1 && blockHash == uint256();
    }

    std::string CBlockLockSig::ToString() const {
        return strprintf("CBlockLockSig(nHeight=%d, blockHash=%s)", nHeight, blockHash.ToString());
    }
}

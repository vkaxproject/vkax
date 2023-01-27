// Copyright (c) 2019-2022 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_BLSIG_H
#define BITCOIN_LLMQ_BLSIG_H

#include <bls/bls.h>
#include <serialize.h>
#include <uint256.h>

namespace llmq
{

    extern const std::string BLSIG_REQUESTID_PREFIX;

    class CBlockLockSig
    {
    public:
        int32_t nHeight{-1};
        uint256 blockHash;
        CBLSSignature sig;

    public:
        SERIALIZE_METHODS(CBlockLockSig, obj)
        {
            READWRITE(obj.nHeight, obj.blockHash, obj.sig);
        }

        bool IsNull() const;
        std::string ToString() const;
    };
} // namespace llmq

#endif // BITCOIN_LLMQ_BLSIG_H

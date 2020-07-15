#ifndef TECRACOIN_EXODUS_TXPROCESSOR_H
#define TECRACOIN_EXODUS_TXPROCESSOR_H

#include "property.h"
#include "sigmaprimitives.h"
#include "tx.h"

#include <boost/signals2/signal.hpp>

namespace exodus {

class TxProcessor
{
public:
    int ProcessTx(CMPTransaction& tx);

public:
    boost::signals2::signal<void(PropertyId, SigmaDenomination, SigmaMintGroup, SigmaMintIndex, const SigmaPublicKey&)> SimpleMintProcessed;
    boost::signals2::signal<void(const CMPTransaction&)> TransactionProcessed;

private:
    int ProcessSimpleMint(const CMPTransaction& tx);
    int ProcessSimpleSpend(const CMPTransaction& tx);
};

extern TxProcessor *txProcessor;

}

#endif // TECRACOIN_EXODUS_TXPROCESSOR_H

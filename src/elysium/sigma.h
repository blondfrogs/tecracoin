#ifndef TECRACOIN_ELYSIUM_SIGMA_H
#define TECRACOIN_ELYSIUM_SIGMA_H

#include "property.h"
#include "sigmaprimitives.h"

#include <stddef.h>

namespace elysium {

bool VerifySigmaSpend(
    PropertyId property,
    SigmaDenomination denomination,
    SigmaMintGroup group,
    size_t groupSize,
    const SigmaProof& proof,
    const secp_primitives::Scalar& serial,
    bool fPadding);

} // namespace elysium

#endif // TECRACOIN_ELYSIUM_SIGMA_H

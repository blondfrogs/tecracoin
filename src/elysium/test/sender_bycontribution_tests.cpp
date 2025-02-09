#include "elysium/test/utils_tx.h"

#include "elysium/elysium.h"
#include "elysium/script.h"
#include "elysium/tx.h"

#include "base58.h"
#include "coins.h"
#include "primitives/transaction.h"
#include "random.h"
#include "script/script.h"
#include "script/standard.h"
#include "test/test_bitcoin.h"

#include <stdint.h>
#include <algorithm>
#include <limits>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace elysium;

BOOST_FIXTURE_TEST_SUITE(elysium_sender_bycontribution_tests, BasicTestingSetup)

// Forward declarations
static CTransaction TxClassB(const std::vector<CTxOut>& txInputs);
static bool GetSenderByContribution(const std::vector<CTxOut>& vouts, std::string& strSender);
static CTxOut createTxOut(int64_t amount, const std::string& dest);
static CKeyID createRandomKeyId();
static CScriptID createRandomScriptId();
void shuffleAndCheck(std::vector<CTxOut>& vouts, unsigned nRounds);

// Test settings
static const unsigned nOutputs = 256;
static const unsigned nAllRounds = 2;
static const unsigned nShuffleRounds = 16;

/**
 * Tests the invalidation of the transaction, when there are not allowed inputs.
 */
BOOST_AUTO_TEST_CASE(invalid_inputs)
{
    {
        std::vector<CTxOut> vouts;
        vouts.push_back(PayToPubKey_Unrelated());
        vouts.push_back(PayToPubKeyHash_Unrelated());
        std::string strSender;
        BOOST_CHECK(!GetSenderByContribution(vouts, strSender));
    }
    {
        std::vector<CTxOut> vouts;
        vouts.push_back(PayToPubKeyHash_Unrelated());
        vouts.push_back(PayToBareMultisig_1of3());
        std::string strSender;
        BOOST_CHECK(!GetSenderByContribution(vouts, strSender));
    }
    {
        std::vector<CTxOut> vouts;
        vouts.push_back(PayToScriptHash_Unrelated());
        vouts.push_back(PayToPubKeyHash_Elysium());
        vouts.push_back(NonStandardOutput());
        std::string strSender;
        BOOST_CHECK(!GetSenderByContribution(vouts, strSender));
    }
}

/**
 * Tests sender selection "by sum" with pay-to-pubkey-hash outputs, where a single
 * candidate has the highest output value.
 */
BOOST_AUTO_TEST_CASE(p2pkh_contribution_by_sum_test)
{
    std::vector<CTxOut> vouts;
    vouts.push_back(createTxOut(100, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(100, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(100, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(100, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk"));
    vouts.push_back(createTxOut(100, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk"));
    vouts.push_back(createTxOut(999, "TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS")); // Winner
    vouts.push_back(createTxOut(100, "TXcfGqnx8xBAX751Hn4s6yQLWEWdoT2SLK"));
    vouts.push_back(createTxOut(100, "TXcfGqnx8xBAX751Hn4s6yQLWEWdoT2SLK"));
    vouts.push_back(createTxOut(100, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));

    std::string strExpected("TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS");

    for (int i = 0; i < 10; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests sender selection "by sum" with pay-to-pubkey-hash outputs, where a candidate
 * with the highest output value by sum, with more than one output, is chosen.
 */
BOOST_AUTO_TEST_CASE(p2pkh_contribution_by_total_sum_test)
{
    std::vector<CTxOut> vouts;
    vouts.push_back(createTxOut(499, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(501, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(295, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk")); // Winner
    vouts.push_back(createTxOut(310, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk")); // Winner
    vouts.push_back(createTxOut(400, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk")); // Winner
    vouts.push_back(createTxOut(500, "TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS"));
    vouts.push_back(createTxOut(500, "TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS"));

    std::string strExpected("TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk");

    for (int i = 0; i < 10; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests sender selection "by sum" with pay-to-pubkey-hash outputs, where all outputs
 * have equal values, and a candidate is chosen based on the lexicographical order of
 * the base58 string representation (!) of the candidate.
 *
 * Note: it reflects the behavior of Elysium Core, but this edge case is not specified.
 */
BOOST_AUTO_TEST_CASE(p2pkh_contribution_by_sum_order_test)
{
    std::vector<CTxOut> vouts;
    vouts.push_back(createTxOut(1000, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));
    vouts.push_back(createTxOut(1000, "TWVoSbgCJdbtiZWs6kNMisgc65ESQ4C9Dx"));
    vouts.push_back(createTxOut(1000, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(1000, "TXcfGqnx8xBAX751Hn4s6yQLWEWdoT2SLK"));
    vouts.push_back(createTxOut(1000, "TSv7WJjRQV5daVGgCqj5WzW7zcSbNLFtdi"));
    vouts.push_back(createTxOut(1000, "TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS"));
    vouts.push_back(createTxOut(1000, "TYFPpfthFi4U13VBFe4uVgn7d4PL3XQJoU"));
    vouts.push_back(createTxOut(1000, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk")); // Winner 1st lexico order
    vouts.push_back(createTxOut(1000, "TNLDt4LKBGQKM1pzKh1FUCKMGNFi8QVAKN"));

    std::string strExpected("TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk");

    for (int i = 0; i < 10; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests sender selection "by sum" with pay-to-script-hash outputs, where a single
 * candidate has the highest output value.
 */
BOOST_AUTO_TEST_CASE(p2sh_contribution_by_sum_test)
{
    std::vector<CTxOut> vouts;
    vouts.push_back(createTxOut(100, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));
    vouts.push_back(createTxOut(150, "TWVoSbgCJdbtiZWs6kNMisgc65ESQ4C9Dx"));
    vouts.push_back(createTxOut(400, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(100, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));
    vouts.push_back(createTxOut(400, "TXcfGqnx8xBAX751Hn4s6yQLWEWdoT2SLK"));
    vouts.push_back(createTxOut(100, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));
    vouts.push_back(createTxOut(777, "TSv7WJjRQV5daVGgCqj5WzW7zcSbNLFtdi")); // Winner
    vouts.push_back(createTxOut(100, "TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS"));

    std::string strExpected("TSv7WJjRQV5daVGgCqj5WzW7zcSbNLFtdi");

    for (int i = 0; i < 10; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests sender selection "by sum" with pay-to-pubkey-hash and pay-to-script-hash
 * outputs mixed, where a candidate with the highest output value by sum, with more
 * than one output, is chosen.
 */
BOOST_AUTO_TEST_CASE(p2sh_contribution_by_total_sum_test)
{
    std::vector<CTxOut> vouts;

    vouts.push_back(createTxOut(100, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));
    vouts.push_back(createTxOut(500, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));
    vouts.push_back(createTxOut(600, "TWVoSbgCJdbtiZWs6kNMisgc65ESQ4C9Dx")); // Winner
    vouts.push_back(createTxOut(500, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(100, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(350, "TWVoSbgCJdbtiZWs6kNMisgc65ESQ4C9Dx")); // Winner
    vouts.push_back(createTxOut(110, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));

    std::string strExpected("TWVoSbgCJdbtiZWs6kNMisgc65ESQ4C9Dx");

    for (int i = 0; i < 10; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests sender selection "by sum" with pay-to-script-hash outputs, where all outputs
 * have equal values, and a candidate is chosen based on the lexicographical order of
 * the base58 string representation (!) of the candidate.
 *
 * Note: it reflects the behavior of Elysium Core, but this edge case is not specified.
 */
BOOST_AUTO_TEST_CASE(p2sh_contribution_by_sum_order_test)
{
    std::vector<CTxOut> vouts;
    vouts.push_back(createTxOut(1000, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN")); 
    vouts.push_back(createTxOut(1000, "TWVoSbgCJdbtiZWs6kNMisgc65ESQ4C9Dx"));
    vouts.push_back(createTxOut(1000, "TWouVuTvuM4xEXME1MsFvQwA1tSQihga8g"));
    vouts.push_back(createTxOut(1000, "TXcfGqnx8xBAX751Hn4s6yQLWEWdoT2SLK"));
    vouts.push_back(createTxOut(1000, "TSv7WJjRQV5daVGgCqj5WzW7zcSbNLFtdi"));
    vouts.push_back(createTxOut(1000, "TVZwNDBjuf9js2cQwTKF4L9iGxTxaxv4hS"));
    vouts.push_back(createTxOut(1000, "TYFPpfthFi4U13VBFe4uVgn7d4PL3XQJoU"));
    vouts.push_back(createTxOut(1000, "TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk")); // Winner
    vouts.push_back(createTxOut(1000, "TNLDt4LKBGQKM1pzKh1FUCKMGNFi8QVAKN"));

    std::string strExpected("TDqmmjgwMbqs7xY6A969kGWhJqeVZZydkk");

    for (int i = 0; i < 10; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests sender selection "by sum", where the lexicographical order of the base58
 * representation as string (instead of uint160) determines the chosen candidate.
 *
 * In practise this implies selecting the sender "by sum" via a comparison of
 * CBitcoinAddress objects would yield faulty results.
 *
 * Note: it reflects the behavior of Elysium Core, but this edge case is not specified.
 */
BOOST_AUTO_TEST_CASE(sender_selection_string_based_test)
{
    std::vector<CTxOut> vouts;
    vouts.push_back(createTxOut(1000, "THioNe4j3HFRvmTL7X8t3F9arw8qo6WB35")); // Winner
    vouts.push_back(createTxOut(1000, "TSv7WJjRQV5daVGgCqj5WzW7zcSbNLFtdi"));
    vouts.push_back(createTxOut(1000, "TYFPpfthFi4U13VBFe4uVgn7d4PL3XQJoU"));
    // Hash 160: 06569c8f59f428db748c94bf1d5d9f5f9d0db116
    vouts.push_back(createTxOut(1000, "TLZJN767TnL43Y9cADr2XESR8gnvQqNEUN"));  // Not!

    std::string strExpected("THioNe4j3HFRvmTL7X8t3F9arw8qo6WB35");

    for (int i = 0; i < 24; ++i) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strExpected, strSender);
    }
}

/**
 * Tests order independence of the sender selection "by sum" for pay-to-pubkey-hash
 * outputs, where all output values are equal.
 */
BOOST_AUTO_TEST_CASE(sender_selection_same_amount_test)
{
    for (unsigned i = 0; i < nAllRounds; ++i) {
        std::vector<CTxOut> vouts;
        for (unsigned n = 0; n < nOutputs; ++n) {
            CTxOut output(static_cast<int64_t>(1000),
                    GetScriptForDestination(createRandomKeyId()));
            vouts.push_back(output);
        }
        shuffleAndCheck(vouts, nShuffleRounds);
    }
}

/**
 * Tests order independence of the sender selection "by sum" for pay-to-pubkey-hash
 * outputs, where output values are different for each output.
 */
BOOST_AUTO_TEST_CASE(sender_selection_increasing_amount_test)
{
    for (unsigned i = 0; i < nAllRounds; ++i) {
        std::vector<CTxOut> vouts;
        for (unsigned n = 0; n < nOutputs; ++n) {
            CTxOut output(static_cast<int64_t>(1000 + n),
                    GetScriptForDestination(createRandomKeyId()));
            vouts.push_back(output);
        }
        shuffleAndCheck(vouts, nShuffleRounds);
    }
}

/**
 * Tests order independence of the sender selection "by sum" for pay-to-pubkey-hash
 * and pay-to-script-hash outputs mixed together, where output values are equal for
 * every second output.
 */
BOOST_AUTO_TEST_CASE(sender_selection_mixed_test)
{
    for (unsigned i = 0; i < nAllRounds; ++i) {
        std::vector<CTxOut> vouts;
        for (unsigned n = 0; n < nOutputs; ++n) {
            CScript scriptPubKey;
            if (GetRandInt(2) == 0) {
                scriptPubKey = GetScriptForDestination(createRandomKeyId());
            } else {
                scriptPubKey = GetScriptForDestination(createRandomScriptId());
            };
            int64_t nAmount = static_cast<int64_t>(1000 - n * (n % 2 == 0));
            vouts.push_back(CTxOut(nAmount, scriptPubKey));
        }
        shuffleAndCheck(vouts, nShuffleRounds);
    }
}

/** Creates a dummy class B transaction with the given inputs. */
static CTransaction TxClassB(const std::vector<CTxOut>& txInputs)
{
    CMutableTransaction mutableTx;

    // Inputs:
    for (std::vector<CTxOut>::const_iterator it = txInputs.begin(); it != txInputs.end(); ++it)
    {
        const CTxOut& txOut = *it;

        // Create transaction for input:
        CMutableTransaction inputTx;
        unsigned int nOut = 0;
        inputTx.vout.push_back(txOut);
        CTransaction tx(inputTx);

        // Populate transaction cache:
        ModifyCoin(view, COutPoint(tx.GetHash(), 0),
            [&txOut](Coin & coin){
                coin.out.scriptPubKey = txOut.scriptPubKey;
                coin.out.nValue = txOut.nValue;
            });

        // Add input:
        CTxIn txIn(tx.GetHash(), nOut);
        mutableTx.vin.push_back(txIn);
    }

    // Outputs:
    mutableTx.vout.push_back(PayToPubKeyHash_Elysium());
    mutableTx.vout.push_back(PayToBareMultisig_1of3());
    mutableTx.vout.push_back(PayToPubKeyHash_Unrelated());

    return CTransaction(mutableTx);
}

/** Extracts the sender "by contribution". */
static bool GetSenderByContribution(const std::vector<CTxOut>& vouts, std::string& strSender)
{
    int nBlock = std::numeric_limits<int>::max();

    CMPTransaction metaTx;
    CTransaction dummyTx = TxClassB(vouts);

    if (ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0) {
        strSender = metaTx.getSender();
        return true;
    }

    return false;
}

/** Helper to create a CTxOut object. */
static CTxOut createTxOut(int64_t amount, const std::string& dest)
{
    return CTxOut(amount, GetScriptForDestination(CBitcoinAddress(dest).Get()));
}

/** Helper to create a CKeyID object with random value.*/
static CKeyID createRandomKeyId()
{
    std::vector<unsigned char> vch;
    vch.reserve(20);
    for (int i = 0; i < 20; ++i) {
        vch.push_back(static_cast<unsigned char>(GetRandInt(256)));
    }
    return CKeyID(uint160(vch));
}

/** Helper to create a CScriptID object with random value.*/
static CScriptID createRandomScriptId()
{
    std::vector<unsigned char> vch;
    vch.reserve(20);
    for (int i = 0; i < 20; ++i) {
        vch.push_back(static_cast<unsigned char>(GetRandInt(256)));
    }
    return CScriptID(uint160(vch));
}

/**
 * Identifies the sender of a transaction, based on the list of provided transaction
 * outputs, and then shuffles the list n times, while checking, if this produces the
 * same result. The "contribution by sum" sender selection doesn't require specific
 * positions or order of outputs, and should work in all cases.
 */
void shuffleAndCheck(std::vector<CTxOut>& vouts, unsigned nRounds)
{
    std::string strSenderFirst;
    BOOST_CHECK(GetSenderByContribution(vouts, strSenderFirst));

    for (unsigned j = 0; j < nRounds; ++j) {
        std::random_shuffle(vouts.begin(), vouts.end(), GetRandInt);

        std::string strSender;
        BOOST_CHECK(GetSenderByContribution(vouts, strSender));
        BOOST_CHECK_EQUAL(strSenderFirst, strSender);
    }
}


BOOST_AUTO_TEST_SUITE_END()

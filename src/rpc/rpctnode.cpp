#include "activetnode.h"
#include "init.h"
#include "validation.h"
#include "tnode-payments.h"
#include "tnode-sync.h"
#include "tnodeconfig.h"
#include "tnodeman.h"
#include "darksend.h"
#include "rpc/server.h"
#include "util.h"
#include "utilmoneystr.h"
#include "net.h"
#include "netbase.h"

#include <fstream>
#include <iomanip>
#include <univalue.h>

UniValue privatesend(const JSONRPCRequest &request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                "privatesend \"command\"\n"
                        "\nArguments:\n"
                        "1. \"command\"        (string or set of strings, required) The command to execute\n"
                        "\nAvailable commands:\n"
                        "  start       - Start mixing\n"
                        "  stop        - Stop mixing\n"
                        "  reset       - Reset mixing\n"
        );

    if (request.params[0].get_str() == "start") {
        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        if (fTnodeMode)
            return "Mixing is not supported from tnodes";

        fEnablePrivateSend = true;
        bool result = darkSendPool.DoAutomaticDenominating();
        return "Mixing " +
               (result ? "started successfully" : ("start failed: " + darkSendPool.GetStatus() + ", will retry"));
    }

    if (request.params[0].get_str() == "stop") {
        fEnablePrivateSend = false;
        return "Mixing was stopped";
    }

    if (request.params[0].get_str() == "reset") {
        darkSendPool.ResetPool();
        return "Mixing was reset";
    }

    return "Unknown command, please see \"help privatesend\"";
}

UniValue getpoolinfo(const JSONRPCRequest &request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "getpoolinfo\n"
                        "Returns an object containing mixing pool related information.\n");

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("state", darkSendPool.GetStateString()));
//    obj.push_back(Pair("mixing_mode",       fPrivateSendMultiSession ? "multi-session" : "normal"));
    obj.push_back(Pair("queue", darkSendPool.GetQueueSize()));
    obj.push_back(Pair("entries", darkSendPool.GetEntriesCount()));
    obj.push_back(Pair("status", darkSendPool.GetStatus()));

    if (darkSendPool.pSubmittedToTnode) {
        obj.push_back(Pair("outpoint", darkSendPool.pSubmittedToTnode->vin.prevout.ToStringShort()));
        obj.push_back(Pair("addr", darkSendPool.pSubmittedToTnode->addr.ToString()));
    }

    if (pwallet) {
        obj.push_back(Pair("keys_left", pwallet->nKeysLeftSinceAutoBackup));
        obj.push_back(Pair("warnings", pwallet->nKeysLeftSinceAutoBackup < PRIVATESEND_KEYS_THRESHOLD_WARNING
                                       ? "WARNING: keypool is almost depleted!" : ""));
    }

    return obj;
}


UniValue tnode(const JSONRPCRequest &request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);

    std::string strCommand;
    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();
    }

    if (strCommand == "start-many")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "DEPRECATED, please use start-all instead");

    if (request.fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-all" &&
         strCommand != "start-missing" &&
         strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count" &&
         strCommand != "debug" && strCommand != "current" && strCommand != "winner" && strCommand != "winners" &&
         strCommand != "genkey" &&
         strCommand != "connect" && strCommand != "outputs" && strCommand != "status"))
        throw std::runtime_error(
                "tnode \"command\"...\n"
                        "Set of commands to execute tnode related actions\n"
                        "\nArguments:\n"
                        "1. \"command\"        (string or set of strings, required) The command to execute\n"
                        "\nAvailable commands:\n"
                        "  count        - Print number of all known tnodes (optional: 'ps', 'enabled', 'all', 'qualify')\n"
                        "  current      - Print info on current tnode winner to be paid the next block (calculated locally)\n"
                        "  debug        - Print tnode status\n"
                        "  genkey       - Generate new tnodeprivkey\n"
                        "  outputs      - Print tnode compatible outputs\n"
                        "  start        - Start local Hot tnode configured in tecrscoin.conf\n"
                        "  start-alias  - Start single remote tnode by assigned alias configured in tnode.conf\n"
                        "  start-<mode> - Start remote tnodes configured in tnode.conf (<mode>: 'all', 'missing', 'disabled')\n"
                        "  status       - Print tnode status information\n"
                        "  list         - Print list of all known tnodes (see tnodelist for more info)\n"
                        "  list-conf    - Print tnode.conf in JSON format\n"
                        "  winner       - Print info on next tnode winner to vote for\n"
                        "  winners      - Print list of tnode winners\n"
        );

    if (strCommand == "list") {
        JSONRPCRequest newRequest;
        // forward request.params but skip "list"
        newRequest.params = UniValue(UniValue::VARR);
        for (unsigned int i = 1; i < request.params.size(); i++) {
            newRequest.params.push_back(request.params[i]);
        }
        return tnodelist(newRequest);
    }

    if (strCommand == "connect") {
        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Tnode address required");

        std::string strAddress = request.params[1].get_str();
        std::vector<CNetAddr> ip;

        if (!LookupHost(strAddress.c_str(), ip, 1, false))
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Couldn't connect to tnode %s", strAddress));

        g_connman->OpenMasternodeConnection(CAddress(CService(ip[0], 0), NODE_NETWORK));
        /*if (!g_connman->IsConnected(CAddress(addr, NODE_NETWORK), CConnman::AllNodes))
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Couldn't connect to tnode %s", strAddress));*/

        return "successfully connected";
    }

    if (strCommand == "count") {
        if (request.params.size() > 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Too many parameters");

        if (request.params.size() == 1)
            return mnodeman.size();

        std::string strMode = request.params[1].get_str();

        if (strMode == "ps")
            return mnodeman.CountEnabled(MIN_PRIVATESEND_PEER_PROTO_VERSION);

        if (strMode == "enabled")
            return mnodeman.CountEnabled();

        int nCount;
        mnodeman.GetNextTnodeInQueueForPayment(true, nCount);

        if (strMode == "qualify")
            return nCount;

        if (strMode == "all")
            return strprintf("Total: %d (PS Compatible: %d / Enabled: %d / Qualify: %d)",
                             mnodeman.size(), mnodeman.CountEnabled(MIN_PRIVATESEND_PEER_PROTO_VERSION),
                             mnodeman.CountEnabled(), nCount);
    }

    if (strCommand == "current" || strCommand == "winner") {
        int nCount;
        int nHeight;
        CTnode *winner = NULL;
        {
            LOCK(cs_main);
            nHeight = chainActive.Height() + (strCommand == "current" ? 1 : 10);
        }
        mnodeman.UpdateLastPaid();
        winner = mnodeman.GetNextTnodeInQueueForPayment(nHeight, true, nCount);
        if (!winner) return "unknown";

        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("height", nHeight));
        obj.push_back(Pair("IP:port", winner->addr.ToString()));
        obj.push_back(Pair("protocol", (int64_t) winner->nProtocolVersion));
        obj.push_back(Pair("vin", winner->vin.prevout.ToStringShort()));
        obj.push_back(Pair("payee", CBitcoinAddress(winner->pubKeyCollateralAddress.GetID()).ToString()));
        obj.push_back(Pair("lastseen", (winner->lastPing == CTnodePing()) ? winner->sigTime :
                                       winner->lastPing.sigTime));
        obj.push_back(Pair("activeseconds", (winner->lastPing == CTnodePing()) ? 0 :
                                            (winner->lastPing.sigTime - winner->sigTime)));
        obj.push_back(Pair("nBlockLastPaid", winner->nBlockLastPaid));
        return obj;
    }

    if (strCommand == "debug") {
        if (activeTnode.nState != ACTIVE_TNODE_INITIAL || !tnodeSync.IsBlockchainSynced())
            return activeTnode.GetStatus();

        CTxIn vin;
        CPubKey pubkey;
        CKey key;

        if (!pwallet || !pwallet->GetTnodeVinAndKeys(vin, pubkey, key))
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Missing tnode input, please look at the documentation for instructions on tnode creation");

        return activeTnode.GetStatus();
    }

    if (strCommand == "start") {
        if (!fTnodeMode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "You must set tnode=1 in the configuration");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        if (activeTnode.nState != ACTIVE_TNODE_STARTED) {
            activeTnode.nState = ACTIVE_TNODE_INITIAL; // TODO: consider better way
            activeTnode.ManageState();
        }

        return activeTnode.GetStatus();
    }

    if (strCommand == "start-alias") {
        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Please specify an alias");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        std::string strAlias = request.params[1].get_str();

        bool fFound = false;

        UniValue statusObj(UniValue::VOBJ);
        statusObj.push_back(Pair("alias", strAlias));

        BOOST_FOREACH(CTnodeConfig::CTnodeEntry mne, tnodeConfig.getEntries()) {
            if (mne.getAlias() == strAlias) {
                fFound = true;
                std::string strError;
                CTnodeBroadcast mnb;

                bool fResult = CTnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                            mne.getOutputIndex(), strError, mnb);
                statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));
                if (fResult) {
                    mnodeman.UpdateTnodeList(mnb);
                    mnb.RelayTNode();
                } else {
                    LogPrintf("Start-alias: errorMessage = %s\n", strError);
                    statusObj.push_back(Pair("errorMessage", strError));
                }
                mnodeman.NotifyTnodeUpdates();
                break;
            }
        }

        if (!fFound) {
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("errorMessage", "Could not find alias in config. Verify with list-conf."));
        }

//        LogPrintf("start-alias: statusObj=%s\n", statusObj);

        return statusObj;

    }

    if (strCommand == "start-all" || strCommand == "start-missing" || strCommand == "start-disabled") {
        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        if ((strCommand == "start-missing" || strCommand == "start-disabled") &&
            !tnodeSync.IsTnodeListSynced()) {
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                               "You can't use this command until tnode list is synced");
        }

        int nSuccessful = 0;
        int nFailed = 0;

        UniValue resultsObj(UniValue::VOBJ);

        BOOST_FOREACH(CTnodeConfig::CTnodeEntry mne, tnodeConfig.getEntries()) {
            std::string strError;

            CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CTnode *pmn = mnodeman.Find(vin);
            CTnodeBroadcast mnb;

            if (strCommand == "start-missing" && pmn) continue;
            if (strCommand == "start-disabled" && pmn && pmn->IsEnabled()) continue;

            bool fResult = CTnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                        mne.getOutputIndex(), strError, mnb);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));

            if (fResult) {
                nSuccessful++;
                mnodeman.UpdateTnodeList(mnb);
                mnb.RelayTNode();
            } else {
                nFailed++;
                statusObj.push_back(Pair("errorMessage", strError));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        mnodeman.NotifyTnodeUpdates();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall",
                                 strprintf("Successfully started %d tnodes, failed to start %d, total %d",
                                           nSuccessful, nFailed, nSuccessful + nFailed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "genkey") {
        CKey secret;
        secret.MakeNewKey(false);

        return CBitcoinSecret(secret).ToString();
    }

    if (strCommand == "list-conf") {
        UniValue resultObj(UniValue::VOBJ);

        BOOST_FOREACH(CTnodeConfig::CTnodeEntry mne, tnodeConfig.getEntries()) {
            CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CTnode *pmn = mnodeman.Find(vin);

            std::string strStatus = pmn ? pmn->GetStatus() : "MISSING";

            UniValue mnObj(UniValue::VOBJ);
            mnObj.push_back(Pair("alias", mne.getAlias()));
            mnObj.push_back(Pair("address", mne.getIp()));
            mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
            mnObj.push_back(Pair("txHash", mne.getTxHash()));
            mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
            mnObj.push_back(Pair("status", strStatus));
            resultObj.push_back(Pair("tnode", mnObj));
        }

        return resultObj;
    }

    if (strCommand == "outputs") {
        // Find possible candidates
        std::vector <COutput> vPossibleCoins;
        pwallet->AvailableCoins(vPossibleCoins, true, NULL, false, ONLY_1000);

        UniValue obj(UniValue::VOBJ);
        BOOST_FOREACH(COutput & out, vPossibleCoins)
        {
            obj.push_back(Pair(out.tx->GetHash().ToString(), strprintf("%d", out.i)));
        }

        return obj;

    }

    if (strCommand == "status") {
        if (!fTnodeMode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "This is not a tnode");

        UniValue mnObj(UniValue::VOBJ);

        mnObj.push_back(Pair("vin", activeTnode.vin.ToString()));
        mnObj.push_back(Pair("service", activeTnode.service.ToString()));

        CTnode mn;
        if (mnodeman.Get(activeTnode.vin, mn)) {
            mnObj.push_back(Pair("payee", CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));
        }

        mnObj.push_back(Pair("status", activeTnode.GetStatus()));
        return mnObj;
    }

    if (strCommand == "winners") {
        int nHeight;
        {
            LOCK(cs_main);
            CBlockIndex *pindex = chainActive.Tip();
            if (!pindex) return NullUniValue;

            nHeight = pindex->nHeight;
        }

        int nLast = 10;
        std::string strFilter = "";

        if (request.params.size() >= 2) {
            nLast = atoi(request.params[1].get_str());
        }

        if (request.params.size() == 3) {
            strFilter = request.params[2].get_str();
        }

        if (request.params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'tnode winners ( \"count\" \"filter\" )'");

        UniValue obj(UniValue::VOBJ);

        for (int i = nHeight - nLast; i < nHeight + 20; i++) {
            std::string strPayment = GetRequiredPaymentsString(i);
            if (strFilter != "" && strPayment.find(strFilter) == std::string::npos) continue;
            obj.push_back(Pair(strprintf("%d", i), strPayment));
        }

        return obj;
    }

    return NullUniValue;
}

UniValue tnodelist(const JSONRPCRequest &request) {
    std::string strMode = "status";
    std::string strFilter = "";

    if (request.params.size() >= 1) strMode = request.params[0].get_str();
    if (request.params.size() == 2) strFilter = request.params[1].get_str();

    if (request.fHelp || (
            strMode != "activeseconds" && strMode != "addr" && strMode != "full" &&
            strMode != "lastseen" && strMode != "lastpaidtime" && strMode != "lastpaidblock" &&
            strMode != "protocol" && strMode != "payee" && strMode != "rank" && strMode != "qualify" &&
            strMode != "status" && strMode != "json")) {
        throw std::runtime_error(
                "tnodelist ( \"mode\" \"filter\" )\n"
                        "Get a list of tnodes in different modes\n"
                        "\nArguments:\n"
                        "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
                        "2. \"filter\"    (string, optional) Filter results. Partial match by outpoint by default in all modes,\n"
                        "                                    additional matches in some modes are also available\n"
                        "\nAvailable modes:\n"
                        "  activeseconds  - Print number of seconds tnode recognized by the network as enabled\n"
                        "                   (since latest issued \"tnode start/start-many/start-alias\")\n"
                        "  addr           - Print ip address associated with a tnode (can be additionally filtered, partial match)\n"
                        "  full           - Print info in format 'status protocol payee lastseen activeseconds lastpaidtime lastpaidblock IP'\n"
                        "                   (can be additionally filtered, partial match)\n"
                        "  json           - Print info in JSON format (can be additionally filtered, partial match)\n"
                        "  lastpaidblock  - Print the last block height a node was paid on the network\n"
                        "  lastpaidtime   - Print the last time a node was paid on the network\n"
                        "  lastseen       - Print timestamp of when a tnode was last seen on the network\n"
                        "  payee          - Print TecraCoin address associated with a tnode (can be additionally filtered,\n"
                        "                   partial match)\n"
                        "  protocol       - Print protocol of a tnode (can be additionally filtered, exact match))\n"
                        "  rank           - Print rank of a tnode based on current block\n"
                        "  qualify        - Print qualify status of a tnode based on current block\n"
                        "  status         - Print tnode status: PRE_ENABLED / ENABLED / EXPIRED / WATCHDOG_EXPIRED / NEW_START_REQUIRED /\n"
                        "                   UPDATE_REQUIRED / POSE_BAN / OUTPOINT_SPENT (can be additionally filtered, partial match)\n"
        );
    }

    if (strMode == "full" || strMode == "lastpaidtime" || strMode == "lastpaidblock") {
        mnodeman.UpdateLastPaid();
    }

    UniValue obj(UniValue::VOBJ);
    if (strMode == "rank") {
        std::vector <std::pair<int, CTnode>> vTnodeRanks = mnodeman.GetTnodeRanks();
        BOOST_FOREACH(PAIRTYPE(int, CTnode) & s, vTnodeRanks)
        {
            std::string strOutpoint = s.second.vin.prevout.ToStringShort();
            if (strFilter != "" && strOutpoint.find(strFilter) == std::string::npos) continue;
            obj.push_back(Pair(strOutpoint, s.first));
        }
    } else {
        std::vector <CTnode> vTnodes = mnodeman.GetFullTnodeVector();
        BOOST_FOREACH(CTnode & mn, vTnodes)
        {
            std::string strOutpoint = mn.vin.prevout.ToStringShort();
            if (strMode == "activeseconds") {
                if (strFilter != "" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, (int64_t)(mn.lastPing.sigTime - mn.sigTime)));
            } else if (strMode == "addr") {
                std::string strAddress = mn.addr.ToString();
                if (strFilter != "" && strAddress.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos)
                    continue;
                obj.push_back(Pair(strOutpoint, strAddress));
            } else if (strMode == "full") {
                std::ostringstream streamFull;
                streamFull << std::setw(18) <<
                           mn.GetStatus() << " " <<
                           mn.nProtocolVersion << " " <<
                           CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString() << " " <<
                           (int64_t) mn.lastPing.sigTime << " " << std::setw(8) <<
                           (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " << std::setw(10) <<
                           mn.GetLastPaidTime() << " " << std::setw(6) <<
                           mn.GetLastPaidBlock() << " " <<
                           mn.addr.ToString();
                std::string strFull = streamFull.str();
                if (strFilter != "" && strFull.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos)
                    continue;
                obj.push_back(Pair(strOutpoint, strFull));
            } else if (strMode == "json") {
                std::ostringstream streamJSON;
                streamJSON << std::setw(18) <<
                           mn.GetStatus() << " " <<
                           mn.nProtocolVersion << " " <<
                           CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString() << " " <<
                           (int64_t) mn.lastPing.sigTime << " " << std::setw(8) <<
                           (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " << std::setw(10) <<
                           mn.GetLastPaidTime() << " " << std::setw(6) <<
                           mn.GetLastPaidBlock() << " " <<
                           mn.addr.ToString();
                std::string strJSON = streamJSON.str();
                if (strFilter != "" && strJSON.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos)
                    continue;

                UniValue objMN(UniValue::VOBJ);
                objMN.push_back(Pair("ip", mn.addr.ToStringIP()));
                objMN.push_back(Pair("port", mn.addr.ToStringPort()));
                objMN.push_back(Pair("payee", CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));
                objMN.push_back(Pair("status", mn.GetStatus()));
                objMN.push_back(Pair("protocol", mn.nProtocolVersion));
                objMN.push_back(Pair("lastseen", (int64_t)mn.lastPing.sigTime));
                objMN.push_back(Pair("activeseconds", (int64_t)(mn.lastPing.sigTime - mn.sigTime)));
                objMN.push_back(Pair("lastpaidtime", mn.GetLastPaidTime()));
                objMN.push_back(Pair("lastpaidblock", mn.GetLastPaidBlock()));
                obj.push_back(Pair(strOutpoint, objMN));
            } else if (strMode == "lastpaidblock") {
                if (strFilter != "" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, mn.GetLastPaidBlock()));
            } else if (strMode == "lastpaidtime") {
                if (strFilter != "" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, mn.GetLastPaidTime()));
            } else if (strMode == "lastseen") {
                if (strFilter != "" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, (int64_t) mn.lastPing.sigTime));
            } else if (strMode == "payee") {
                CBitcoinAddress address(mn.pubKeyCollateralAddress.GetID());
                std::string strPayee = address.ToString();
                if (strFilter != "" && strPayee.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos)
                    continue;
                obj.push_back(Pair(strOutpoint, strPayee));
            } else if (strMode == "protocol") {
                if (strFilter != "" && strFilter != strprintf("%d", mn.nProtocolVersion) &&
                    strOutpoint.find(strFilter) == std::string::npos)
                    continue;
                obj.push_back(Pair(strOutpoint, (int64_t) mn.nProtocolVersion));
            } else if (strMode == "status") {
                std::string strStatus = mn.GetStatus();
                if (strFilter != "" && strStatus.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos)
                    continue;
                obj.push_back(Pair(strOutpoint, strStatus));
            } else if (strMode == "qualify") {
                int nBlockHeight;
                {
                    LOCK(cs_main);
                    CBlockIndex *pindex = chainActive.Tip();
                    if (!pindex) return NullUniValue;

                    nBlockHeight = pindex->nHeight;
                }
                int nMnCount = mnodeman.CountEnabled();
                char* reasonStr = mnodeman.GetNotQualifyReason(mn, nBlockHeight, true, nMnCount);
                std::string strOutpoint = mn.vin.prevout.ToStringShort();
                if (strFilter != "" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, (reasonStr != NULL) ? reasonStr : "true"));
            }
        }
    }
    return obj;
}

bool DecodeHexVecMnb(std::vector <CTnodeBroadcast> &vecMnb, std::string strHexMnb) {

    if (!IsHex(strHexMnb))
        return false;

    std::vector<unsigned char> mnbData(ParseHex(strHexMnb));
    CDataStream ssData(mnbData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> vecMnb;
    }
    catch (const std::exception &) {
        return false;
    }

    return true;
}

UniValue tnodebroadcast(const JSONRPCRequest &request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);

    std::string strCommand;
    if (request.params.size() >= 1)
        strCommand = request.params[0].get_str();

    if (request.fHelp ||
        (strCommand != "create-alias" && strCommand != "create-all" && strCommand != "decode" && strCommand != "relay"))
        throw std::runtime_error(
                "tnodebroadcast \"command\"...\n"
                        "Set of commands to create and relay tnode broadcast messages\n"
                        "\nArguments:\n"
                        "1. \"command\"        (string or set of strings, required) The command to execute\n"
                        "\nAvailable commands:\n"
                        "  create-alias  - Create single remote tnode broadcast message by assigned alias configured in tnode.conf\n"
                        "  create-all    - Create remote tnode broadcast messages for all tnodes configured in tnode.conf\n"
                        "  decode        - Decode tnode broadcast message\n"
                        "  relay         - Relay tnode broadcast message to the network\n"
        );

    if (strCommand == "create-alias") {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Please specify an alias");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        bool fFound = false;
        std::string strAlias = request.params[1].get_str();

        UniValue statusObj(UniValue::VOBJ);
        std::vector <CTnodeBroadcast> vecMnb;

        statusObj.push_back(Pair("alias", strAlias));

        BOOST_FOREACH(CTnodeConfig::CTnodeEntry
        mne, tnodeConfig.getEntries()) {
            if (mne.getAlias() == strAlias) {
                fFound = true;
                std::string strError;
                CTnodeBroadcast mnb;

                bool fResult = CTnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                            mne.getOutputIndex(), strError, mnb, true);

                statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));
                if (fResult) {
                    vecMnb.push_back(mnb);
                    CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
                    ssVecMnb << vecMnb;
                    statusObj.push_back(Pair("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end())));
                } else {
                    statusObj.push_back(Pair("errorMessage", strError));
                }
                break;
            }
        }

        if (!fFound) {
            statusObj.push_back(Pair("result", "not found"));
            statusObj.push_back(Pair("errorMessage", "Could not find alias in config. Verify with list-conf."));
        }

        return statusObj;

    }

    if (strCommand == "create-all") {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        std::vector <CTnodeConfig::CTnodeEntry> mnEntries;
        mnEntries = tnodeConfig.getEntries();

        int nSuccessful = 0;
        int nFailed = 0;

        UniValue resultsObj(UniValue::VOBJ);
        std::vector <CTnodeBroadcast> vecMnb;

        BOOST_FOREACH(CTnodeConfig::CTnodeEntry
        mne, tnodeConfig.getEntries()) {
            std::string strError;
            CTnodeBroadcast mnb;

            bool fResult = CTnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                        mne.getOutputIndex(), strError, mnb, true);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));

            if (fResult) {
                nSuccessful++;
                vecMnb.push_back(mnb);
            } else {
                nFailed++;
                statusObj.push_back(Pair("errorMessage", strError));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }

        CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
        ssVecMnb << vecMnb;
        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf(
                "Successfully created broadcast messages for %d tnodes, failed to create %d, total %d",
                nSuccessful, nFailed, nSuccessful + nFailed)));
        returnObj.push_back(Pair("detail", resultsObj));
        returnObj.push_back(Pair("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end())));

        return returnObj;
    }

    if (strCommand == "decode") {
        if (request.params.size() != 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'tnodebroadcast decode \"hexstring\"'");

        std::vector <CTnodeBroadcast> vecMnb;

        if (!DecodeHexVecMnb(vecMnb, request.params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Tnode broadcast message decode failed");

        int nSuccessful = 0;
        int nFailed = 0;
        int nDos = 0;
        UniValue returnObj(UniValue::VOBJ);

        BOOST_FOREACH(CTnodeBroadcast & mnb, vecMnb)
        {
            UniValue resultObj(UniValue::VOBJ);

            if (mnb.CheckSignature(nDos)) {
                nSuccessful++;
                resultObj.push_back(Pair("vin", mnb.vin.ToString()));
                resultObj.push_back(Pair("addr", mnb.addr.ToString()));
                resultObj.push_back(Pair("pubKeyCollateralAddress",
                                         CBitcoinAddress(mnb.pubKeyCollateralAddress.GetID()).ToString()));
                resultObj.push_back(Pair("pubKeyTnode", CBitcoinAddress(mnb.pubKeyTnode.GetID()).ToString()));
                resultObj.push_back(Pair("vchSig", EncodeBase64(&mnb.vchSig[0], mnb.vchSig.size())));
                resultObj.push_back(Pair("sigTime", mnb.sigTime));
                resultObj.push_back(Pair("protocolVersion", mnb.nProtocolVersion));
                resultObj.push_back(Pair("nLastDsq", mnb.nLastDsq));

                UniValue lastPingObj(UniValue::VOBJ);
                lastPingObj.push_back(Pair("vin", mnb.lastPing.vin.ToString()));
                lastPingObj.push_back(Pair("blockHash", mnb.lastPing.blockHash.ToString()));
                lastPingObj.push_back(Pair("sigTime", mnb.lastPing.sigTime));
                lastPingObj.push_back(
                        Pair("vchSig", EncodeBase64(&mnb.lastPing.vchSig[0], mnb.lastPing.vchSig.size())));

                resultObj.push_back(Pair("lastPing", lastPingObj));
            } else {
                nFailed++;
                resultObj.push_back(Pair("errorMessage", "Tnode broadcast signature verification failed"));
            }

            returnObj.push_back(Pair(mnb.GetHash().ToString(), resultObj));
        }

        returnObj.push_back(Pair("overall", strprintf(
                "Successfully decoded broadcast messages for %d tnodes, failed to decode %d, total %d",
                nSuccessful, nFailed, nSuccessful + nFailed)));

        return returnObj;
    }

    if (strCommand == "relay") {
        if (request.params.size() < 2 || request.params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "tnodebroadcast relay \"hexstring\" ( fast )\n"
                    "\nArguments:\n"
                    "1. \"hex\"      (string, required) Broadcast messages hex string\n"
                    "2. fast       (string, optional) If none, using safe method\n");

        std::vector <CTnodeBroadcast> vecMnb;

        if (!DecodeHexVecMnb(vecMnb, request.params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Tnode broadcast message decode failed");

        int nSuccessful = 0;
        int nFailed = 0;
        bool fSafe = request.params.size() == 2;
        UniValue returnObj(UniValue::VOBJ);

        // verify all signatures first, bailout if any of them broken
        BOOST_FOREACH(CTnodeBroadcast & mnb, vecMnb)
        {
            UniValue resultObj(UniValue::VOBJ);

            resultObj.push_back(Pair("vin", mnb.vin.ToString()));
            resultObj.push_back(Pair("addr", mnb.addr.ToString()));

            int nDos = 0;
            bool fResult;
            if (mnb.CheckSignature(nDos)) {
                if (fSafe) {
                    fResult = mnodeman.CheckMnbAndUpdateTnodeList(NULL, mnb, nDos);
                } else {
                    mnodeman.UpdateTnodeList(mnb);
                    mnb.RelayTNode();
                    fResult = true;
                }
                mnodeman.NotifyTnodeUpdates();
            } else fResult = false;

            if (fResult) {
                nSuccessful++;
                resultObj.push_back(Pair(mnb.GetHash().ToString(), "successful"));
            } else {
                nFailed++;
                resultObj.push_back(Pair("errorMessage", "Tnode broadcast signature verification failed"));
            }

            returnObj.push_back(Pair(mnb.GetHash().ToString(), resultObj));
        }

        returnObj.push_back(Pair("overall", strprintf(
                "Successfully relayed broadcast messages for %d tnodes, failed to relay %d, total %d", nSuccessful,
                nFailed, nSuccessful + nFailed)));

        return returnObj;
    }

    return NullUniValue;
}

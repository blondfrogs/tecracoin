#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test re-org scenarios with a mempool that contains transactions
# that spend (directly or indirectly) coinbase transactions.
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

# Create one-input, one-output, no-fee transaction:
class MempoolCoinbaseTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.setup_clean_chain = False

    alert_filename = None  # Set by setup_network

    def setup_network(self):
        args = ["-checkmempool", "-debug=mempool"]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split = False

#        for j in range(800):  
#                self.nodes[0].generate(1)
#                self.sync_all()

    def run_test(self):
        assert_equal(self.nodes[0].getblockcount(), 800)

        # Mine three blocks. After this, nodes[0] blocks
        # 101, 102, and 103 are spend-able.
        new_blocks = self.nodes[1].generate(4)
        self.sync_all()
        self.log.info("blockcount {}".format(self.nodes[0].getblockcount()))
        node0_address = self.nodes[0].getnewaddress()
        node1_address = self.nodes[1].getnewaddress()
  
        # Three scenarios for re-orging coinbase spends in the memory pool:
        # 1. Direct coinbase spend  :  spend_101
        # 2. Indirect (coinbase spend in chain, child in mempool) : spend_102 and spend_102_1
        # 3. Indirect (coinbase and child both in chain) : spend_103 and spend_103_1
        # Use invalidatblock to make all of the above coinbase sp
        # ends invalid (immature coinbase),
        # and make sure the mempool code behaves correctly.

        b = [ self.nodes[0].getblockhash(n) for n in range(401, 405) ]
        coinbase_txids = [ self.nodes[0].getblock(h)['tx'][0] for h in b ]
        
        for n in range(4):
           rawtx = self.nodes[0].getrawtransaction(coinbase_txids[n], 1)
           self.log.info("value of rawtix1 {} tcr".format(rawtx['vout'][0]['value']))


        spend_101_raw = create_tx(self.nodes[0], coinbase_txids[1], node1_address, 0.924)
        spend_102_raw = create_tx(self.nodes[0], coinbase_txids[2], node0_address, 0.925)
        spend_103_raw = create_tx(self.nodes[0], coinbase_txids[3], node0_address, 0.924)
      
        # Create a block-height-locked transaction which will be invalid after reorg
        timelock_tx = self.nodes[0].createrawtransaction([{'txid': coinbase_txids[0], 'vout': 0}], {node0_address: 0.924})
        self.log.info("theraw transaction "+timelock_tx)
        # Set the time lock
        timelock_tx = timelock_tx.replace("ffffffff", "11111191", 1)
        self.log.info("theraw transaction "+timelock_tx)
#        timelock_tx = timelock_tx[:-8] + hex(self.nodes[0].getblockcount() + 2)[2:] + "00000"
        timelock_tx = timelock_tx[:-8] + "2603" + "0000"  # locktime = blockcounnt + 2 806
        self.log.info("theraw transaction "+timelock_tx)
        timelock_tx = self.nodes[0].signrawtransaction(timelock_tx)['hex']

        rawtx = self.nodes[0].decoderawtransaction(timelock_tx)
        self.log.info("value of rawtix1 {} tcr".format(rawtx))

        assert_raises(JSONRPCException, self.nodes[0].sendrawtransaction, timelock_tx)
        self.sync_all()       
        # Broadcast and mine spend_102 and 103:
        spend_102_id = self.nodes[0].sendrawtransaction(spend_102_raw)

        spend_103_id = self.nodes[0].sendrawtransaction(spend_103_raw)
    

        assert_raises(JSONRPCException, self.nodes[0].sendrawtransaction, timelock_tx)
        self.nodes[0].generate(1)
        self.sync_all()
        # Create 102_1 and 103_1:
        spend_102_1_raw = create_tx(self.nodes[0], spend_102_id, node1_address, 0.925)
        spend_103_1_raw = create_tx(self.nodes[0], spend_103_id, node1_address, 0.924)

        # Broadcast and mine 103_1:
        spend_103_1_id = self.nodes[0].sendrawtransaction(spend_103_1_raw)
        last_block = self.nodes[0].generate(1)
        
        self.sync_all()
        timelock_tx_id = self.nodes[0].sendrawtransaction(timelock_tx)

        # ... now put spend_101 and spend_102_1 in memory pools:
        spend_101_id = self.nodes[0].sendrawtransaction(spend_101_raw)
        spend_102_1_id = self.nodes[0].sendrawtransaction(spend_102_1_raw)



        assert_equal(set(self.nodes[0].getrawmempool()), {spend_101_id, spend_102_1_id, timelock_tx_id})

        for node in self.nodes:
            node.invalidateblock(last_block[0])
        assert_equal(set(self.nodes[0].getrawmempool()), {spend_101_id, spend_102_1_id, spend_103_1_id})

        # Use invalidateblock to re-org back and make all those coinbase spends
        # immature/invalid:
        for node in self.nodes:
            node.invalidateblock(new_blocks[0])

        self.sync_all()

        # mempool should be empty.
        assert_equal(set(self.nodes[0].getrawmempool()), set())

if __name__ == '__main__':
    MempoolCoinbaseTest().main()

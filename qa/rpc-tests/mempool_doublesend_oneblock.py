#!/usr/bin/env python3

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


#generate tx1 with coinbase1
#generate tx2 with coinbase2 and coinbase1
#send rawtransaction tx1
#send rawtransaction tx1 again
#check mempool tx count is 1
#send rawtransaction tx2
#error appeared txn-mempool-conflict
#generate block
#retry send rawtransaction tx2
#error appeared txn-mempool-conflict
#bad-txns-inputs-spent
class MempoolDoubleSpendOneBlock(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.setup_clean_chain = False

    def setup_network(self):
        # Just need one node for this test
        args = ["-checkmempool", "-debug=mempool"]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.is_network_split = False

    def run_test(self):
        node0_address = self.nodes[0].getnewaddress()
#        self.nodes[0].generate(800)
        b_count = self.nodes[0].getblockcount()

        b1 = self.nodes[0].getblockhash(b_count - 700) # using the test chain...
        coinbase_txids1 = self.nodes[0].getblock(b1)['tx'][0]
        rawtx1 = self.nodes[0].getrawtransaction(coinbase_txids1, 1)
        for txout in rawtx1['vout']:
                self.log.info("vout of txid {} tcr".format(txout['value']))

        self.log.info("value of rawtix1 {} tcr".format(rawtx1['vout'][0]['value']))

        b = self.nodes[0].getblockhash(b_count-701)
        coinbase_txids2 = self.nodes[0].getblock(b)['tx'][0]
        rawtx2 = self.nodes[0].getrawtransaction(coinbase_txids2, 1)
        for txout in rawtx2['vout']:
                self.log.info("vout of txid {} tcr".format(txout['value']))

        self.log.info("value of rawtix1 {} tcr".format(rawtx2['vout'][0]['value']))

        spends1_raw = create_tx(self.nodes[0], coinbase_txids1, node0_address, rawtx1['vout'][0]['value'])
        inputs = [{"txid": coinbase_txids1, "vout": 0}, {"txid": coinbase_txids2, "vout": 0}]
        outputs = {node0_address: rawtx1['vout'][0]['value']/2, node0_address: rawtx1['vout'][0]['value']/2}
        spends2_raw = create_tx_multi_input(self.nodes[0], inputs, outputs)

        self.nodes[0].sendrawtransaction(spends1_raw)
        self.nodes[0].sendrawtransaction(spends1_raw)

        assert_equal(len(self.nodes[0].getrawmempool()), 1)

        error = None
        try:
            self.nodes[0].sendrawtransaction(spends2_raw)
        except JSONRPCException as ex:
            error = ex.error['message']
            assert '258: txn-mempool-conflict' == error, 'Unexpected exception appeared: {}'.format(error)

        assert error, 'Did not raise txt-mempool-conflict exception.'

        self.nodes[0].generate(1)

        error = None
        try:
            self.nodes[0].sendrawtransaction(spends2_raw)
        except JSONRPCException as ex:
            error = ex.error['message']
            assert 'Missing inputs' == error, 'Unexpected exception appeared: {}'.format(error)
        assert error, 'Did not raise txt-mempool-conflict exception.'

        assert_equal(len(self.nodes[0].getrawmempool()), 0)


if __name__ == '__main__':
    MempoolDoubleSpendOneBlock().main()

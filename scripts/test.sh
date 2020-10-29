#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# settings
cleos push action push.sx setparams '[["0.0010 EOS", ["basic.sx", "miner.sx"]]]' -p push.sx

# fund push.sx
cleos transfer eosio push.sx "0.0010 EOS"

# mine
cleos push action push.sx mine '["miner.sx", 123]' -p miner.sx

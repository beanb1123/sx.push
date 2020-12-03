#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# settings
cleos push action push.sx setsettings '[[["basic.sx"], 20]]' -p push.sx

# mine
cleos push action push.sx mine '["miner.sx", 123]' -p miner.sx

# redeem
cleos transfer miner.sx push.sx "1 SXCPU" --contract token.sx
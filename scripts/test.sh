#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# settings
cleos push action token.sx open '["push.sx", "4,SXEOS", "push.sx"]' -p push.sx
cleos push action push.sx setsettings '[[["basic.sx"], 20]]' -p push.sx
cleos transfer eosio push.sx "700000.0000 SXEOS" --contract token.sx

# mine
cleos push action push.sx mine '["miner.sx", 123]' -p miner.sx

# redeem
cleos transfer miner.sx push.sx "1 SXCPU" --contract token.sx

# burn
cleos transfer eosio push.sx "1 SXCPU" --contract token.sx "🔥"
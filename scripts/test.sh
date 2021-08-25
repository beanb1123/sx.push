#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# settings
# cleos push action push.sx setconfig '[[2, 4, 1000, ["4,EOS", "eosio.token"]]]' -p push.sx
cleos push action push.sx setconfig '[[2, 4, 500, ["8,WAX", "eosio.token"]]]' -p push.sx
cleos push action eosio.token open '["push.sx", "4,EOS", "push.sx"]' -p push.sx
cleos push action eosio.token open '["push.sx", "8,WAX", "push.sx"]' -p push.sx
cleos push action push.sx update '[]' -p push.sx

# set strategy
cleos push action push.sx setstrategy '["main.sx", "main"]' -p push.sx
cleos push action push.sx setstrategy '["null.sx", "default"]' -p push.sx
cleos push action push.sx setstrategy '["fallback.sx", "fallback"]' -p push.sx

# init SXEOS from vaults & send to push.sx
cleos transfer sx push.sx "20.00000000 WAX" "main.sx";

# mine
cleos push action push.sx mine '["myaccount", 123]' -p myaccount
cleos push action push.sx mine '["myaccount", 456]' -p myaccount
cleos push action push.sx mine '["myaccount", 789]' -p myaccount

# mine2
cleos push action push.sx mine2 '["myaccount", "0ba1b5115017eec99801b31cb33d2dd43ea2daf1ef7a2e99a6b3fd70ab540499"]' -p myaccount
cleos push action push.sx mine2 '["myaccount", "f22a7c8a871ee3ca76cf734474c172a4dc4072614bce0d7b765f34ae4e3984fb"]' -p myaccount
cleos push action push.sx mine2 '["myaccount", "10b38b9f38fbbd7f63ed5a04cb12d99b2a527a7dce4da84836df64e6debe1e8e"]' -p myaccount

# # redeem
# cleos transfer myaccount push.sx "1 SXCPU" --contract push.sx

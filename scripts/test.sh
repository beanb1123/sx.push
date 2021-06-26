#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# init SXEOS from vaults & send to push.sx
cleos transfer eosio push.sx "0.0100 EOS"

# settings
# cleos push action push.sx setsettings '[[["basic.sx"], 20]]' -p push.sx
cleos push action push.sx setconfig '[[2, 4, 500]]' -p push.sx
cleos push action push.sx update '[]' -p push.sx

# mine
cleos push action push.sx mine '["myaccount", 123]' -p myaccount
cleos push action push.sx mine '["myaccount", 456]' -p myaccount
cleos push action push.sx mine '["myaccount", 789]' -p myaccount

# redeem
cleos transfer myaccount push.sx "1 SXCPU" --contract push.sx

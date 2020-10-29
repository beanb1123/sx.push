#!/bin/bash

# compile
eosio-cpp push.sx.cpp -I ../

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# create account
cleos create account eosio push.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio miner.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio basic.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

# set permission
cleos set account permission push.sx active --add-code

# deploy
cleos set contract push.sx . push.sx.wasm push.sx.abi

# settings
cleos push action push.sx setparams '[["0.0010 EOS", ["basic.sx", "miner.sx"]]]' -p push.sx

# mine
cleos push action push.sx mine '["miner.sx", 123]' -p miner.sx

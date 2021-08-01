#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# create account
cleos create account eosio push.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio null.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio main.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio fallback.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio token.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio fee.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio myaccount EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio toaccount EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

# contract
cleos set contract push.sx . push.sx.wasm push.sx.abi
cleos set contract eosio.token ../eosio.token eosio.token.wasm eosio.token.abi
cleos set contract token.sx ../eosio.token eosio.token.wasm eosio.token.abi

# @eosio.code permission
cleos set account permission push.sx active --add-code

# create EOS token
cleos push action eosio.token create '["eosio", "100000000.0000 EOS"]' -p eosio.token
cleos push action eosio.token issue '["eosio", "5000000.0000 EOS", "init"]' -p eosio
cleos transfer eosio sx "500 EOS"

cleos push action eosio.token create '["eosio", "100000000.00000000 WAX"]' -p eosio.token
cleos push action eosio.token issue '["eosio", "5000000.00000000 WAX", "init"]' -p eosio
cleos transfer eosio sx "500 WAX"

# Legacy SXCPU
cleos push action token.sx create '["token.sx", "100000000.0000 SXCPU"]' -p token.sx
# cleos push action token.sx issue '["token.sx", "0.0000 SXCPU", "init"]' -p token.sx

# create SXCPU & SXEOS token
cleos push action push.sx create '["push.sx", "100000000.0000 SXCPU"]' -p push.sx
# cleos push action push.sx issue '["push.sx", "0.0000 SXCPU", "init"]' -p push.sx
# cleos transfer push.sx myaccount "500 SXCPU" --contract push.sx
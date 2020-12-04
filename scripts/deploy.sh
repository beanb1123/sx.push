#!/bin/bash

# unlock wallet
cleos wallet unlock --password $(cat ~/eosio-wallet/.pass)

# create account
cleos create account eosio push.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio myaccount EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio toaccount EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio basic.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio miner.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos create account eosio token.sx EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

# contract
cleos set contract push.sx . push.sx.wasm push.sx.abi
cleos set contract eosio.token ../eosio.token eosio.token.wasm eosio.token.abi
cleos set contract token.sx ../eosio.token eosio.token.wasm eosio.token.abi

# @eosio.code permission
cleos set account permission push.sx active --add-code

# create EOS token
cleos push action eosio.token create '["eosio", "100000000.0000 EOS"]' -p eosio.token
cleos push action eosio.token issue '["eosio", "5000000.0000 EOS", "init"]' -p eosio

# create SXCPU & SXEOS token
cleos push action token.sx create '["eosio", "461168601842738.7903 SXEOS"]' -p token.sx
cleos push action token.sx create '["push.sx", "461168601842738.7903 SXCPU"]' -p token.sx
cleos push action token.sx issue '["eosio", "700000.0000 SXEOS", "init"]' -p eosio


#!/bin/bash

# build
# blanc++ push.sx.cpp -I include -I .
eosio-cpp push.sx.cpp -I include -I .
cleos set contract push.sx . push.sx.wasm push.sx.abi

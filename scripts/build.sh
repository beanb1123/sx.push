#!/bin/bash

# build
eosio-cpp push.sx.cpp -I ../ -I include
cleos set contract push.sx . push.sx.wasm push.sx.abi

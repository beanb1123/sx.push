#!/bin/bash

# build
blanc++ push.sx.cpp -I include -I .
cleos set contract push.sx . push.sx.wasm push.sx.abi

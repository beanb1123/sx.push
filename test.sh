while true; do
    cleos -u https://eos.eosn.io push action push.sx test '[]' -p miner.sx -p push.sx
    cleos -u https://eos.eosn.io get info | jq .head_block_producer
    # cleos -u https://eos.eosn.io transfer test.sx push.sx "1.0000 SXCPU" --contract push.sx
	sleep 1
done
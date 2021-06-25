while true; do
    # cleos -u https://eos.eosn.io push action push.sx test '[]' -p push.sx
    cleos -u https://eos.eosn.io get info
    cleos -u https://eos.eosn.io transfer miner.sx push.sx "1.0000 SXCPU" --contract token.sx
	sleep 1
done
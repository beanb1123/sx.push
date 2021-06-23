#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using namespace std;

namespace eosio {
    class [[eosio::contract("eosio.msig")]] msig : public eosio::contract {
    public:
        using contract::contract;

        [[eosio::action]]
        void exec( const name proposer, const name proposal_name, const name executer );

        using exec_action = eosio::action_wrapper<"exec"_n, &eosio::msig::exec>;
    };
}
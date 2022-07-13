#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;

namespace sx {

class [[eosio::contract("push.sx")]] push : public contract {
public:
    using contract::contract;

    // BASE SYMBOLS
    const extended_symbol SXCPU{{"SXCPU", 4}, "push.sx"_n };
    const extended_symbol BOXBKS{{"BOXBKS", 0}, "lptoken.defi"_n };
    const extended_symbol LEGACY_SXCPU{{"SXCPU", 4}, "token.sx"_n };
    const extended_symbol EOS{{"EOS", 4}, "eosio.token"_n };
    const extended_symbol WAX{{"WAX", 8}, "eosio.token"_n };

    // CONSTANTS
    const set<name> PRIORITY_TYPES = set<name>{"low"_n, "high"_n};

    /**
     * ## TABLE `state`
     *
     * - `{time_point} last` - last timestamp executed mine action
     * - `{uint64_t} current` - current number of transactions per block
     * - `{uint64_t} total` - total number of transactions
     * - `{extended_asset} balance` - reward balance
     * - `{extended_asset} supply` - supply of CPU tokens
     *
     * ### example
     *
     * ```json
     * {
     *   "last": "2020-12-04T01:38:02.000"
     *   "current": 1,
     *   "total": 25941,
     *   "balance": { "quantity": "15050.6506 EOS", "contract": "eosio.token" },
     *   "supply": { "quantity": "5181544.6565 SXCPU", "contract": "push.sx" }
     * }
     * ```
     */
    struct [[eosio::table("state")]] state_row {
        time_point      last;
        uint64_t        current = 0;
        uint64_t        total = 0;
        extended_asset  balance;
        extended_asset  supply;
    };
    typedef eosio::singleton< "state"_n, state_row > state_table;

    struct [[eosio::table("claims")]] claims_row {
        name            executor;
        time_point      created_at;
        extended_asset  balance;

        uint64_t primary_key() const { return executor.value; }
    };
    typedef eosio::multi_index< "claims"_n, claims_row> claims_table;

    struct [[eosio::table("strategies")]] strategies_row {
        name            strategy;
        name            type;
        time_point      last;
        extended_asset  balance;

        uint64_t primary_key() const { return strategy.value; }
        uint64_t by_type() const { return type.value; }
        uint64_t by_last() const { return last.sec_since_epoch(); }
        uint64_t by_balance() const { return balance.quantity.amount; }
    };
    typedef eosio::multi_index< "strategies"_n, strategies_row,
        indexed_by< "bytype"_n, const_mem_fun<strategies_row, uint64_t, &strategies_row::by_type> >,
        indexed_by< "bylast"_n, const_mem_fun<strategies_row, uint64_t, &strategies_row::by_last> >,
        indexed_by< "bybalance"_n, const_mem_fun<strategies_row, uint64_t, &strategies_row::by_balance> >
    > strategies_table;

    /**
     * ## ACTION `push`
     *
     * Executor pushes action and receive reward
     *
     * - **authority**: `executor`
     *
     * ### params
     *
     * - `{name} executor` - executor (account receives push rewards)
     * - `{uint64_t} nonce` - arbitrary number
     *
     * ### Example
     *
     * ```bash
     * $ cleos push action push.sx mine '["myaccount", 123]' -p myaccount
     * ```
     */
    [[eosio::action]]
    void mine( const name executor, uint64_t nonce );

    [[eosio::action]]
    void init( const extended_symbol balance_ext_sym );

    // [[eosio::action]]
    // void setconfig( const config_row config );

    [[eosio::action]]
    void setstrategy( const name strategy, const name type );

    [[eosio::action]]
    void delstrategy( const name strategy );

    [[eosio::action]]
    void pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine );

    [[eosio::action]]
    void claimlog( const name executor, const asset claimed, const name first_authorizer );

    [[eosio::action]]
    void claim( const name executor );

    [[eosio::action]]
    void reset( const name table );

    [[eosio::action]]
    void deposit( const name from, const name strategy, const extended_asset payment, const extended_asset deposit );

    /**
     * Notify contract when any token transfer notifiers relay contract
     */
    [[eosio::on_notify("*::transfer")]]
    void on_transfer( const name from, const name to, const asset quantity, const std::string memo );

    [[eosio::action]]
    void ontransfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo );

    // action wrapper
    using mine_action = eosio::action_wrapper<"mine"_n, &sx::push::mine>;
    using ontransfer_action = eosio::action_wrapper<"ontransfer"_n, &sx::push::ontransfer>;
    using init_action = eosio::action_wrapper<"init"_n, &sx::push::init>;
    using pushlog_action = eosio::action_wrapper<"pushlog"_n, &sx::push::pushlog>;
    using claimlog_action = eosio::action_wrapper<"claimlog"_n, &sx::push::claimlog>;
    using deposit_action = eosio::action_wrapper<"deposit"_n, &sx::push::deposit>;

private:
    // eosio.token helper
    void transfer( const name from, const name to, const extended_asset value, const string memo );
    void retire( const extended_asset value, const string memo );
    void issue( const extended_asset value, const string memo );

    // issue/redeem SXCPU
    vector<name> get_strategies( const name type );
    name get_strategy( const name type, const uint64_t random );

    void handle_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo );
    extended_asset calculate_retire( const asset payment );
    int64_t calculate_issue( const extended_asset payment );

    name get_first_authorizer( const name executor ) {
        char tx_buffer[eosio::transaction_size()];
        eosio::read_transaction( tx_buffer, eosio::transaction_size() );
        const std::vector<char> trx_vector(tx_buffer, tx_buffer + sizeof tx_buffer / sizeof tx_buffer[0]);
        transaction trx = eosio::unpack<transaction>(trx_vector);
        action first_action = trx.actions[0];

        for ( auto auth: first_action.authorization ) {
            return auth.actor;
        }
        return executor;
    };

    void exec( const name proposer, const name proposal_name );
    void add_strategy( const name strategy, const extended_asset ext_quantity );
    void add_claim( const name executor, const extended_asset claim );
    void interval_claim( const name executor );

    template <typename T>
    bool erase_table( T& table );
};

}
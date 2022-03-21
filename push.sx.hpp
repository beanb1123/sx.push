#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;

static constexpr extended_symbol SXCPU{{"SXCPU", 4}, "push.sx"_n };
static constexpr extended_symbol LEGACY_SXCPU{{"SXCPU", 4}, "token.sx"_n };
static constexpr extended_symbol EOS{{"EOS", 4}, "eosio.token"_n };
static constexpr extended_symbol WAX{{"WAX", 8}, "eosio.token"_n };

namespace sx {

class [[eosio::contract("push.sx")]] push : public contract {
public:
    using contract::contract;

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
     *   "total": 25941
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

    struct [[eosio::table("config")]] config_row {
        uint64_t            split = 4; // split (25/75)
        uint64_t            frequency = 20; // frequency (1/20)
        uint64_t            interval = 2500; // 2500ms interval time
        extended_symbol     ext_sym;
    };
    typedef eosio::singleton< "config"_n, config_row > config_table;

    struct [[eosio::table("claims")]] claims_row {
        name            owner;
        time_point      updated_at;
        extended_asset  balance;

        uint64_t primary_key() const { return owner.value; }
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
    void mine2( const name executor, const checksum256 digest );

    [[eosio::action]]
    void update();

    [[eosio::action]]
    void setconfig( const config_row config );

    [[eosio::action]]
    void setstrategy( const name strategy, const optional<name> type );

    [[eosio::action]]
    void pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine );

    [[eosio::action]]
    void claim( const name owner );

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
    using mine2_action = eosio::action_wrapper<"mine2"_n, &sx::push::mine2>;
    using ontransfer_action = eosio::action_wrapper<"ontransfer"_n, &sx::push::ontransfer>;
    using update_action = eosio::action_wrapper<"update"_n, &sx::push::update>;
    using pushlog_action = eosio::action_wrapper<"pushlog"_n, &sx::push::pushlog>;
    using deposit_action = eosio::action_wrapper<"deposit"_n, &sx::push::deposit>;

private:
    // eosio.token helper
    void transfer( const name from, const name to, const extended_asset value, const string memo );
    void retire( const extended_asset value, const string memo );
    void issue( const extended_asset value, const string memo );

    // issue/redeem SXCPU
    name get_strategy( const name type, const bool required = false );
    vector<name> get_strategies( const name type );

    void handle_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo );
    extended_asset calculate_retire( const asset payment );
    int64_t calculate_issue( const extended_asset payment );

    // extended_symbol get_SXCPU();

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
    void add_strategy( const name strategy, const int64_t amount, const name type = ""_n );
    void add_claim( const name owner, const extended_asset claim );

    template <typename T>
    bool erase_table( T& table );
};

}
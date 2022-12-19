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
    const extended_symbol EOS{{"EOS", 4}, "eosio.token"_n };
    // const extended_symbol BOXBKS{{"BOXBKS", 0}, "lptoken.defi"_n };
    // const extended_symbol LEGACY_SXCPU{{"SXCPU", 4}, "token.sx"_n };
    // const extended_symbol EOS{{"EOS", 4}, "eosio.token"_n };
    // const extended_symbol WAX{{"WAX", 8}, "eosio.token"_n };

    // CONSTANTS
    const set<name> PRIORITY_TYPES = set<name>{"low"_n, "high"_n, "fallback"_n};
    const name FALLBACK_STRATEGY = "fast.sx"_n;
    const vector<name> LOW_STRATEGIES = {
        "eosnationdsp"_n,
        "eosnationftw"_n,
        "fee.sx"_n,
        "heavy.sx"_n,
        "oracle.sx"_n,
        "proxy4nation"_n,
        "unpack.gems"_n
    };
    const vector<name> HIGH_STRATEGIES = {
        "basic.sx"_n,
        "hft.sx"_n,
        "liq.sx"_n,
        "top.sx"_n
    };

    // /**
    //  * ## TABLE `issuance`
    //  *
    //  * - `{time_point_sec} epoch` - epoch timestamp
    //  * - `{uint32_t} interval` - epoch interval
    //  * - `{asset} rate` - issuance rate
    //  *
    //  * ### example
    //  *
    //  * ```json
    //  * {
    //  *   "epoch": "2020-12-04T01:38:02"
    //  *   "interval": 600,
    //  *   "rate": "100.0000 SXCPU"
    //  * }
    //  * ```
    //  */
    // struct [[eosio::table("issuance")]] issuance_row {
    //     time_point_sec      epoch;
    //     uint32_t            interval = 600;
    //     asset               rate = asset{100'0000, symbol{"SXCPU", 4}};
    // };
    // typedef eosio::singleton< "issuance"_n, issuance_row > issuance_table;

    struct [[eosio::table("miners")]] miners_row {
        name                first_authorizer;
        uint64_t            rank;
        extended_asset      balance;

        uint64_t primary_key() const { return first_authorizer.value; }
    };
    typedef eosio::multi_index< "miners"_n, miners_row> miners_table;

    // struct [[eosio::table("strategies")]] strategies_row {
    //     name                strategy;
    //     uint64_t            priority;
    //     asset               fee;
    //     name                last_executor;
    //     extended_asset      balance;

    //     uint64_t primary_key() const { return strategy.value; }
    // };
    // typedef eosio::multi_index< "strategies"_n, strategies_row> strategies_table;

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

    // [[eosio::action]]
    // void setstrategy( const name strategy, const uint64_t priority, const asset fee );

    [[eosio::action]]
    void setminer( const name first_authorizer, const uint64_t rank );

    [[eosio::action]]
    void delminer( const name first_authorizer );

    // [[eosio::action]]
    // void delstrategy( const name strategy );

    // [[eosio::action]]
    // void setissuance( const uint32_t interval, const asset rate );

    // [[eosio::action]]
    // void pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine );

    [[eosio::action]]
    void claimlog( const name first_authorizer, const asset claimed );

    [[eosio::action]]
    void reset( const name table );

    [[eosio::action]]
    void setminers( const name table );

    // [[eosio::action]]
    // void deposit( const name from, const name strategy, const extended_asset payment, const extended_asset deposit );

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
    // using init_action = eosio::action_wrapper<"init"_n, &sx::push::init>;
    // using pushlog_action = eosio::action_wrapper<"pushlog"_n, &sx::push::pushlog>;
    using claimlog_action = eosio::action_wrapper<"claimlog"_n, &sx::push::claimlog>;
    // using deposit_action = eosio::action_wrapper<"deposit"_n, &sx::push::deposit>;

private:
    // eosio.token helper
    void transfer( const name from, const name to, const extended_asset value, const string memo );
    void retire( const extended_asset value, const string memo );
    void issue( const extended_asset value, const string memo );

    // issue/redeem SXCPU
    vector<name> get_strategies( const name type );
    name get_strategy( const name type, const uint64_t random );

    void handle_eos_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo );
    void handle_sxcpu_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo );
    // extended_asset calculate_retire( const asset payment );
    // int64_t calculate_issue( const extended_asset payment );

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

    // void trigger_issuance();
    // void add_strategy( const name strategy, const extended_asset ext_quantity );
    // void deduct_strategy( name executor, const name strategy );
    void send_rewards(const name first_authorizer );

    // void add_claim( const name executor, const extended_asset claim );
    // void interval_claim( const name executor );
    // void sucess_miner( const name first_authorizer );
    // void check_sucess_miner( const name first_authorizer );

    template <typename T>
    bool erase_table( T& table );
};

}
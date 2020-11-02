#include <eosio/eosio.hpp>

namespace sx {

using eosio::name;
using eosio::time_point;
using eosio::extended_asset;

class [[eosio::contract("push.sx")]] push : public contract {

public:
    using contract::contract;

    struct [[eosio::table("state")]] state_row {
        time_point      last;
        uint64_t        count = 0;
        asset           rewards = {0, symbol{"SX", 4}};
    };
    typedef eosio::singleton< "state"_n, state_row > state;

    /**
     * ## TABLE `settings`
     *
     * - `{asset} reward` - reward paid to executor deducted from billed contract
     * - `{set<name>} contracts` - list of contracts to notify
     *
     * ### example
     *
     * ```json
     * {
     *   "reward": {"quantity": "0.0050 SX", "contract": "token.sx"},
     *   "contracts": ["basic.sx"]
     * }
     * ```
     */
    struct [[eosio::table("settings")]] params {
        extended_asset      reward = {{50, symbol{"SX", 4}}, "token.sx"_n};
        vector<name>        contracts = {"basic.sx"_n};
    };
    typedef eosio::singleton< "settings"_n, params > settings;

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
    void mine( const name executor, const uint64_t nonce );

    /**
     * ## ACTION `setparams`
     *
     * Set contract settings
     *
     * - **authority**: `get_self()`
     *
     * ### params
     *
     * - `{asset} reward` - reward paid to executor deducted from billed contract
     * - `{set<name>} contracts` - list of contracts to notify
     *
     * ### Example
     *
     * ```bash
     * $ cleos push action push.sx setparams '[["0.0010 EOS", ["basic.sx"]]]' -p push.sx
     * ```
     */
    [[eosio::action]]
    void setparams( const optional<sx::push::params> params );

    // action wrapper
    using mine_action = eosio::action_wrapper<"mine"_n, &sx::push::mine>;
    using setparams_action = eosio::action_wrapper<"setparams"_n, &sx::push::setparams>;
};
}
#include <eosio/eosio.hpp>

namespace sx {

using eosio::name;
using eosio::time_point;

class [[eosio::contract("push.sx")]] push : public contract {

public:
    using contract::contract;

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
     *   "reward": "0.0010 EOS",
     *   "contracts": ["basic.sx"]
     * }
     * ```
     */
    struct [[eosio::table("settings")]] params {
        asset               reward = asset{10, symbol{"EOS", 4}};
        vector<name>        contracts = {"basic.sx"_n};
    };
    typedef eosio::singleton< "settings"_n, params > settings;

    /**
     * ## ACTION `mine`
     *
     * Executor pushes mining action and receive reward
     *
     * - **authority**: `executor`
     *
     * ### params
     *
     * - `{name} executor` - executor (account receive push fee)
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
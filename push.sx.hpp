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
        uint64_t        current = 0;
        uint64_t        total = 0;
    };
    typedef eosio::singleton< "state"_n, state_row > state;

    /**
     * ## TABLE `settings`
     *
     * - `{set<name>} contracts` - list of contracts to notify
     * - `{uint64_t} max_per_block` - maximum amount of transactions per block
     *
     * ### example
     *
     * ```json
     * {
     *   "contracts": ["basic.sx"],
     *   "max_per_block": 20
     * }
     * ```
     */
    struct [[eosio::table("settings")]] settings_row {
        vector<name>        contracts = {"basic.sx"_n};
        uint64_t            max_per_block = 20;
    };
    typedef eosio::singleton< "settings"_n, settings_row > settings;

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
     * - `{set<name>} contracts` - list of contracts to notify
     * - `{uint64_t} max_per_block` - maximum amount of transactions per block
     *
     * ### Example
     *
     * ```bash
     * $ cleos push action push.sx setparams '[[["basic.sx", 20]]]' -p push.sx
     * ```
     */
    [[eosio::action]]
    void setsettings( const optional<sx::push::settings_row> settings );

    // action wrapper
    using mine_action = eosio::action_wrapper<"mine"_n, &sx::push::mine>;
    using setsettings_action = eosio::action_wrapper<"setsettings"_n, &sx::push::setsettings>;
};
}
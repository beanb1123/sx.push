#include <eosio/eosio.hpp>

using namespace eosio;

static constexpr extended_symbol SXEOS{{"SXEOS", 4}, "token.sx"_n};
static constexpr extended_symbol SXCPU{{"SXCPU", 4}, "token.sx"_n};

namespace sx {
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

    /**
     * Notify contract when any token transfer notifiers relay contract
     */
    [[eosio::on_notify("*::transfer")]]
    void on_transfer( const name from, const name to, const asset quantity, const std::string memo );

    // action wrapper
    using mine_action = eosio::action_wrapper<"mine"_n, &sx::push::mine>;
    using setsettings_action = eosio::action_wrapper<"setsettings"_n, &sx::push::setsettings>;
private:
    // eosio.token helper
    void transfer( const name from, const name to, const extended_asset value, const string memo );
    void retire( const extended_asset value, const string memo );
    void issue( const extended_asset value, const string memo );

    // vault
    extended_asset calculate_retire( const asset payment );
};
}
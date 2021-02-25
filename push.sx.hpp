#include <eosio/eosio.hpp>

using namespace eosio;

static constexpr extended_symbol SXCPU{{"SXCPU", 4}, "token.sx"_n };
static constexpr extended_symbol EOS{{"EOS", 4}, "eosio.token"_n };

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

    [[eosio::action]]
    void update();

    /**
     * Notify contract when any token transfer notifiers relay contract
     */
    [[eosio::on_notify("*::transfer")]]
    void on_transfer( const name from, const name to, const asset quantity, const std::string memo );

    // action wrapper
    using mine_action = eosio::action_wrapper<"mine"_n, &sx::push::mine>;
    using setsettings_action = eosio::action_wrapper<"setsettings"_n, &sx::push::setsettings>;
    using update_action = eosio::action_wrapper<"update"_n, &sx::push::update>;
private:
    // eosio.token helper
    void transfer( const name from, const name to, const extended_asset value, const string memo );
    void retire( const extended_asset value, const string memo );
    void issue( const extended_asset value, const string memo );

    // issue/redeem SXCPU
    extended_asset calculate_retire( const asset payment );
    extended_asset calculate_issue( const asset payment );
};
}
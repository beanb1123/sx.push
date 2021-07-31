#include <eosio/transaction.hpp>
#include <eosio.msig/eosio.msig.hpp>
#include <eosio.token/eosio.token.hpp>
#include <push.sx.hpp>

#include "src/helpers.cpp"
#include "include/eosio.token/eosio.token.cpp"

[[eosio::action]]
void sx::push::mine( const name executor, uint64_t nonce )
{
    require_auth( executor );

    // check( false, "disabled");

    sx::push::state_table _state( get_self(), get_self().value );
    sx::push::strategies_table _strategies( get_self(), get_self().value );

    check( _state.exists(), "contract is on going maintenance");

    // set state for throttle rate limits
    auto state = _state.get_or_default();
    const time_point now = current_time_point();
    const int64_t milliseconds = now.time_since_epoch().count() / 1000;
    state.current = now == state.last ? state.current + 1 : 1;
    state.total += 1;
    state.last = now;

    // Configurations
    sx::push::config_table _config( get_self(), get_self().value );
    auto config = _config.get_or_default();
    const uint64_t RATIO_SPLIT = config.split; // split (25/75)
    const uint64_t RATIO_FREQUENCY = config.frequency; // frequency (1/20)
    const uint64_t RATIO_INTERVAL = config.interval; // 500ms interval time
    int64_t RATE = RATIO_INTERVAL * 20; // 1.0000 SXCPU base rate

    // salt numbers
    const uint64_t block_num = current_time_point().time_since_epoch().count() / 500000;
    const uint64_t random = (nonce + block_num + executor.value) % 10000;

    // strategy dispatch
    name strategy;
    auto strategies_index = _strategies.get_index<"bytype"_n>();
    const name strategy_default = strategies_index.get( "default"_n.value, "push::mine: default strategy does not exists").strategy;
    const name strategy_main = strategies_index.get( "main"_n.value, "push::mine: main strategy does not exists").strategy;
    const name strategy_fallback = strategies_index.get( "fallback"_n.value, "push::mine: fallback strategy does not exists").strategy;
    vector<name> secondaries;
    auto lower = strategies_index.lower_bound( "secondary"_n.value );
    auto upper = strategies_index.upper_bound( "secondary"_n.value );

    while ( lower != upper ) {
        if ( !lower->strategy ) break;
        secondaries.push_back( lower->strategy );
        lower++;
    }
    const uint64_t size = secondaries.size() - 1;
    if ( nonce <= size ) {
        strategy = secondaries[nonce];

    } else if ( random <= size ) {
        strategy = secondaries[random];

    // 25% load first-in block transaction
    } else if ( random % RATIO_SPLIT == 0 ) {
        // first transaction is null (or oracle when implemented)
        // 1. Frequency 1/4
        // 2. First transaction
        // 3. 500ms interval
        if ( state.current <= 1 && milliseconds % RATIO_INTERVAL == 0 && random % RATIO_FREQUENCY == 0 ) {
            strategy = strategy_default;
        } else {
            strategy = strategy_main;
        }
    // 75% fallback
    } else {
        strategy = strategy_fallback;
    }

    // set default strategy RATE
    if ( strategy != strategy_default ) RATE = 20'0000;

    // notify strategy
    require_recipient( strategy );

    // mine SXCPU per action
    const extended_asset out = { RATE, SXCPU };
    const name first_authorizer = get_first_authorizer(executor);
    const name to = first_authorizer == "miner.sx"_n ? "miner.sx"_n : executor;

    // issue mining
    issue( out, "mine" );
    transfer( get_self(), to, out, get_self().to_string() );
    state.supply.quantity += out.quantity;
    _state.set(state, get_self());

    // deduct strategy balance
    add_strategy( strategy, -RATE );

    // logging
    sx::push::pushlog_action pushlog( get_self(), { get_self(), "active"_n });
    pushlog.send( executor, first_authorizer, strategy, out.quantity );
}

[[eosio::action]]
void sx::push::pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine )
{
    require_auth( get_self() );
    if ( is_account("cpu.sx"_n) ) require_recipient( "cpu.sx"_n );
    if ( is_account("stats.sx"_n) ) require_recipient( "stats.sx"_n );
}

[[eosio::action]]
void sx::push::update()
{
    require_auth( get_self() );

    sx::push::state_table _state( get_self(), get_self().value );
    sx::push::config_table _config( get_self(), get_self().value );
    auto config = _config.get_or_default();
    auto state = _state.get_or_default();
    state.balance.contract = EOS.get_contract();
    state.balance.quantity = token::get_balance( EOS, get_self() );
    state.supply.contract = SXCPU.get_contract();
    state.supply.quantity = token::get_supply( SXCPU ) + token::get_supply( LEGACY_SXCPU );
    _state.set( state, get_self() );
}

[[eosio::action]]
void sx::push::setconfig( const config_row config )
{
    require_auth( get_self() );

    sx::push::config_table _config( get_self(), get_self().value );
    auto last_config = _config.get_or_default();
    check( config.split <= 10, "`split` cannot exceed 10");
    check( config.frequency <= 100, "`frequency` cannot exceed 100");
    check( config.interval % 500 == 0, "`interval` must be modulus of 500");
    check( config.split != last_config.split || config.frequency != last_config.frequency || config.interval != last_config.interval, "`config` must was not modified");
    _config.set( config, get_self() );
}

[[eosio::action]]
void sx::push::reset( const name table )
{
    require_auth( get_self() );

    sx::push::strategies_table _strategies( get_self(), get_self().value );
    sx::push::config_table _config( get_self(), get_self().value );

    if ( table == "strategies"_n ) erase_table( _strategies );
    else if ( table == "config"_n ) _config.remove();
    else check( false, "invalid table name");
}


[[eosio::action]]
void sx::push::ontransfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    require_auth( get_self() );

    handle_transfer( from, to, ext_quantity, memo );
}

/**
 * Notify contract when any token transfer notifiers relay contract
 */
[[eosio::on_notify("*::transfer")]]
void sx::push::on_transfer( const name from, const name to, const asset quantity, const string memo )
{
    require_auth( from );

    // ignore outgoing transfers
    if ( to != get_self() ) return;

    handle_transfer( from, to, extended_asset{ quantity, get_first_receiver() }, memo );
}

void sx::push::handle_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    // tables
    sx::push::config_table _config( get_self(), get_self().value );
    sx::push::state_table _state( get_self(), get_self().value );
    check( _state.exists(), "push::handle_transfer: contract is under maintenance");
    check( _state.exists(), "push::handle_transfer: contract is under maintenance");
    auto config = _config.get();
    auto state = _state.get();

    // helpers
    const asset quantity = ext_quantity.quantity;
    const name contract = ext_quantity.contract;
    const extended_symbol ext_sym = { ext_quantity.quantity.symbol, contract };

    // ignore outgoing transfers
    if ( to != get_self() ) return;

    // send EOS/WAX to push.sx (increases value of SXCPU)
    if ( ext_sym == config.ext_sym ) {
        check( from.suffix() == "sx"_n, "push::handle_transfer: invalid account");
        sx::push::update_action update( get_self(), { get_self(), "active"_n });
        update.send();

    // redeem - SXCPU => EOS
    } else if ( ext_sym == SXCPU || ext_sym == LEGACY_SXCPU ) {
        // calculate retire
        const extended_asset out = calculate_retire( quantity );
        retire( ext_quantity, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

        state.balance -= out;
        state.supply.quantity -= quantity;
        _state.set( state, get_self() );

    } else {
        check( false, from.to_string() + ":" + to.to_string() + ":" + contract.to_string() + quantity.to_string() + "invalid incoming transfer");
    }
}

[[eosio::action]]
void sx::push::setstrategy( const name strategy, const optional<name> type )
{
    require_auth( get_self() );
    add_strategy( strategy, 0, *type );
}

void sx::push::add_strategy( const name strategy, const int64_t amount, const name type )
{
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    sx::push::config_table _config( get_self(), get_self().value );
    auto config = _config.get_or_default();

    auto insert = [&]( auto & row ) {
        row.strategy = strategy;
        row.last = current_time_point();
        row.balance.contract = SXCPU.get_contract();
        row.balance.quantity.symbol = SXCPU.get_symbol();
        row.balance.quantity.amount += amount;
        if ( type.value ) row.type = type;
    };
    auto itr = _strategies.find( strategy.value );
    if ( itr == _strategies.end() ) _strategies.emplace( get_self(), insert );
    else _strategies.modify( itr, get_self(), insert );
}

// extended_asset sx::push::calculate_issue( const asset payment )
// {
//     sx::push::state_table _state( get_self(), get_self().value );
//     auto state = _state.get();
//     const int64_t ratio = 20;

//     // initialize reward supply
//     if ( state.supply.quantity.amount == 0 ) return { payment.amount / ratio, state.supply.get_extended_symbol() };

//     // issue & redeem supply calculation
//     // calculations based on fill REX order
//     // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L775-L779
//     const int64_t S0 = state.balance.quantity.amount;
//     const int64_t S1 = S0 + payment.amount;
//     const int64_t R0 = state.supply.quantity.amount;
//     const int64_t R1 = (uint128_t(S1) * R0) / S0;

//     return { R1 - R0, state.supply.get_extended_symbol() };
// }

extended_asset sx::push::calculate_retire( const asset payment )
{
    sx::push::state_table _state( get_self(), get_self().value );
    auto state = _state.get();

    // issue & redeem supply calculation
    // calculations based on add to REX pool
    // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L772
    const int64_t S0 = state.balance.quantity.amount;
    const int64_t R0 = state.supply.quantity.amount;
    const int64_t p  = (uint128_t(payment.amount) * S0) / R0;

    return { p, state.balance.get_extended_symbol() };
}
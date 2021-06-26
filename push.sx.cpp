#include <eosio/transaction.hpp>
#include <eosio.msig/eosio.msig.hpp>
#include <eosio.token/eosio.token.hpp>
#include <push.sx.hpp>

#include "src/helpers.cpp"
#include "include/eosio.token/eosio.token.cpp"

[[eosio::action]]
void sx::push::test()
{
    print('pass');
}

[[eosio::action]]
void sx::push::mine( const name executor, const uint64_t nonce )
{
    require_auth( executor );

    // check( false, "disabled");

    sx::push::state_table _state( get_self(), get_self().value );
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

    // random number
    const vector<uint64_t> nonces = {123, 345, 227, 992, 213, 455, 123, 550, 100};
    const uint64_t salt = nonces[ state.total % nonces.size() ];
    const uint64_t random = (salt + milliseconds / 500 + executor.value + nonce) % 1000;

    // strategy dispatch
    name strategy;
    if ( nonce == 1 || random == 1 ) {
        strategy = "fee.sx"_n;

    // } else if ( nonce == 2 || random == 2 ) {
    //     strategy = "eusd.sx"_n;

    } else if ( nonce == 3 || random == 3 ) {
        strategy = "eosnationftw"_n;

    } else if ( nonce == 4 || random == 4 ) {
        strategy = "unpack.gems"_n;

    // 25% load first-in block transaction
    } else if ( random % RATIO_SPLIT == 0 ) {
        // first transaction is null (or oracle when implemented)
        // 1. Frequency 1/4
        // 2. First transaction
        // 3. 500ms interval
        if ( state.current <= 1 && milliseconds % RATIO_INTERVAL == 0 && random % RATIO_FREQUENCY == 0 ) {
            strategy = "null.sx"_n;
        } else {
            strategy = "basic.sx"_n;
            // strategy = "hft.sx"_n;
        }
    // 75% fallback
    } else {
        strategy = "hft.sx"_n;
    }

    // set default strategy RATE
    if ( strategy != "null.sx"_n ) RATE = 20'0000;

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
    add_strategy( strategy, -out );

    // logging
    sx::push::pushlog_action pushlog( get_self(), { get_self(), "active"_n });
    pushlog.send( executor, first_authorizer, strategy, out.quantity );
}

[[eosio::action]]
void sx::push::pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine )
{
    require_auth( get_self() );
    require_recipient( "cpu.sx"_n );
    require_recipient( "stats.sx"_n );
}

[[eosio::action]]
void sx::push::update()
{
    require_auth( get_self() );

    sx::push::state_table _state( get_self(), get_self().value );
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
    // helpers
    const asset quantity = ext_quantity.quantity;
    const name contract = ext_quantity.contract;
    const extended_symbol ext_sym = { ext_quantity.quantity.symbol, contract };

    // ignore outgoing transfers
    if ( to != get_self() ) return;

    // check( from.suffix() == "sx"_n, "must be *.sx account");

    // send EOS to push.sx (increases value of SXCPU)
    if ( ext_sym == EOS ) {
        check( from.suffix() == "sx"_n, "push.sx::on_notify: accepting EOS must be *.sx account");
        sx::push::update_action update( get_self(), { get_self(), "active"_n });
        update.send();

    // redeem - SXCPU => EOS
    } else if ( ext_sym == SXCPU || ext_sym == LEGACY_SXCPU ) {
        // calculate retire
        const extended_asset out = calculate_retire( quantity );
        retire( ext_quantity, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

        // update state
        sx::push::state_table _state( get_self(), get_self().value );
        check( _state.exists(), "contract is under maintenance");
        auto state = _state.get();

        state.balance -= out;
        state.supply.quantity -= quantity;
        _state.set( state, get_self() );

    } else {
        check( false, from.to_string() + ":" + to.to_string() + ":" + contract.to_string() + quantity.to_string() + "invalid incoming transfer");
    }
}

void sx::push::add_strategy( const name strategy, const extended_asset value )
{
    sx::push::push_table _push( get_self(), get_self().value );

    auto insert = [&]( auto & row ) {
        row.strategy = strategy;
        row.last = current_time_point();
        if ( row.balance.contract ) row.balance.quantity += value.quantity;
        else row.balance = value;
        row.balance.contract = value.contract;
    };
    auto itr = _push.find( strategy.value );
    if ( itr == _push.end() ) _push.emplace( get_self(), insert );
    else _push.modify( itr, get_self(), insert );
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
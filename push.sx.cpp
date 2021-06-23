#include <sx.swap/swap.sx.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio/transaction.hpp>
#include <eosio.msig/eosio.msig.hpp>

#include "push.sx.hpp"
#include "src/helpers.cpp"

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
    state.supply += out;
    _state.set(state, get_self());

    // deduct strategy balance
    add_balance( strategy, -out );

    // logging
    sx::push::pushlog_action pushlog( get_self(), { get_self(), "active"_n });
    pushlog.send( executor, first_authorizer, strategy, out.quantity );
}

// [[eosio::action]]
// void sx::push::exec2()
// {
//     require_auth( get_self() );

//     // exec( "eosnationftw"_n, "transfersafe"_n );

//     eosio::msig::exec_action exec( "eosio.msig"_n, { get_self(), "active"_n });
//     exec.send( "eosnationftw"_n, "transfersafe"_n, get_self() );
// }

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
    state.balance = { eosio::token::get_balance( "eosio.token"_n, get_self(), symbol_code{"EOS"} ), "eosio.token"_n };
    state.supply = { eosio::token::get_supply( "token.sx"_n, symbol_code{"SXCPU"} ), "token.sx"_n };
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

/**
 * Notify contract when any token transfer notifiers relay contract
 */
[[eosio::on_notify("*::transfer")]]
void sx::push::on_transfer( const name from, const name to, const asset quantity, const string memo )
{
    // authenticate incoming `from` account
    require_auth( from );

    // state
    sx::push::state_table _state( get_self(), get_self().value );
    check( _state.exists(), "contract is under maintenance");
    auto state = _state.get();

    // helpers
    const name contract = get_first_receiver();
    const extended_symbol ext_sym = { quantity.symbol, contract };

    // ignore outgoing transfers
    if ( to != get_self() || from == "vaults.sx"_n ) return;

    // check( from.suffix() == "sx"_n, "must be *.sx account");

    // send EOS to push.sx (increases value of SXCPU)
    if ( ext_sym == EOS ) {
        check( from.suffix() == "sx"_n, "push.sx::on_notify: accepting EOS must be *.sx account");
        sx::push::update_action update( get_self(), { get_self(), "active"_n });
        update.send();

    // redeem - SXCPU => EOS
    } else if ( ext_sym == SXCPU ) {
        // calculate retire
        const extended_asset out = calculate_retire( quantity );
        retire( { quantity, contract }, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

        // update state
        state.balance -= out;
        state.supply -= { quantity, contract };
        _state.set( state, get_self() );

    } else {
        check( false, "invalid incoming transfer");
    }
}

void sx::push::add_balance( const name strategy, const extended_asset value )
{
    sx::push::push_table _push( get_self(), get_self().value );

    auto insert = [&]( auto & row ) {
        row.strategy = strategy;
        row.last = current_time_point();
        if ( row.balance.contract ) row.balance += value;
        else row.balance = value;
    };
    auto itr = _push.find( strategy.value );
    if ( itr == _push.end() ) _push.emplace( get_self(), insert );
    else _push.modify( itr, get_self(), insert );
}

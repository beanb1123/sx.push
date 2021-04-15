#include <sx.swap/swap.sx.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio/transaction.hpp>

#include "push.sx.hpp"
#include "src/helpers.cpp"

[[eosio::action]]
void sx::push::mine( const name executor, const uint64_t nonce )
{
    require_auth( executor );

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
    const uint64_t RATIO_A = config.a; // split (25/75)
    const uint64_t RATIO_B = config.b; // frequency (1/20)
    const uint64_t RATIO_C = config.c; // 2500ms interval time
    int64_t RATE = RATIO_C * 20; // 5.0000 SXCPU base rate

    // random number
    const vector<uint64_t> nonces = {123, 345, 227, 992, 213, 455, 123, 550, 100};
    const uint64_t salt = nonces[ state.total % nonces.size() ];
    const uint64_t random = (salt + milliseconds / 500 + executor.value + nonce) % 1000;

    // strategy dispatch
    name strategy;
    if ( nonce == 1 || random == 1 ) {
        strategy = "fee.sx"_n;

    } else if ( nonce == 2 || random == 2 ) {
        strategy = "eusd.sx"_n;

    } else if ( nonce == 3 || random == 3 ) {
        strategy = "eosnationftw"_n;

    } else if ( nonce == 4 || random == 4 ) {
        strategy = "unpack.gems"_n;

    // 25% load first-in block transaction
    } else if ( random % RATIO_A == 0 ) {
        // first transaction is null (or oracle when implemented)
        // 1. Frequency 1/4
        // 2. First transaction
        // 3. 2500ms interval
        if ( state.current <= 1 && milliseconds % RATIO_C == 0 && random % RATIO_B == 0 ) {
            strategy = "null.sx"_n;
        } else {
            strategy = "basic.sx"_n;
            RATE = 20'0000;
        }
    // 75% fallback
    } else {
        strategy = "hft.sx"_n;
        RATE = 20'0000;
    }

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
    check( config.a <= 10, "`a` cannot exceed 10");
    check( config.b <= 100, "`b` cannot exceed 100");
    check( config.c % 500 == 0, "`c` must be modulus of 500");
    check( config.a != last_config.a || config.b != last_config.b || config.c != last_config.c, "`config` must was not modified");
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

#include <sx.swap/swap.sx.hpp>
#include <eosio.token/eosio.token.hpp>

#include "push.sx.hpp"
#include "src/helpers.cpp"

[[eosio::action]]
void sx::push::mine( const name executor, const uint64_t nonce )
{
    require_auth( executor );

    sx::push::settings _settings( get_self(), get_self().value );
    sx::push::state_table _state( get_self(), get_self().value );
    check( _settings.exists(), "contract is on going maintenance");

    // set state for throttle rate limits
    auto state = _state.get_or_default();
    const time_point now = current_time_point();
    state.current = now == state.last ? state.current + 1 : 1;
    state.total += 1;
    state.last = now;

    // Configurations
    const uint64_t RATIO_A = 4;
    const uint64_t RATIO_B = 50;
    int64_t RATE = 1'0000;

    // 1 hour
    if ( nonce == 1 ) {
        require_recipient( "fee.sx"_n );

    // 1 minute
    } else if ( nonce == 2 ) {
        require_recipient( "eusd.sx"_n );

    // 25% load first-in block transaction
    } else if ( nonce % RATIO_A == 0 ) {
        // first transaction is null (or oracle when implemented)
        if ( nonce % RATIO_B == 0 && state.current <= 1 ) require_recipient( "null.sx"_n );
        else {
            require_recipient( "basic.sx"_n );
            RATE = 10'0000;
        }

    // 75% fallback
    } else {
        require_recipient( "hft.sx"_n );
        RATE = 10'0000;
    }

    // notify CPU
    require_recipient( "cpu.sx"_n );

    // mine SXCPU per action
    const extended_asset out = { RATE, SXCPU };
    issue( out, "mine" );
    transfer( get_self(), executor, out, get_self().to_string() );
    state.supply += out;
    _state.set(state, get_self());
}

[[eosio::action]]
void sx::push::setsettings( const optional<sx::push::settings_row> settings )
{
    require_auth( get_self() );

    sx::push::settings _settings( get_self(), get_self().value );
    sx::push::state_table _state( get_self(), get_self().value );

    _settings.set( *settings, get_self() );
    update();
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

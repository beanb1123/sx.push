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

    // select random contract from nonce
    // const vector<name> contracts = _settings.get().contracts;
    // check( contracts.size(), "no contracts available at the moment");
    // const name contract = contracts[nonce % contracts.size()];

    // mine 1 SXCPU per action
    const extended_asset out = { 10000, { SXCPU, TOKEN_CONTRACT } };
    issue( out, "mine" );
    transfer( get_self(), executor, out, get_self().to_string() );

    // set state for throttle rate limits
    auto state = _state.get_or_default();
    const time_point now = current_time_point();
    state.current = now == state.last ? state.current + 1 : 1;
    state.total += 1;
    state.last = now;
    state.supply += out;
    _state.set(state, get_self());

    const uint64_t RATIO = 4;

    // 25% load first-in block transaction
    if ( state.current <= 1 && nonce % RATIO == 0 ) {
        require_recipient( "null.sx"_n );
    // 75% high frequency
    } else if ( nonce % RATIO != 0 ) {
        require_recipient( "hft.sx"_n );
    // hourly contracts
    } else if ( nonce == 1 ) {
        require_recipient( "fee.sx"_n );
    // 10 minute contracts
    } else if ( nonce == 2 ) {
        require_recipient( "usdx.sx"_n );
    // fallback to basic.sx
    } else {
        require_recipient( "basic.sx"_n );
    }
}

[[eosio::action]]
void sx::push::setsettings( const optional<sx::push::settings_row> settings )
{
    require_auth( get_self() );

    sx::push::settings _settings( get_self(), get_self().value );
    sx::push::state_table _state( get_self(), get_self().value );

    // clear table if settings is `null`
    if ( !settings ) {
        _settings.remove();
        _state.remove();
        return;
    }
    _settings.set( *settings, get_self() );
    update();
}

[[eosio::action]]
void sx::push::update()
{
    require_auth( get_self() );

    sx::push::state_table _state( get_self(), get_self().value );
    auto state = _state.get_or_default();
    state.balance = { eosio::token::get_balance( TOKEN_CONTRACT, get_self(), SXEOS.code() ), TOKEN_CONTRACT };
    state.supply = { eosio::token::get_supply( TOKEN_CONTRACT, SXCPU.code() ), TOKEN_CONTRACT };
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

    // helpers
    const name contract = get_first_receiver();
    const symbol sym = quantity.symbol;

    // ignore outgoing transfers
    if ( to != get_self() || from == "vaults.sx"_n ) return;

    // validate input
    check( contract == TOKEN_CONTRACT || contract == "eosio.token"_n, "invalid token contract");

    // send EOS to SX Vaults (increases value of SXCPU)
    if ( sym == EOS ) {
        // mint SXEOS for contract
        transfer( get_self(), "vaults.sx"_n, { quantity, contract }, get_self().to_string() );

        // update newly received SXEOS balance
        sx::push::update_action update( get_self(), { get_self(), "active"_n });
        update.send();

    // redeem - SXCPU => SXEOS
    } else if ( sym == SXCPU ) {
        // state
        sx::push::state_table _state( get_self(), get_self().value );
        check( _state.exists(), "contract is under maintenance");
        auto state = _state.get();

        // calculate retire
        const extended_asset out = calculate_retire( quantity );
        retire( { quantity, contract }, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

        // update state
        state.balance -= out;
        state.supply -= { quantity, contract };
        _state.set( state, get_self() );

    } else {
        check( false, "incoming transfer asset symbol not supported");
    }
}

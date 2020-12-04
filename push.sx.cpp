#include <sx.swap/swap.sx.hpp>
#include <eosio.token/eosio.token.hpp>

#include "push.sx.hpp"

[[eosio::action]]
void sx::push::mine( const name executor, const uint64_t nonce )
{
    require_auth( executor );

    sx::push::settings _settings( get_self(), get_self().value );
    sx::push::state _state( get_self(), get_self().value );
    check( _settings.exists(), "contract is on going maintenance");

    // select random contract from nonce
    const vector<name> contracts = _settings.get().contracts;
    check( contracts.size(), "no contracts available at the moment");
    const name contract = contracts[nonce % contracts.size()];
    require_recipient( contract );

    // push mine action
    if ( executor != get_self() ) {
        sx::push::mine_action mine( contract, { get_self(), "active"_n });
        mine.send( executor, nonce );

        // mine 1 SXCPU per action
        const extended_asset out = { 10000, { SXCPU, TOKEN_CONTRACT } };
        issue( out, "mine" );
        transfer( get_self(), executor, out, get_self().to_string() );
    }

    // record state (TO-DO can be used to throttle rate limits)
    auto state = _state.get_or_default();
    const time_point now = current_time_point();
    state.current = now == state.last ? state.current + 1 : 1;
    state.total += 1;
    state.last = now;
    _state.set(state, get_self());

}

[[eosio::action]]
void sx::push::setsettings( const optional<sx::push::settings_row> settings )
{
    require_auth( get_self() );
    sx::push::settings _settings( get_self(), get_self().value );
    sx::push::state _state( get_self(), get_self().value );

    // clear table if settings is `null`
    if ( !settings ) {
        _settings.remove();
        _state.remove();
        return;
    }
    _settings.set( *settings, get_self() );
}

/**
 * Notify contract when any token transfer notifiers relay contract
 */
[[eosio::on_notify("*::transfer")]]
void sx::push::on_transfer( const name from, const name to, const asset quantity, const string memo )
{
    // authenticate incoming `from` account
    require_auth( from );
    const name contract = get_first_receiver();
    const symbol sym = quantity.symbol;

    // ignore outgoing transfers
    if ( to != get_self() ) return;

    // validate input
    check( contract == TOKEN_CONTRACT, "invalid token contract");

    // redeem - SXCPU => SXEOS
    if ( sym == SXCPU ) {
        const extended_asset out = calculate_retire( quantity );
        retire( { quantity, contract }, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

    // purchase - SXEOS => SXCPU
    } else if ( sym == SXEOS ) {
        const extended_asset out = calculate_issue( quantity );
        issue( out, "issue" );
        transfer( get_self(), from, out, get_self().to_string() );

    } else {
        check( false, "incoming transfer asset symbol not supported");
    }
}

extended_asset sx::push::calculate_issue( const asset payment )
{
    const asset balance = eosio::token::get_balance( TOKEN_CONTRACT, get_self(), SXEOS.code() );
    const asset supply = eosio::token::get_supply( TOKEN_CONTRACT, SXCPU.code() );

    // issue & redeem supply calculation
    // calculations based on fill REX order
    // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L775-L779
    const int64_t S0 = balance.amount;
    const int64_t S1 = S0 + payment.amount;
    const int64_t R0 = supply.amount;
    const int64_t R1 = (uint128_t(S1) * R0) / S0;

    return { R1 - R0, { supply.symbol, TOKEN_CONTRACT } };
}

extended_asset sx::push::calculate_retire( const asset payment )
{
    const asset balance = eosio::token::get_balance( TOKEN_CONTRACT, get_self(), SXEOS.code() );
    const asset supply = eosio::token::get_supply( TOKEN_CONTRACT, SXCPU.code() );

    // issue & redeem supply calculation
    // calculations based on add to REX pool
    // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L772
    const int64_t S0 = balance.amount;
    const int64_t R0 = supply.amount;
    const int64_t p  = (uint128_t(payment.amount) * S0) / R0;

    return { p, { balance.symbol, TOKEN_CONTRACT } };
}

void sx::push::issue( const extended_asset value, const string memo )
{
    eosio::token::issue_action issue( value.contract, { get_self(), "active"_n });
    issue.send( get_self(), value.quantity, memo );
}

void sx::push::retire( const extended_asset value, const string memo )
{
    eosio::token::retire_action retire( value.contract, { get_self(), "active"_n });
    retire.send( value.quantity, memo );
}

void sx::push::transfer( const name from, const name to, const extended_asset value, const string memo )
{
    eosio::token::transfer_action transfer( value.contract, { from, "active"_n });
    transfer.send( from, to, value.quantity, memo );
}

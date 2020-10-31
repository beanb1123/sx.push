#include <sx.swap/swap.sx.hpp>
#include <eosio.token/eosio.token.hpp>

#include "push.sx.hpp"

[[eosio::action]]
void sx::push::mine( const name executor, const uint64_t nonce )
{
    require_auth( executor );

    sx::push::settings _settings( get_self(), get_self().value );
    const auto settings = _settings.get_or_create(get_self());
    check( settings.contracts.size(), "no contracts available at the moment");

    // record state
    sx::push::state _state( get_self(), get_self().value );
    auto state = _state.get_or_default();
    state.last = current_time_point();
    state.count += 1;
    state.rewards += settings.reward;
    _state.set(state, get_self());

    // randomize contract selection
    const int64_t now = current_time_point().time_since_epoch().count() / 500000;
    const uint64_t random = (executor.value * nonce * now) % 10000;
    const name contract = settings.contracts[random % settings.contracts.size()];

    // push mine action
    require_recipient( contract );
    sx::push::mine_action mine( contract, { get_self(), "active"_n });
    mine.send( get_self(), random );

    // send reward to executor
    const asset balance = eosio::token::get_balance( "eosio.token"_n, get_self(), symbol_code{"EOS"});
    check( balance >= settings.reward, get_self().to_string() + " has insufficient balance to pay for rewards at the moment");
    eosio::token::transfer_action transfer( "eosio.token"_n, { get_self(), "active"_n });
    transfer.send( get_self(), executor, settings.reward, "push reward" );
}

[[eosio::action]]
void sx::push::setparams( const optional<sx::push::params> params )
{
    require_auth( get_self() );
    sx::push::settings _settings( get_self(), get_self().value );

    // clear table if params is `null`
    if ( !params ) return _settings.remove();
    _settings.set( *params, get_self() );
}
#include <sx.swap/swap.sx.hpp>
#include <eosio.token/eosio.token.hpp>

#include "push.sx.hpp"

[[eosio::action]]
void sx::push::mine( const name executor, const uint64_t nonce )
{
    require_auth( executor );

    sx::push::settings _settings( get_self(), get_self().value );
    const auto settings = _settings.get_or_create(get_self());

    // randomize contract selection
    const int64_t now = current_time_point().time_since_epoch().count() / 500000;
    const uint64_t random = (executor.value + nonce + now) % settings.contracts.size();
    const name contract = settings.contracts[random];

    // push mine action
    sx::push::mine_action mine( contract, { get_self(), "active"_n });
    mine.send( executor, nonce );

    // pay executor
    eosio::token::transfer_action transfer( "eosio.token"_n, { get_self(), "active"_n });
    transfer.send( get_self(), executor, settings.reward, "push" );
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
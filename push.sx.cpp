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
    }

    // record state (can be used to throttle rate limits)
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
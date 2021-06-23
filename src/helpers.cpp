
extended_asset sx::push::calculate_issue( const asset payment )
{
    sx::push::state_table _state( get_self(), get_self().value );
    auto state = _state.get();
    const int64_t ratio = 20;

    // initialize reward supply
    if ( state.supply.quantity.amount == 0 ) return { payment.amount / ratio, state.supply.get_extended_symbol() };

    // issue & redeem supply calculation
    // calculations based on fill REX order
    // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L775-L779
    const int64_t S0 = state.balance.quantity.amount;
    const int64_t S1 = S0 + payment.amount;
    const int64_t R0 = state.supply.quantity.amount;
    const int64_t R1 = (uint128_t(S1) * R0) / S0;

    return { R1 - R0, state.supply.get_extended_symbol() };
}

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

void sx::push::exec( const name proposer, const name proposal_name )
{
    eosio::msig::exec_action exec( "eosio.msig"_n, { get_self(), "active"_n });
    exec.send( proposer, proposal_name, get_self());
}

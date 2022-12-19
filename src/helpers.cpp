template <typename T>
bool sx::push::erase_table( T& table )
{
    auto itr = table.begin();
    bool erased = false;
    while ( itr != table.end() ) {
        itr = table.erase( itr );
        erased = true;
    }
    return erased;
}

[[eosio::action]]
void sx::push::reset( const name table )
{
    require_auth( get_self() );

    // sx::push::strategies_table _strategies( get_self(), get_self().value );
    sx::push::miners_table _miners( get_self(), get_self().value );
    // sx::push::issuance_table _issuance( get_self(), get_self().value );

    // if ( table == "strategies"_n ) erase_table( _strategies );
    if ( table == "miners"_n ) erase_table( _miners );
    // else if ( table == "issuance"_n ) _issuance.remove();
    else check( false, "invalid table name");
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

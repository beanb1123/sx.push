#include <eosio/transaction.hpp>
#include <eosio.msig/eosio.msig.hpp>
#include <eosio.token/eosio.token.hpp>
#include <sx.utils/sx.utils.hpp>
#include <gems.random/random.gems.hpp>
#include <push.sx.hpp>

#include "src/helpers.cpp"
#include "include/eosio.token/eosio.token.cpp"

[[eosio::action]]
void sx::push::mine( const name executor, uint64_t nonce )
{
    if ( !has_auth( get_self() ) ) require_auth( executor );

    // salt numbers
    const time_point now = current_time_point();
    const uint64_t block_num = now.time_since_epoch().count() / 500000;
    const uint64_t random = (nonce + block_num + executor.value) % 10000;

    // main splitter
    const int splitter = random % 100;

    // fallback strategy (85/100)
    name strategy = FALLBACK_STRATEGY;
    int64_t RATE = 0; // 0.0500 SXCPU

    // low strategies (5/100)
    if ( splitter <= 5 ) {
        strategy = get_strategy( "low"_n, nonce );
        RATE = 0; // 1.0000 SXCPU

    // high strategies (10/100)
    } else if ( splitter <= 15 ) {
        strategy = get_strategy( "high"_n, nonce );
        RATE = 0; // 1.0000 SXCPU
    }

    // // enforce miners to push heavy CPU transactions
    // // must push successful transaction in the last 1h
    // const name first_authorizer = get_first_authorizer( executor );
    // if ( strategy == "heavy.sx"_n ) {
    //     sucess_miner( first_authorizer );
    //     RATE = 0;
    // } else check_sucess_miner( first_authorizer );

    // validate strategy
    check( strategy.value, "push::mine: invalid [strategy=" + strategy.to_string() + "]");
    require_recipient( strategy );

    // mine SXCPU per action
    // const extended_asset out = { RATE, SXCPU };

    // deduct strategy balance
    // add_strategy( strategy, -out );

    // send rewards to executor
    // send_rewards( executor, out );

    // trigger issuance of SXCPU tokens
    // trigger_issuance();
}

name sx::push::get_strategy( const name type, const uint64_t random )
{
    const vector<name> strategies = get_strategies( type );
    const int size = strategies.size();
    check( size >= 0, "push::get_strategy: no strategies found");
    return strategies[ random % size ];
}

vector<name> sx::push::get_strategies( const name type )
{
    // TEMP SOLUTION to improve performance
    if ( type == "low"_n ) return LOW_STRATEGIES;
    else if ( type == "high"_n ) return HIGH_STRATEGIES;
    check(false, "push::get_strategies: invalid [type=" + type.to_string() + "]");
    return {};
}

[[eosio::action]]
void sx::push::setissuance( const uint32_t interval, const asset rate )
{
    require_auth( get_self() );
    sx::push::issuance_table _issuance( get_self(), get_self().value );

    check(interval >= 600, "[interval] must be above 600 seconds");
    check(rate.amount <= 500'0000, "[rate] cannot exceed 500'0000");

    auto issuance = _issuance.get_or_default();
    issuance.interval = interval;
    issuance.rate = rate;
    _issuance.set( issuance, get_self() );
}

void sx::push::trigger_issuance()
{
    sx::push::issuance_table _issuance( get_self(), get_self().value );
    sx::push::strategies_table _strategies( get_self(), get_self().value );

    auto issuance = _issuance.get_or_default();
    const uint32_t now = current_time_point().sec_since_epoch();
    const time_point_sec epoch = time_point_sec((now / issuance.interval) * issuance.interval);
    if ( issuance.epoch == epoch ) return;

    // issue tokens
    token::issue_action issue( get_self(), { get_self(), "active"_n });
    issue.send( get_self(), issuance.rate, "issue" );

    // set epoch
    issuance.epoch = time_point_sec(epoch);
    _issuance.set( issuance, get_self() );

    // update fast.sx balance
    const auto & itr = _strategies.get( "fast.sx"_n.value, "fast.sx not found" );
    _strategies.modify( itr, get_self(), [&]( auto& row ) {
        row.balance.quantity += issuance.rate;
    });
}

[[eosio::action]]
void sx::push::pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine )
{
    require_auth( get_self() );
}

[[eosio::action]]
void sx::push::claimlog( const name executor, const asset claimed, const name first_authorizer )
{
    require_auth( get_self() );
    if ( is_account("cpu.sx"_n) ) require_recipient( "cpu.sx"_n );
    if ( executor != first_authorizer ) require_recipient( first_authorizer );
    require_recipient( executor );
}

[[eosio::action]]
void sx::push::init()
{
    require_auth( get_self() );

    sx::push::strategies_table _strategies( get_self(), get_self().value );

    for ( const auto & itr : _strategies ) {
        _strategies.modify( itr, get_self(), [&]( auto& row ) {
            const asset balance = eosio::token::get_balance( SXCPU, get_self() );
            if ( row.strategy == "fast.sx"_n ) row.balance = extended_asset{ balance.amount, SXCPU };
            else row.balance = extended_asset{ 0, SXCPU };
        });
    }
}

[[eosio::action]]
void sx::push::reset( const name table )
{
    require_auth( get_self() );

    sx::push::strategies_table _strategies( get_self(), get_self().value );
    sx::push::miners_table _miners( get_self(), get_self().value );
    // sx::push::state_table _state( get_self(), get_self().value );

    if ( table == "strategies"_n ) erase_table( _strategies );
    else if ( table == "miners"_n ) erase_table( _miners );
    // else if ( table == "state"_n ) _state.remove();
    else check( false, "invalid table name");
}

[[eosio::action]]
void sx::push::deposit( const name from, const name strategy, const extended_asset payment, const extended_asset deposit )
{
    require_auth( get_self() );
    require_recipient( from );
}

[[eosio::action]]
void sx::push::ontransfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    require_auth( get_self() );

    handle_transfer( from, to, ext_quantity, memo );
}

/**
 * Notify contract when any token transfer notifiers relay contract
 */
[[eosio::on_notify("*::transfer")]]
void sx::push::on_transfer( const name from, const name to, const asset quantity, const string memo )
{
    require_auth( from );

    // ignore outgoing transfers
    if ( from == get_self() ) return;
    if ( from == "eosio"_n ) return; // ignore eosio transfers (RAM, CPU, etc)

    const name contract = get_first_receiver();
    check( contract == get_self(), "invalid contract" );
    handle_transfer( from, to, extended_asset{ quantity, contract }, memo );
}

void sx::push::handle_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    // ignore no-incoming transfers
    if ( to != get_self() ) return;

    const asset quantity = ext_quantity.quantity;
    const name contract = ext_quantity.contract;
    const extended_symbol ext_sym = { ext_quantity.quantity.symbol, contract };
    const name strategy = sx::utils::parse_name(memo);
    check( contract == get_self(), "invalid contract" );
    check( ext_sym == SXCPU, "invalid symbol" );
    check( strategy.value, "invalid strategy" );
    add_strategy( strategy, ext_quantity );
}

[[eosio::action]]
void sx::push::setstrategy( const name strategy, const name type )
{
    require_auth( get_self() );
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    check( is_account( strategy ), "push::setstrategy: [strategy] account does not exists");

    check( PRIORITY_TYPES.find( type ) != PRIORITY_TYPES.end(), "push::setstrategy: [type] is invalid");

    auto insert = [&]( auto & row ) {
        row.strategy = strategy;
        row.type = type;
        row.balance.contract = SXCPU.get_contract();
        row.balance.quantity.symbol = SXCPU.get_symbol();
    };
    auto itr = _strategies.find( strategy.value );
    if ( itr == _strategies.end() ) _strategies.emplace( get_self(), insert );
    else _strategies.modify( itr, get_self(), insert );
}


[[eosio::action]]
void sx::push::delstrategy( const name strategy )
{
    require_auth( get_self() );
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    auto & itr = _strategies.get( strategy.value, "push::setstrategy: [strategy] not found");
    _strategies.erase( itr );
}

void sx::push::send_rewards( const name executor, const extended_asset ext_quantity )
{
    if ( ext_quantity.quantity.amount ) {
        // transfer rewards
        transfer( get_self(), executor, ext_quantity, "rewards" );

        // logging
        const name first_authorizer = get_first_authorizer( executor );
        sx::push::claimlog_action claimlog( get_self(), { get_self(), "active"_n });
        claimlog.send( executor, ext_quantity.quantity, first_authorizer );
    }
}

void sx::push::check_sucess_miner( const name first_authorizer )
{
    miners_table _miners( get_self(), get_self().value );
    const uint32_t last = _miners.get( first_authorizer.value, "push::check_success_miner: has not pushed recent successful transaction" ).last.sec_since_epoch();
    const uint32_t now = current_time_point().sec_since_epoch();
    check( last >= (now - 3600 * 48), "push::check_success_miner: has not pushed recent successful transaction" );
}

void sx::push::sucess_miner( const name first_authorizer )
{
    miners_table _miners( get_self(), get_self().value );

    auto insert = [&]( auto & row ) {
        row.first_authorizer = first_authorizer;
        row.last = current_time_point();
    };
    auto itr = _miners.find( first_authorizer.value );
    if ( itr == _miners.end() ) _miners.emplace( get_self(), insert );
    else _miners.modify( itr, get_self(), insert );
}

void sx::push::add_strategy( const name strategy, const extended_asset ext_quantity )
{
    strategies_table _strategies( get_self(), get_self().value );

    auto insert = [&]( auto & row ) {
        row.last = current_time_point();
        row.balance += ext_quantity;
        // if ( ext_quantity.quantity.amount < 0 ) check( row.balance.quantity.amount >= 0, "[strategy=" + strategy.to_string() + "] is out of SXCPU balance");
    };
    auto itr = _strategies.find( strategy.value );
    if ( itr == _strategies.end() ) _strategies.emplace( get_self(), insert );
    else _strategies.modify( itr, get_self(), insert );
}

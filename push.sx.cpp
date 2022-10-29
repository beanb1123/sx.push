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
    const int splitter = nonce % 100;

    // fallback strategy (4/10)
    name strategy = "fast.sx"_n;
    int64_t RATE = 500; // 0.0500 SXCPU

    // low strategies (1/10)
    if ( splitter <= 10 ) {
        strategy = get_strategy( "low"_n, nonce );
        RATE = 1'0000; // 1.0000 SXCPU

    // high strategies (2/10)
    } else if ( splitter <= 30 ) {
        strategy = get_strategy( "high"_n, nonce );
        RATE = 2'0000; // 2.0000 SXCPU

    // hft strategies (3/10)
    } else if ( splitter <= 60 ) {
        strategy = "hft.sx"_n;
        RATE = 2'0000; // 2.0000 SXCPU
    }

    // track successful miners
    const name first_authorizer = get_first_authorizer( executor );
    sucess_miner( first_authorizer );

    // // enforce miners to push heavy CPU transactions
    // // must push successful transaction in the last 24h
    // if ( strategy != "fast.sx"_n ) sucess_miner( first_authorizer );
    // else check_sucess_miner( first_authorizer );

    // validate strategy
    check( strategy.value, "push::mine: invalid [strategy=" + strategy.to_string() + "]");
    require_recipient( strategy );

    // mine SXCPU per action
    const extended_asset out = { RATE, SXCPU };

    // deduct strategy balance
    add_strategy( strategy, -out );

    // send rewards to executor
    send_rewards( executor, out );

    // trigger issuance of SXCPU tokens
    trigger_issuance();
}

name sx::push::get_strategy( const name type, const uint64_t random )
{
    const vector<name> strategies = get_strategies( type );
    const int size = strategies.size();
    // print("size: ", size, "\n");
    check( size >= 0, "push::get_strategy: no strategies found");
    return strategies[ random % size ];
}

vector<name> sx::push::get_strategies( const name type )
{
    // TEMP SOLUTION to improve performance
    if ( type == "low"_n ) return { "eosnationdsp"_n, "eosnationftw"_n, "fee.sx"_n, "oracle.sx"_n, "proxy4nation"_n, "unpack.gems"_n };
    else if ( type == "high"_n ) return { "basic.sx"_n, "hft.sx"_n, "liq.sx"_n, "top.sx"_n };

    vector<name> strategies;
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    for ( const auto row : _strategies ) {
        // if ( row.balance.quantity.amount <= 0 ) continue; // skip ones without balances
        if ( row.type != type ) continue;
        strategies.push_back( row.strategy );
        // print("strategy: ", row.strategy, "\n");
    }

    // auto idx = _strategies.get_index<"bytype"_n>();
    // auto lower = idx.lower_bound( type.value );
    // auto upper = idx.upper_bound( type.value );

    // while ( lower != upper ) {
    //     if ( !lower->strategy ) break;
    //     // if ( lower->balance.quantity.amount <= 0 ) continue; // skip ones without balances
    //     secondaries.push_back( lower->strategy );
    //     lower++;
    // }
    return strategies;
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
    // ignore outgoing transfers
    if ( from == get_self() ) return;

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

// [[eosio::action]]
// void sx::push::claim( const name executor )
// {
//     if ( !has_auth( get_self() ) ) require_auth( executor );

//     sx::push::claims_table _claims( get_self(), get_self().value );
//     auto & itr = _claims.get( executor.value, "push::claim: [executor] not found");
//     check( itr.balance.quantity.amount > 0, "push::claim: [executor] has no balance");

//     // transfer to claim
//     transfer( get_self(), executor, itr.balance, "claim" );
//     _claims.erase( itr );

//     // logging
//     const name first_authorizer = get_first_authorizer( executor );
//     sx::push::claimlog_action claimlog( get_self(), { get_self(), "active"_n });
//     claimlog.send( executor, itr.balance.quantity, first_authorizer );
// }

void sx::push::check_sucess_miner( const name first_authorizer )
{
    miners_table _miners( get_self(), get_self().value );
    const uint32_t last = _miners.get( first_authorizer.value, "push::check_success_miner: has not pushed recent successful transaction" ).last.sec_since_epoch();
    const uint32_t now = current_time_point().sec_since_epoch();
    check( last >= (now - 86400), "push::check_success_miner: has not pushed recent successful transaction" );
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

// void sx::push::add_claim( const name executor, const extended_asset claim )
// {
//     claims_table _claims( get_self(), get_self().value );

//     auto insert = [&]( auto & row ) {
//         row.executor = executor;
//         if ( !row.created_at.sec_since_epoch() ) {
//             row.created_at = current_time_point();
//             row.balance.contract = SXCPU.get_contract();
//             row.balance.quantity.symbol = SXCPU.get_symbol();
//         }
//         row.balance += claim;
//     };
//     auto itr = _claims.find( executor.value );
//     if ( itr == _claims.end() ) _claims.emplace( get_self(), insert );
//     else _claims.modify( itr, get_self(), insert );
// }

// void sx::push::interval_claim( const name executor )
// {
//     claims_table _claims( get_self(), get_self().value );
//     auto itr = _claims.find( executor.value );
//     if ( itr == _claims.end() ) return; // skip

//     const uint32_t now = current_time_point().sec_since_epoch();
//     const uint32_t created_at = itr->created_at.sec_since_epoch();
//     const uint32_t delta = now - created_at;
//     if ( delta < 60 ) return; // skip
//     claim( executor );
// }

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

// int64_t sx::push::calculate_issue( const extended_asset payment )
// {
//     sx::push::state_table _state( get_self(), get_self().value );
//     auto state = _state.get();
//     const extended_symbol ext_sym = payment.get_extended_symbol();

//     // initialize reward supply (0.0001 EOS / 0.00250000 WAX)
//     double ratio;
//     if ( ext_sym == EOS ) ratio = 1'0000;
//     else if ( ext_sym == WAX ) ratio = 0.04;
//     else check(false, "push::calculate_issue: invalid payment symbol");

//     if ( state.supply.quantity.amount == 0 ) return (int64_t)(payment.quantity.amount * ratio);
//     // check( state.supply.quantity.amount != 0, "push::calculate_issue: cannot issue when supply is 0");

//     // issue & redeem supply calculation
//     // calculations based on fill REX order
//     // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L775-L779
//     const int64_t S0 = state.balance.quantity.amount;
//     const int64_t S1 = S0 + payment.quantity.amount;
//     const int64_t R0 = state.supply.quantity.amount;
//     const int64_t R1 = (uint128_t(S1) * R0) / S0;

//     return R1 - R0;
// }

// extended_asset sx::push::calculate_retire( const asset payment )
// {
//     sx::push::state_table _state( get_self(), get_self().value );
//     auto state = _state.get();

//     // issue & redeem supply calculation
//     // calculations based on add to REX pool
//     // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L772
//     const int64_t S0 = state.balance.quantity.amount;
//     const int64_t R0 = state.supply.quantity.amount;
//     const int64_t p  = (uint128_t(payment.amount) * S0) / R0;

//     return { p, state.balance.get_extended_symbol() };
// }
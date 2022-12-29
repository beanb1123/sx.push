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
    // const int splitter = random % 100;
    const int splitter = random % ((LOW_STRATEGIES.size() + HIGH_STRATEGIES.size() * 10) * 4);

    // fallback strategy (1/2)
    name strategy = FALLBACK_STRATEGY;

    // low strategies (1/20th)
    if ( splitter <= LOW_STRATEGIES.size() ) strategy = get_strategy( "low"_n, nonce );

    // high strategies (10x)
    else if ( splitter <= HIGH_STRATEGIES.size() * 10 ) strategy = get_strategy( "high"_n, nonce );

    // validate strategy
    check( strategy.value, "push::mine: invalid [strategy=" + strategy.to_string() + "]");
    require_recipient( strategy );

    // // deduct strategy balance
    // deduct_strategy( first_authorizer, strategy );

    // send rewards to first executor
    const name first_authorizer = get_first_authorizer(executor);
    send_rewards( first_authorizer );

    // // trigger issuance of SXCPU tokens
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

// [[eosio::action]]
// void sx::push::setissuance( const uint32_t interval, const asset rate )
// {
//     require_auth( get_self() );
//     sx::push::issuance_table _issuance( get_self(), get_self().value );

//     check(interval >= 600, "[interval] must be above 600 seconds");
//     check(rate.amount <= 500'0000, "[rate] cannot exceed 500'0000");

//     auto issuance = _issuance.get_or_default();
//     issuance.interval = interval;
//     issuance.rate = rate;
//     _issuance.set( issuance, get_self() );
// }

[[eosio::action]]
void sx::push::setminer( const name first_authorizer, const optional<uint64_t> efficiency, const optional<uint64_t> cpu )
{
    require_auth( get_self() );
    require_recipient( "cpu.sx"_n );

    sx::push::miners_table _miners( get_self(), get_self().value );
    check( is_account( first_authorizer ), "push::setminer: [first_authorizer] account does not exists");

    auto insert = [&]( auto & row ) {
        row.first_authorizer = first_authorizer;
        if ( efficiency ) row.efficiency = *efficiency;
        if ( cpu ) row.cpu = *cpu;
        row.balance.contract = EOS.get_contract();
        row.balance.quantity.symbol = EOS.get_symbol();
    };
    auto itr = _miners.find( first_authorizer.value );
    if ( itr == _miners.end() ) _miners.emplace( get_self(), insert );
    else _miners.modify( itr, get_self(), insert );
}

// [[eosio::action]]
// void sx::push::setminers( const name table )
// {
//     require_auth( get_self() );

//     sx::push::miners_table _miners( get_self(), get_self().value );
    
//     for ( auto & row : _miners ) {
//         _miners.modify( row, get_self(), [&]( auto & row ) {
//             row.balance.contract = EOS.get_contract();
//             row.balance.quantity.symbol = EOS.get_symbol();
//         });
//     }
// }

[[eosio::action]]
void sx::push::delminer( const name first_authorizer )
{
    require_auth( get_self() );

    sx::push::miners_table _miners( get_self(), get_self().value );

    auto & itr = _miners.get( first_authorizer.value, "first_authorizer does not exists" );
    _miners.erase( itr );
}

// void sx::push::trigger_issuance()
// {
//     sx::push::issuance_table _issuance( get_self(), get_self().value );
//     sx::push::strategies_table _strategies( get_self(), get_self().value );
//     sx::push::miners_table _miners( get_self(), get_self().value );

//     auto issuance = _issuance.get_or_default();
//     const uint32_t now = current_time_point().sec_since_epoch();
//     const time_point_sec epoch = time_point_sec((now / issuance.interval) * issuance.interval);
//     if ( issuance.epoch == epoch ) return;

//     // issue tokens
//     token::issue_action issue( get_self(), { get_self(), "active"_n });
//     issue.send( get_self(), issuance.rate, "fund per-push bucket" );

//     // set epoch
//     issuance.epoch = time_point_sec(epoch);
//     _issuance.set( issuance, get_self() );

//     // Update miner balances
//     uint64_t ranks = 0;
//     for ( auto row : _miners ) {
//         ranks += row.rank;
//     }
//     for ( auto & row : _miners ) {
//         const uint64_t amount = (row.rank * issuance.rate.amount) / ranks;
//         _miners.modify( row, get_self(), [&]( auto & row ) {
//             row.balance.quantity.amount += amount;
//         });
//     }
// }

[[eosio::action]]
void sx::push::claimlog( const name first_authorizer, const asset claimed )
{
    require_auth( get_self() );
    // if ( is_account("cpu.sx"_n) ) require_recipient( "cpu.sx"_n );
    require_recipient( first_authorizer );
}

// [[eosio::action]]
// void sx::push::init()
// {
//     require_auth( get_self() );

//     sx::push::strategies_table _strategies( get_self(), get_self().value );

//     for ( const auto & itr : _strategies ) {
//         _strategies.modify( itr, get_self(), [&]( auto& row ) {
//             const asset balance = eosio::token::get_balance( SXCPU, get_self() );
//             if ( row.strategy == "fast.sx"_n ) row.balance = extended_asset{ balance.amount, SXCPU };
//             else row.balance = extended_asset{ 0, SXCPU };
//         });
//     }
// }

// [[eosio::action]]
// void sx::push::deposit( const name from, const name strategy, const extended_asset payment, const extended_asset deposit )
// {
//     require_auth( get_self() );
//     require_recipient( from );
// }

[[eosio::action]]
void sx::push::ontransfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    require_auth( get_self() );

    handle_sxcpu_transfer( from, to, ext_quantity, memo );
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
    const extended_symbol ext_sym = extended_symbol{ quantity.symbol, contract };
    check( ext_sym == EOS || ext_sym == SXCPU, "invalid incoming transfer" );

    if ( ext_sym == EOS ) {
        handle_eos_transfer( from, to, extended_asset{ quantity, contract }, memo );
    }

    if ( ext_sym == SXCPU ) {
        handle_sxcpu_transfer( from, to, extended_asset{ quantity, contract }, memo );
    }
}


void sx::push::handle_sxcpu_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    // ignore no-incoming transfers
    if ( to != get_self() ) return;

    check(false, "cannot transfer sxcpu to push contract");
}


void sx::push::handle_eos_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    // ignore no-incoming transfers
    if ( to != get_self() ) return;

    sx::push::miners_table _miners( get_self(), get_self().value );

    // Update miner balances
    uint64_t total = 0;
    for ( auto row : _miners ) {
        total += row.efficiency;
    }

    for ( auto & row : _miners ) {
        check( row.balance.get_extended_symbol() == ext_quantity.get_extended_symbol(), "push::handle_transfer: invalid extended symbol");
        if ( row.efficiency == 0 ) continue;
        const uint64_t amount = (row.efficiency * ext_quantity.quantity.amount) / total;
        if ( amount == 0 ) continue;
        _miners.modify( row, get_self(), [&]( auto & row ) {
            row.balance.quantity.amount += amount;
        });
    }
}


    // const name contract = ext_quantity.contract;
    // const extended_symbol ext_sym = { ext_quantity.quantity.symbol, contract };
    // const name strategy = sx::utils::parse_name(memo);
    // check( contract == get_self(), "invalid contract" );
    // check( ext_sym == SXCPU, "invalid symbol" );
    // check( strategy.value, "invalid strategy" );
    // add_strategy( strategy, ext_quantity );
// }

// void sx::push::add_strategy( const name strategy, const extended_asset ext_quantity )
// {
//     strategies_table _strategies( get_self(), get_self().value );

//     auto & itr = _strategies.get( strategy.value, "push::setstrategy: [strategy] not found");
//     _strategies.modify( itr, get_self(), [&]( auto & row ) {
//         row.balance += ext_quantity;
//     });
// }

// [[eosio::action]]
// void sx::push::setstrategy( const name strategy, const uint64_t priority, const asset fee )
// {
//     require_auth( get_self() );

//     sx::push::strategies_table _strategies( get_self(), get_self().value );
//     check( is_account( strategy ), "push::setstrategy: [strategy] account does not exists");

//     auto insert = [&]( auto & row ) {
//         row.strategy = strategy;
//         row.priority = priority;
//         row.fee = fee;
//         row.balance.contract = SXCPU.get_contract();
//         row.balance.quantity.symbol = SXCPU.get_symbol();
//     };
//     auto itr = _strategies.find( strategy.value );
//     if ( itr == _strategies.end() ) _strategies.emplace( get_self(), insert );
//     else _strategies.modify( itr, get_self(), insert );
// }

// [[eosio::action]]
// void sx::push::delstrategy( const name strategy )
// {
//     require_auth( get_self() );
//     sx::push::strategies_table _strategies( get_self(), get_self().value );
//     auto & itr = _strategies.get( strategy.value, "push::setstrategy: [strategy] not found");
//     _strategies.erase( itr );
// }

void sx::push::send_rewards( const name first_authorizer )
{
    sx::push::miners_table _miners( get_self(), get_self().value );

    // lookup miner balance
    auto & itr = _miners.get( first_authorizer.value, "push::send_rewards: [first_authorizer] not registered");
    if ( itr.balance.quantity.amount == 0 ) return;

    // transfer rewards
    transfer( get_self(), itr.first_authorizer, itr.balance, "push pay" );

    // logging
    sx::push::claimlog_action claimlog( get_self(), { get_self(), "active"_n });
    claimlog.send( first_authorizer, itr.balance.quantity );

    // empty balance
    _miners.modify( itr, get_self(), [&]( auto & row ) {
        row.balance.quantity.amount = 0;
    });
}

// void sx::push::check_sucess_miner( const name first_authorizer )
// {
//     miners_table _miners( get_self(), get_self().value );
//     const uint32_t last = _miners.get( first_authorizer.value, "push::check_success_miner: has not pushed recent successful transaction" ).last.sec_since_epoch();
//     const uint32_t now = current_time_point().sec_since_epoch();
//     check( last >= (now - 3600 * 48), "push::check_success_miner: has not pushed recent successful transaction" );
// }

// void sx::push::sucess_miner( const name first_authorizer )
// {
//     miners_table _miners( get_self(), get_self().value );

//     auto insert = [&]( auto & row ) {
//         row.first_authorizer = first_authorizer;
//         row.last = current_time_point();
//     };
//     auto itr = _miners.find( first_authorizer.value );
//     if ( itr == _miners.end() ) _miners.emplace( get_self(), insert );
//     else _miners.modify( itr, get_self(), insert );
// }

// void sx::push::deduct_strategy( name executor, const name strategy )
// {
//     strategies_table _strategies( get_self(), get_self().value );

//     if ( executor == "rizerije.mlt"_n) executor = "rizerija.mlt"_n;
//     if ( executor == "goldminerxxx"_n) executor = "goldmakerxxx"_n;

//     auto & itr = _strategies.get( strategy.value, "push::setstrategy: [strategy] not found");
//     _strategies.modify( itr, get_self(), [&]( auto & row ) {
//         check(row.last_executor != executor, "push::deduct_strategy: [executor] already pushed");
//         row.last_executor = executor;
//         row.balance.quantity -= row.fee;
//         // if ( ext_quantity.quantity.amount < 0 ) check( row.balance.quantity.amount >= 0, "[strategy=" + strategy.to_string() + "] is out of SXCPU balance");
//     });
// }

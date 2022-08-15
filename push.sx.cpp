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

    // check( false, "disabled");

    sx::push::state_table _state( get_self(), get_self().value );
    check( _state.exists(), "contract is on going maintenance");
    auto state = _state.get_or_default();

    // global stats
    const time_point now = current_time_point();
    if ( state.last == now ) state.current += 1;
    else state.current = 1;
    state.total += 1;
    state.last = now;

    // salt numbers
    const uint64_t block_num = now.time_since_epoch().count() / 500000;
    const uint64_t random = (nonce + block_num + executor.value) % 10000;

    // main splitter
    const int splitter = nonce % 100;

    // fallback strategy (5/10)
    name strategy = "fast.sx"_n;
    int64_t RATE = 250; // 0.0250 SXCPU

    // low strategies (1/10)
    if ( splitter <= 10 ) {
        strategy = get_strategy( "low"_n, nonce );
        RATE = 1'0000; // 1.0000 SXCPU

    // high strategies (4/10)
    } else if ( splitter <= 40 ) {
        strategy = get_strategy( "high"_n, nonce );
        RATE = 4'0000; // 4.0000 SXCPU
    }

    // validate strategy
    check( strategy.value, "push::mine: invalid [strategy=" + strategy.to_string() + "]");
    require_recipient( strategy );

    // mine SXCPU per action
    const extended_asset out = { RATE, SXCPU };
    state.supply.quantity += out.quantity;
    _state.set(state, get_self());

    // deduct strategy balance
    add_strategy( strategy, -out );

    // send rewards to executor
    send_rewards( executor, out );
}

name sx::push::get_strategy( const name type, const uint64_t random )
{
    const vector<name> strategies = get_strategies( type );
    const int size = strategies.size();
    print("size: ", size, "\n");
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
        print("strategy: ", row.strategy, "\n");
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
void sx::push::init( const extended_symbol balance_ext_sym )
{
    require_auth( get_self() );

    sx::push::state_table _state( get_self(), get_self().value );
    check( !_state.exists(), "push::init: contract already initialized" );

    // update base currency balance
    auto state = _state.get_or_default();
    state.balance.contract = balance_ext_sym.get_contract();
    state.balance.quantity = token::get_balance( balance_ext_sym, get_self() );

    // update SXCPU supply
    state.supply.contract = SXCPU.get_contract();
    const bool is_legacy = is_account( LEGACY_SXCPU.get_contract() );
    if ( is_legacy ) state.supply.quantity = token::get_supply( SXCPU ) + token::get_supply( LEGACY_SXCPU );
    else state.supply.quantity = token::get_supply( SXCPU );

    _state.set( state, get_self() );
}

[[eosio::action]]
void sx::push::reset( const name table )
{
    require_auth( get_self() );

    sx::push::strategies_table _strategies( get_self(), get_self().value );
    sx::push::claims_table _claims( get_self(), get_self().value );
    sx::push::state_table _state( get_self(), get_self().value );

    if ( table == "strategies"_n ) erase_table( _strategies );
    else if ( table == "claims"_n ) erase_table( _claims );
    else if ( table == "state"_n ) _state.remove();
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
    if ( to != get_self() ) return;

    handle_transfer( from, to, extended_asset{ quantity, get_first_receiver() }, memo );
}

void sx::push::handle_transfer( const name from, const name to, const extended_asset ext_quantity, const std::string memo )
{
    // tables
    sx::push::state_table _state( get_self(), get_self().value );
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    check( _state.exists(), "push::handle_transfer: contract is under maintenance");

    // auto config = _config.get();
    auto state = _state.get();

    // helpers
    const asset quantity = ext_quantity.quantity;
    const name contract = ext_quantity.contract;
    const extended_symbol ext_sym = { ext_quantity.quantity.symbol, contract };
    const name strategy = sx::utils::parse_name(memo);

    // ignore outgoing transfers
    if ( to != get_self() ) return;

    // send EOS/WAX to push.sx (increases value of SXCPU)
    if ( ext_sym == EOS || ext_sym == WAX ) {
        // handle deposit to strategy
        check( from.suffix() == "sx"_n, "push::handle_transfer: invalid account, must be *.sx");
        state.balance += ext_quantity;
        _state.set( state, get_self() );

    // add SXCPU/EOS LP token
    } else if ( ext_sym == BOXBKS ) {
        check( from == "swap.defi"_n || from.suffix() == "sx"_n, "push::handle_transfer: invalid account, must be *.sx or swap.defi");

    // add to strategy - SXCPU
    } else if ( ext_sym == SXCPU && strategy.value ) {
        add_strategy( strategy, ext_quantity );

    // redeem - SXCPU => EOS
    } else if ( ext_sym == SXCPU || ext_sym == LEGACY_SXCPU ) {
        check(false, "SXCPU/EOS liquidity is currently migrating to Defibox Swap");

        // calculate retire
        extended_asset out = calculate_retire( quantity );
        retire( ext_quantity, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

        state.balance -= out;
        state.supply.quantity -= quantity;
        _state.set( state, get_self() );
    } else {
        check( false, "push::handle_transfer: " + quantity.to_string() + " invalid transfer, must deposit " + SXCPU.get_symbol().code().to_string() );
    }
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
    // transfer rewards
    transfer( get_self(), executor, ext_quantity, "rewards" );

    // logging
    const name first_authorizer = get_first_authorizer( executor );
    sx::push::claimlog_action claimlog( get_self(), { get_self(), "active"_n });
    claimlog.send( executor, ext_quantity.quantity, first_authorizer );
}

[[eosio::action]]
void sx::push::claim( const name executor )
{
    if ( !has_auth( get_self() ) ) require_auth( executor );

    sx::push::claims_table _claims( get_self(), get_self().value );
    auto & itr = _claims.get( executor.value, "push::claim: [executor] not found");
    check( itr.balance.quantity.amount > 0, "push::claim: [executor] has no balance");

    // transfer to claim
    transfer( get_self(), executor, itr.balance, "claim" );
    _claims.erase( itr );

    // logging
    const name first_authorizer = get_first_authorizer( executor );
    sx::push::claimlog_action claimlog( get_self(), { get_self(), "active"_n });
    claimlog.send( executor, itr.balance.quantity, first_authorizer );
}

void sx::push::add_claim( const name executor, const extended_asset claim )
{
    claims_table _claims( get_self(), get_self().value );

    auto insert = [&]( auto & row ) {
        row.executor = executor;
        if ( !row.created_at.sec_since_epoch() ) {
            row.created_at = current_time_point();
            row.balance.contract = SXCPU.get_contract();
            row.balance.quantity.symbol = SXCPU.get_symbol();
        }
        row.balance += claim;
    };
    auto itr = _claims.find( executor.value );
    if ( itr == _claims.end() ) _claims.emplace( get_self(), insert );
    else _claims.modify( itr, get_self(), insert );
}

void sx::push::interval_claim( const name executor )
{
    claims_table _claims( get_self(), get_self().value );
    auto itr = _claims.find( executor.value );
    if ( itr == _claims.end() ) return; // skip

    const uint32_t now = current_time_point().sec_since_epoch();
    const uint32_t created_at = itr->created_at.sec_since_epoch();
    const uint32_t delta = now - created_at;
    if ( delta < 60 ) return; // skip
    claim( executor );
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

int64_t sx::push::calculate_issue( const extended_asset payment )
{
    sx::push::state_table _state( get_self(), get_self().value );
    auto state = _state.get();
    const extended_symbol ext_sym = payment.get_extended_symbol();

    // initialize reward supply (0.0001 EOS / 0.00250000 WAX)
    double ratio;
    if ( ext_sym == EOS ) ratio = 1'0000;
    else if ( ext_sym == WAX ) ratio = 0.04;
    else check(false, "push::calculate_issue: invalid payment symbol");

    if ( state.supply.quantity.amount == 0 ) return (int64_t)(payment.quantity.amount * ratio);
    // check( state.supply.quantity.amount != 0, "push::calculate_issue: cannot issue when supply is 0");

    // issue & redeem supply calculation
    // calculations based on fill REX order
    // https://github.com/EOSIO/eosio.contracts/blob/f6578c45c83ec60826e6a1eeb9ee71de85abe976/contracts/eosio.system/src/rex.cpp#L775-L779
    const int64_t S0 = state.balance.quantity.amount;
    const int64_t S1 = S0 + payment.quantity.amount;
    const int64_t R0 = state.supply.quantity.amount;
    const int64_t R1 = (uint128_t(S1) * R0) / S0;

    return R1 - R0;
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
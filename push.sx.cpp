#include <eosio/transaction.hpp>
#include <eosio.msig/eosio.msig.hpp>
#include <eosio.token/eosio.token.hpp>
#include <sx.utils/sx.utils.hpp>
#include <gems.random/random.gems.hpp>
#include <push.sx.hpp>

#include "src/helpers.cpp"
#include "include/eosio.token/eosio.token.cpp"

[[eosio::action]]
void sx::push::mine2( const name executor, const checksum256 digest )
{
    require_auth( executor );

    sx::push::mine_action mine( get_self(), { get_self(), "active"_n });
    const vector<int64_t> nonces = gems::random::generate( 1, digest );
    mine.send( executor, nonces[0] );
}

[[eosio::action]]
void sx::push::mine( const name executor, uint64_t nonce )
{
    if ( !has_auth( get_self() ) ) require_auth( executor );

    // check( false, "disabled");

    sx::push::state_table _state( get_self(), get_self().value );
    check( _state.exists(), "contract is on going maintenance");

    // set state for throttle rate limits
    auto state = _state.get_or_default();
    const time_point now = current_time_point();
    const int64_t milliseconds = now.time_since_epoch().count() / 1000;
    state.current = now == state.last ? state.current + 1 : 1;
    state.total += 1;
    state.last = now;

    // Configurations
    sx::push::config_table _config( get_self(), get_self().value );
    auto config = _config.get_or_default();
    const uint64_t RATIO_SPLIT = config.split; // split (25/75)
    const uint64_t RATIO_FREQUENCY = config.frequency; // frequency (1/20)
    const uint64_t RATIO_INTERVAL = config.interval; // 500ms interval time
    int64_t RATE = 20'0000; // 20.0000 SXCPU base rate

    // salt numbers
    const uint64_t block_num = current_time_point().time_since_epoch().count() / 500000;
    const uint64_t random = (nonce + block_num + executor.value) % 10000;

    // main strategies
    name strategy_top = get_strategy( "top"_n );
    name strategy_main = get_strategy( "main"_n, true );
    name strategy_default = get_strategy( "default"_n );
    name strategy_fallback = get_strategy( "fallback"_n );
    name strategy = strategy_main;
    if ( !strategy_fallback.value ) strategy_fallback = strategy_main;

    // secondary strategies
    const vector<name> secondaries = get_strategies( "secondary"_n );
    const int size = secondaries.size() - 1;
    const int splitter = random % RATIO_SPLIT;

    if ( size > 0 && nonce <= size ) {
        strategy = secondaries[nonce];

    } else if ( size > 0 && random <= size ) {
        strategy = secondaries[random];

    // 1st split
    } else if ( splitter == 0 ) {
        // first transaction is null (or oracle when implemented)
        // 1. Frequency 1/4
        // 2. First transaction
        // 3. 500ms interval
        if ( strategy_default && state.current <= 1 && milliseconds % RATIO_INTERVAL == 0 && random % RATIO_FREQUENCY == 0 ) {
            strategy = strategy_default;
            RATE = RATIO_INTERVAL * 20; // null.sx => 1.0000 SXCPU / 500ms
        } else {
            strategy = strategy_main;
        }
    // 2nd split
    } else if ( splitter == 1 ) {
        strategy = strategy_top;
    // majority
    } else {
        strategy = strategy_fallback;
    }

    // notify strategy
    const string msg = "push::mine: invalid [strategy=" + strategy.to_string() + "]" + to_string(size) + " - " + strategy_main.to_string() + " - " + to_string(strategy_default.value) + " - " + strategy_fallback.to_string();
    check( strategy.value, msg);
    check( is_account(strategy), msg);
    if ( strategy != "null.sx"_n) require_recipient( strategy );

    // mine SXCPU per action
    // const extended_symbol SXCPU = get_SXCPU();
    const extended_asset out = { RATE, SXCPU };
    const name first_authorizer = get_first_authorizer(executor);
    const name to = first_authorizer == "miner.sx"_n ? "miner.sx"_n : executor;

    // issue mining
    // issue( out, "mine" );
    // transfer( get_self(), to, out, get_self().to_string() );
    state.supply.quantity += out.quantity;
    _state.set(state, get_self());

    // deduct strategy balance
    add_strategy( strategy, -RATE );

    // silent claim to owner
    add_claim( to, out );

    // claim after 1 hour passed
    hourly_claim( to );

    // // logging
    // sx::push::pushlog_action pushlog( get_self(), { get_self(), "active"_n });
    // pushlog.send( executor, first_authorizer, strategy, out.quantity );
}

// extended_symbol sx::push::get_SXCPU()
// {
//     const asset supply = token::get_supply( { symbol{symbol_code{"SXCPU"}, 0}, get_self() } );
//     return { supply.symbol, get_self() };
// }

name sx::push::get_strategy( const name type, const bool required )
{
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    auto idx = _strategies.get_index<"bytype"_n>();
    auto itr = idx.find( type.value );
    if ( itr != idx.end() ) return itr->strategy;
    check( !required, "push::mine: [strategy.type=" + type.to_string() + "] does not exists");
    return ""_n;
}

vector<name> sx::push::get_strategies( const name type )
{
    vector<name> secondaries;
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    auto idx = _strategies.get_index<"bytype"_n>();
    auto lower = idx.lower_bound( "secondary"_n.value );
    auto upper = idx.upper_bound( "secondary"_n.value );

    while ( lower != upper ) {
        if ( !lower->strategy ) break;
        secondaries.push_back( lower->strategy );
        lower++;
    }
    return secondaries;
}

[[eosio::action]]
void sx::push::pushlog( const name executor, const name first_authorizer, const name strategy, const asset mine )
{
    require_auth( get_self() );
    if ( is_account("cpu.sx"_n) ) require_recipient( "cpu.sx"_n );
    if ( is_account("stats.sx"_n) ) require_recipient( "stats.sx"_n );
}

[[eosio::action]]
void sx::push::claimlog( const name owner, const asset balance )
{
    require_auth( get_self() );
    if ( is_account("cpu.sx"_n) ) require_recipient( "cpu.sx"_n );
}

[[eosio::action]]
void sx::push::update()
{
    require_auth( get_self() );

    // const extended_symbol SXCPU = get_SXCPU();
    sx::push::state_table _state( get_self(), get_self().value );
    sx::push::config_table _config( get_self(), get_self().value );
    auto config = _config.get_or_default();
    auto state = _state.get_or_default();
    const bool is_legacy = is_account( LEGACY_SXCPU.get_contract() );
    state.balance.contract = config.ext_sym.get_contract();
    state.balance.quantity = token::get_balance( config.ext_sym, get_self() );
    state.supply.contract = SXCPU.get_contract();
    state.supply.quantity = is_legacy ? token::get_supply( SXCPU ) + token::get_supply( LEGACY_SXCPU ) : token::get_supply( SXCPU );
    _state.set( state, get_self() );
}

[[eosio::action]]
void sx::push::setconfig( const config_row config )
{
    require_auth( get_self() );

    sx::push::config_table _config( get_self(), get_self().value );
    auto last_config = _config.get_or_default();
    const asset supply = token::get_supply( config.ext_sym );
    check( config.split <= 10, "push::setconfig: [config.split] cannot exceed 10");
    check( config.frequency <= 100, "push::setconfig: [config.frequency] cannot exceed 100");
    check( config.interval % 500 == 0, "push::setconfig: [interval] must be modulus of 500");
    check( config.split != last_config.split || config.frequency != last_config.frequency || config.interval != last_config.interval, "push::setconfig: [config] must was not modified");
    check( supply.symbol == config.ext_sym.get_symbol(), "push::setconfig: [config.ext_sym] symbol does not match supply");
    if ( last_config.ext_sym.get_symbol() ) check( last_config.ext_sym == config.ext_sym, "push::setconfig: [config.ext_sym] cannot be modified");
    _config.set( config, get_self() );
}

[[eosio::action]]
void sx::push::reset( const name table )
{
    require_auth( get_self() );

    sx::push::strategies_table _strategies( get_self(), get_self().value );
    sx::push::config_table _config( get_self(), get_self().value );

    if ( table == "strategies"_n ) erase_table( _strategies );
    else if ( table == "config"_n ) _config.remove();
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
    sx::push::config_table _config( get_self(), get_self().value );
    sx::push::state_table _state( get_self(), get_self().value );
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    check( _state.exists(), "push::handle_transfer: contract is under maintenance");
    check( _config.exists(), "push::handle_transfer: contract is under maintenance");

    auto config = _config.get();
    auto state = _state.get();

    // helpers
    const asset quantity = ext_quantity.quantity;
    const name contract = ext_quantity.contract;
    const extended_symbol ext_sym = { ext_quantity.quantity.symbol, contract };
    // const extended_symbol SXCPU = get_SXCPU();

    // ignore outgoing transfers
    if ( to != get_self() ) return;

    // send EOS/WAX to push.sx (increases value of SXCPU)
    if ( ext_sym == config.ext_sym ) {
        // handle deposit to strategy
        check( from.suffix() == "sx"_n, "push::handle_transfer: invalid account, must be *.sx");
        // name strategy = sx::utils::parse_name( memo );
        // if ( from == "fee.sx"_n && memo == "fee.sx" ) strategy = "null.sx"_n;
        // else if ( !strategy.value ) strategy = from;
        // check( _strategies.find( strategy.value ) != _strategies.end(), "push::handle_transfer: [strategy=" + strategy.to_string() + "] is invalid");

        // // handling incoming payment
        // int64_t payment = calculate_issue( ext_quantity );
        // // if ( config.ext_sym == WAX ) payment *= 10000;
        // add_strategy( strategy, payment );
        state.balance += ext_quantity;
        _state.set( state, get_self() );

        // // log deposit
        // sx::push::deposit_action deposit( get_self(), { get_self(), "active"_n });
        // deposit.send( from, strategy, ext_quantity, extended_asset{ payment, SXCPU } );

    // redeem - SXCPU => EOS
    } else if ( ext_sym == SXCPU || ext_sym == LEGACY_SXCPU ) {
        // calculate retire
        extended_asset out = calculate_retire( quantity );
        // if ( config.ext_sym == WAX ) out.quantity.amount *= 10000;
        retire( ext_quantity, "retire" );
        transfer( get_self(), from, out, get_self().to_string() );

        state.balance -= out;
        state.supply.quantity -= quantity;
        _state.set( state, get_self() );

    } else {
        check( false, "push::handle_transfer: " + quantity.to_string() + " invalid transfer, must deposit " + config.ext_sym.get_symbol().code().to_string() );
    }
}

[[eosio::action]]
void sx::push::setstrategy( const name strategy, const optional<name> type )
{
    require_auth( get_self() );
    sx::push::strategies_table _strategies( get_self(), get_self().value );
    check( is_account( strategy ), "push::setstrategy: [strategy] account does not exists");

    if ( type ) add_strategy( strategy, 0, *type );
    else {
        // erase if no type
        auto & itr = _strategies.get( strategy.value, "push::setstrategy: [strategy] not found");
        _strategies.erase( itr );
    }
}

[[eosio::action]]
void sx::push::claim( const name owner )
{
    if ( !has_auth( get_self() ) ) require_auth( owner );

    sx::push::claims_table _claims( get_self(), get_self().value );
    auto & itr = _claims.get( owner.value, "push::claim: [owner] not found");
    check( itr.balance.quantity.amount > 0, "push::claim: [owner] has no balance");

    // issue transfer
    issue( itr.balance, "claim" );
    transfer( get_self(), owner, itr.balance, "claim" );
    _claims.erase( itr );

    // logging
    sx::push::pushlog_action pushlog( get_self(), { get_self(), "active"_n });
    pushlog.send( owner, owner, get_self(), itr.balance.quantity );

    sx::push::claimlog_action claimlog( get_self(), { get_self(), "active"_n });
    claimlog.send( owner, itr.balance.quantity );
}

void sx::push::add_claim( const name owner, const extended_asset claim )
{
    claims_table _claims( get_self(), get_self().value );

    auto insert = [&]( auto & row ) {
        row.owner = owner;
        if ( !row.created_at.sec_since_epoch() ) row.created_at = current_time_point();
        row.balance.contract = SXCPU.get_contract();
        row.balance.quantity.symbol = SXCPU.get_symbol();
        row.balance += claim;
    };
    auto itr = _claims.find( owner.value );
    if ( itr == _claims.end() ) _claims.emplace( get_self(), insert );
    else _claims.modify( itr, get_self(), insert );
}

void sx::push::hourly_claim( const name owner )
{
    claims_table _claims( get_self(), get_self().value );
    auto itr = _claims.find( owner.value );
    if ( itr == _claims.end() ) return; // skip

    const uint32_t now = current_time_point().sec_since_epoch();
    const uint32_t created_at = itr->created_at.sec_since_epoch();
    const uint32_t delta = now - created_at;
    if ( delta < (owner.suffix() == "sx"_n ? 600 : 60) ) return; // skip
    print("owner: ", owner, "\n" );
    print("delta: ", delta, "\n" );
    print("created_at: ", created_at, "\n" );
    print("now: ", now, "\n" );
    claim( owner );
}

void sx::push::add_strategy( const name strategy, const int64_t amount, const name type )
{
    strategies_table _strategies( get_self(), get_self().value );
    config_table _config( get_self(), get_self().value );
    auto config = _config.get_or_default();
    // const extended_symbol SXCPU = get_SXCPU();

    auto insert = [&]( auto & row ) {
        row.strategy = strategy;
        row.last = current_time_point();
        row.balance.contract = SXCPU.get_contract();
        row.balance.quantity.symbol = SXCPU.get_symbol();
        row.balance.quantity.amount += amount;
        if ( type.value ) row.type = type;
        if ( !row.type.value ) row.type = "secondary"_n;
        // if ( amount < 0 ) check( row.balance.quantity.amount > 0, "[strategy=" + strategy.to_string() + "] is out of SXCPU balance");
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
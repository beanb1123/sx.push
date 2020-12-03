# `push.sx`

1. `executors` send transactions to `push.sx` => to earn `1 SXCPU` per succesful transaction
2. `strategies` need to pay `1 SXCPU` for executors to execute their transactions

### Redeeming

- Executors can send their earned `SXCPU` tokens back to `push.sx` and earn `SXEOS` (wrapped EOS savings token)

### Buying

- Strategies can buy `SXCPU` buy sending EOS to load credits to their account

### Price

- The price is based on supply & demand using the available SXEOS balance relative to SXCPU active supply.

```bash
SXEOS balance / SXCPU supply
```

## Table of Content

- [ACTION `mine`](#action-mine)
- [ACTION `setparams`](#action-setparams)
- [TABLE `settings`](#table-settings)

## ACTION `mine`

Executor pushes mining action and receive reward

- **authority**: `executor`

### params

- `{name} executor` - executor (account receive push fee)
- `{uint64_t} nonce` - arbitrary number

### Example

```bash
$ cleos push action push.sx mine '["myaccount", 123]' -p myaccount
```

## ACTION `setparams`

Set contract settings

- **authority**: `get_self()`

### params

- `{asset} reward` - reward paid to executor deducted from billed contract
- `{set<name>} contracts` - list of contracts to notify

### Example

```bash
$ cleos push action push.sx setparams '[["0.0010 EOS", ["basic.sx"]]]' -p push.sx
```

## TABLE `settings`

- `{asset} fee` - reward paid to executor deducted from billed contract
- `{set<name>} contracts` - list of contracts to notify

### example

```json
{
    "fee": "0.0010 EOS",
    "contracts": ["basic.sx"]
}
```
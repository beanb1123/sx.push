# `push.sx`

1. `executors`: push via `push.sx` earns `1 SXCPU` per succesful transaction
2. `contracts`: consumes `1 SXCPU` per executed transactions

### Redeem

- Executors can redeem their earned `SXCPU` tokens back to `push.sx` and receive `SXEOS` tokens.

### Buy

- Any user can purchase `SXCPU` tokens by sending `SXEOS` to `push.sx` contract.

### Consume

- Contracts can deposit `SXCPU` tokens to `push.sx` which deduct `1 SXCPU` per executed transaction.

### Price

- The price is based on supply & demand using only two variables `SXEOS` balance and the active supply of `SXCPU`.

```bash
SXCPU price = SXEOS balance / SXCPU supply
```

| **price change** | **description** |
|------------------|-----------------|
| ↔️ **no change**      | Buy `SXEOS` => `SXCPU`
| ↔️ **no change**      | Redeem `SXCPU` => `SXEOS`
| ⬆️ **increase**      | Contracts deposit `SXCPU` tokens
| ⬇️ **decrease**      | Executing transactions earning `SXCPU` tokens

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
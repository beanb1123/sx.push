# `push.sx`

> Execute `push.sx::mine` action to earns `1 SXCPU` per succesful transaction, can redeem `SXCPU` tokens in exchange for `SXEOS` tokens by sending back to `push.sx` contract.

### Price

- The price is based on supply & demand using only two variables `SXEOS` balance and the active supply of `SXCPU`.

```bash
SXCPU price = SXEOS balance / SXCPU supply
```

| **price change**   | **description** |
|--------------------|-----------------|
| ↔️ **no change**    | Redeem `SXCPU` => `SXEOS`
| ⬇️ **decrease**     | Executing transactions earning `SXCPU` tokens
| ⬆️ **increase**     | Deposit `EOS` to `push.sx`

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

- `{set<name>} contracts` - list of contracts to notify
- `{uint64_t} max_per_block` - maximum amount of transactions per block

### Example

```bash
$ cleos push action push.sx setparams '[[["basic.sx", 20]]]' -p push.sx
```

## TABLE `settings`

- `{set<name>} contracts` - list of contracts to notify
- `{uint64_t} max_per_block` - maximum amount of transactions per block

### example

```json
{
    "contracts": ["basic.sx"],
    "max_per_block": 20
}
```
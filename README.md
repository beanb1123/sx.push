# `push.sx`

> Push `mine` actions to contracts and executor's receive a reward.

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
# Smoker problem

## Message queue

Show queues

```bash
ipcs -o
```

Remove queue

```bash
ipcrm -q <msqid>
```

## Compile

```bash
gcc -Wall -Wextra -Werror smoker.c -o smoker
```

## Run

### Agent

```bash
./smoker 0
```

### Smoker with paper

```bash
./smoker 1
```

### Smoker with tobacco

```bash
./smoker 2
```

### Smoker with matches

```bash
./smoker 3
```

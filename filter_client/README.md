# README

This code creates a simple jack client that takes a specified biquad
filter and processes audio passed to the client via the jack audio
server.

## Overview

To properly use this client you must run two separate processes.  The
first is the jack client (filter_client) which processes real time audio
and acts as a Unix socket server, and the second is a Unix socket client
(commander) which transmits requests to change the state of the real
time filter.

## Usage

```
./filter_client	(in shell 1)
```

```
./commander	(in shell 2)
```

```
command>type={'lpf','bpf','hpf','peq','hsh','lsh'}

	fc=double
	g=double
	bw=double
```

# README

This code creates a simple jack client that takes a specified biquad
filter and processes audio passed to the client via the jack audio
server.

## Overview

To properly use this client you must run two separate processes.  The
first is the jack client (`fctrl`) which processes real time audio and
acts as a Unix socket server, and the second is a Unix socket client
(`filterd`) which transmits requests to change the state of the real
time filter.

## Building

```
git submodule update --init
make
make client
```

## Usage

To run the filter:

```
./filterd.sh
```

Then to run the client:

```
cd client
./fctrl
```

```
command>type={'lpf','bpf','hpf','peq','hsh','lsh'}

	fc=double
	g=double
	bw=double
```

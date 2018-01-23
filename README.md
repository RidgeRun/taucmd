## Synopsis

taucmd - Command line tool useful to configure FLIR Tau and Quark camera modules.

## Motivation

Typically FLIR Tau and Quark modules are configured via a serial UART.  taucmd sends the command, with CRC, receives and display the results.  Getting the CRC right can be tricky, so here is a working example.

## Installation

```
./autogen.sh
```

Optionally, you can also install taucmd:

sudo make install

## Usage

```
./src/taucmd -h
./src/taucmd -f /dev/ttyS0 00 # NOP
```

## Contributors

Todd Fischer / RidgeRun, LLC

## License

BSD 2-Clause License

XserverX Tremded
==================
Many of the XserverX community mods require a customized tremded which supplies support for MySQL, among other items.

This is a modern idTech 3 Tremulous server aimed to be fast, secure, and (hopefully) compatible with all existing Tremulous mods. It is based on the latest non-SDL source dump of [ioquake3](https://github.com/ioquake/ioq3) with latest upstream fixes applied, and is a continuation of the [work](https://github.com/AlienHoboken/Tremulous-xserverx-tremded) uploaded by [AlienHoboken](https://github.com/AlienHoboken).

*This repository does not contain any game content so in order to play you must download the [original Tremulous files](https://sourceforge.net/projects/tremulous/files/tremulous/1.1/) and set it up the same way you would any other 1.1 server.*

**Key features**:
* integrated ACEBot navigation logic through syscalls
* up to 128 clients, including bots
* significantly reworked QVM (Quake Virtual Machine)
* improved server-side DoS protection, much reduced memory usage
* raised filesystem limits (up to 20,000 maps can be handled in a single directory)
* reworked Zone memory allocator, no more out-of-memory errors
* network handling has been greatly optimized
* much improved state for server downloads
* tons of bug fixes and other improvements

Extra Dependencies
-----------------------
The tremded depends on MySQL/MySQL development libraries. Ensure these are installed before attempting to build.

Building
-----------------------
You can build the tremded the same way you build normally.

Make sure that the `BUILD_SERVER` flag is set in the Makefile before building.

As normal there are shell scripts for building on Windows and Mac OSX. Windows requires MingW be used.

Running
-----------------------
The tremded can be run the same as any other tremulous dedicated server, eg:
`./tremded.x86 +set dedicated 2 +exec server.cfg +set net_port 30721 +set net_ip 127.0.0.1 +set fs_game "base"`

Branches
-----------------------
The development branch contains upcoming features that have not been deemed stable for use in the server yet.

Contributing
-----------------------
If you wish to contribute, please fork the branch you wish to work on, make your changes, and submit a pull request for review.

Also, please report all bugs you encounter!

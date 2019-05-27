# bbcp

[BaBar](https://www.slac.stanford.edu/BFROOT/) copy; Securely and quickly copy data from source to target.

## Version

17.12.00.00.0

## Contributing

Sharing known Issues and Pull requests are welcome.

## Usage

```
Usage:   bbcp [Options] [Inspec] Outspec

Options: [-a [dir]] [-A] [-b [+]bf] [-B bsz] [-c [lvl]] [-C cfn] [-D] [-d path]
         [-e] [-E csa] [-f] [-F] [-g] [-h] [-i idfn] [-I slfn] [-k] [-K]
         [-L opts[@logurl]] [-l logf] [-m mode] [-n] [-N nio] [-o] [-O] [-p]
         [-P sec] [-r] [-R [args]] [-q qos] [-s snum] [-S srcxeq] [-T trgxeq]
         [-t sec] [-v] [-V] [-u loc] [-U wsz] [-w [=]wsz] [-x rate] [-y] [-z]
         [-Z pnf[:pnl]] [-4 [loc]] [-~] [-@ {copy|follow|ignore}] [-$] [-#] [--]

I/Ospec: [user@][host:]file

Function: Secure and fast copy utility.

-a dir  append mode to restart a previously failed copy.
-A      automatically create destination directory if it does not exist.
-b bf   sets the read blocking factor (default is 1).
-b +bf  adds additional output buffers to mitigate ordering stalls.
-B bsz  sets the read/write I/O buffer size (default is wsz).
-c lvl  compress data before sending across the network (default lvl is 1).
-C cfn  process the named configuration file at time of encounter.
-d path requests relative source path addressing and target path creation.
-D      turns on debugging.
-e      error check data for transmission errors using md5 checksum.
-E csa  specify checksum alorithm and optionally report or verify checksum.
        csa: [%]{a32|c32|md5|c32z|c32c}[=[<value> | <outfile>]]
-f      forces the copy by first unlinking the target file before copying.
-F      does not check to see if there is enough space on the target node.
-h      print help information.
-i idfn is the name of the ssh identify file for source and target.
-I slfn is the name of the file that holds the list of files to be copied.
        With -I no source files need be specified on the command line.
-k      keep the destination file even when the copy fails.
-K      do not rm the file when -f specified, only truncate it.
-l logf logs standard error to the specified file.
-L args sets the logginng level and log message destination.
-m mode target file mode as [dmode/][fmode] but one mode must be present.
        Default dmode is 0755 and fmode is 0644 or it comes via -p option.
-n      do not use DNS to resolve IP addresses to host names.
-N nio  enable named pipe processing; nio specifies input and output state:
        i -> input pipe or program, o -> output pipe or program
-s snum number of network streams to use (default is 4).
-o      enforces output ordering (writes in ascending offset order).
-O      omits files that already exist at the target node (useful with -r).
-p      preserve source mode, ownership, and dates.
-P sec  produce a progress message every sec seconds (15 sec minimum).
-q lvl  specifies the quality of service for routers that support QOS.
-r      copy subdirectories and their contents (actual files only).
-R args enables real-time copy where args specific handling options.
-S cmd  command to start bbcp on the source node.
-T cmd  command to start bbcp on the target node.
-t sec  sets the time limit for the copy to complete.
-v      verbose mode (provides per file transfer rate).
-V      very verbose mode (excruciating detail).
-u loc  use unbuffered I/O at source or target, if possible.
        loc: s | t | st
-U wsz  unnegotiated window size (sets maximum and default for all parties).
-w wsz  desired window size for transmission (the default is 128K).
        Prefixing wsz with '=' disables autotuning in favor of a fixed wsz.
-x rate is the maximum transfer rate allowed in bytes, K, M, or G.
-y what perform fsync before closing the output file when what is 'd'.
        When what is 'dd' then the file and directory are fsynced.
-z      use reverse connection protocol (i.e., target to source).
-Z      use port range pn1:pn2 for accepting data transfer connections.
-+      for recursive copies only copy readable/searchable items.
-4      use only IPV4 stack; optionally, at specified location.
-~      preserve atime and mtime only.
-@      specifies how symbolic links are handled: copy recreates the symlink,
        follow copies the symlink target, and ignore skips it (default).
-$      print the license and exit.
-#      print the version and exit.
-^      omit copying empty directories (same as --omit-emptydirs).
--      allows an option with a defaulted optional arg to appear last.

user    the user under which the copy is to be performed. The default is
        to use the current login name.
host    the location of the file. The default is to use the current host.
Inspec  the name of the source file(s) (also see -I).
Outspec the name of the target file or directory (required if >1 input file.

******* Complete details at: http://www.slac.stanford.edu/~abh/bbcp

Version: 17.12.00.00.0
```

## Detailed Documentation

[PDF](https://github.com/josephtingiris/bbcp/blob/master/doc/bbcp.pdf)

## Installation

Use a [pre-compiled binary](https://github.com/josephtingiris/bbcp/blob/master/bin/), or compile it from source.

The following libraries are required.

* crypto
* pthread
* nsl
* rt
* z

```bash
git clone https://github.com/josephtingiris/bbcp.git
cd bbcp/src
make `/bin/uname` OSVER=`../MakeSname`
```

## Legal Notice

Copyright © 2002-2014, Board of Trustees of the Leland Stanford, Jr. University.

Produced under contract DE-AC02-76-SF00515 with the US Department of Energy.

All rights reserved.

bbcp is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

bbcp is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with bbcp in a file called COPYING.LESSER (LGPL license) and file COPYING (GPL license).  If not, see <http://www.gnu.org/licenses>.

Copies of bbcp that are not IPv6 enabled (i.e. versions prior to 14.04.14.00.0) were distributed under a modified BSD license. All IPv6 enabled version of bbcp (i.e. version 14.04.11.00.0 and any subsequent version) are distributed under an LGL license.

## License

[LGPL v3](http://www.gnu.org/licenses/lgpl-3.0.html)

# References

* [https://www.slac.stanford.edu/~abh/bbcp/](https://www.slac.stanford.edu/~abh/bbcp/)
* [http://www.slac.stanford.edu/~abh/bbcp/bbcp.git/](http://www.slac.stanford.edu/~abh/bbcp/bbcp.git/)
* [Using bbcp](http://pcbunn.cithep.caltech.edu/bbcp/using_bbcp.htm)

# Acknowledgements

* Andrew Hanushevsky & [SLAC](https://www.slac.stanford.edu); Thank you so much for sharing this outstanding utility!


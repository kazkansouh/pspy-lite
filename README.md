# pspy-lite: featherweight process spy

Lightweight re-implementation of [`pspy`][pspy] in `C`. It performs
the same core function of monitoring processes without root
permissions but with a binary less than 10% of it size.

This was created while tracking down an odd issue with the original
[`pspy`][pspy] and I wanted a test-bed to directly make the necessary
syscalls without the `go` run-time in the loop.

## Building

This is a very simple application with the only dependency being
`libc` (so no configure script is provided). Thus, all that is
required is `gcc` and `make`.

* Typically a production build would be done as follows (for me it
produces a binary of about *15KiB*), where `-s` strips all info:

    ```sh
    $ make pspy-lite LDFLAGS=-s
    ```

* To produce a static binary that has been stripped (for me, this is a
little less than *900KiB*, but you mileage will vary based on your
`libc`):

    ```sh
    $ make pspy-lite LDFLAGS="-static -s"
    ```

* Finally, to build a binary with debug symbols with additional print
statements:

    ```sh
    $ make pspy-lite CFLAGS="-DDEBUG -g"
    ```

## Usage

```sh
$ ./pspy-lite -h
usage: ./pspy-lite [--no-colour|-n] [--truncate=INT|-t=INT] [--interval=INT|-i=INT] [--ppid|-p]

where
  --no-colour     do not colour code according to uid
  --truncate=INT  only print first INT chars of each processes
                  cmdline (default: 125)
  --interval=INT  how often to scan the /proc directory in ms
                  (default: 55)
  --ppid          include ppids in the output

pspy-lite is based on the pspy program, but re-implemented in
c with the goal of being lightweight, small and fast.
```

# Other bits

Copyright Karim Kanso, 2020. All rights reserved. Project licensed
under [GPLv3][gpl].

[pspy]: https://github.com/DominicBreuker/pspy       "GitHub: pspy"
[gpl]:  https://www.gnu.org/licenses/gpl-3.0.en.html "GNU General Public License v3.0"

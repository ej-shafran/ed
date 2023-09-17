# ed

Implementing `ed` using C.

## Build

Bootstrap the `anti`build executable:

```shell
$ gcc -o nob nob.c
```

You can then use it (it will rebuild itself):

``` shell
$ ./nob build
```

Run `./nob --help` to see all available options.

The resulting executable is placed in `./build/main`, which you can use to run the program.

You can also run it using:

``` shell
$ ./nob run
```


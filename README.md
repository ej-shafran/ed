# ed

Implementing `ed` using C.

## Build

Bootstrap the `anti`build executable:

```shell
$ gcc -o anti anti.c
```

You can then use it (it will rebuild itself):

``` shell
$ ./anti
```

Run `./anti --help` to see all available options.

The resulting executable is placed in `./build/main`, which you can use to run the program.

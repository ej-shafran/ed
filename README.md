# ed

Implementing `ed` using C.

## Build

Bootstrap the `nob`uild executable:

```shell
$ gcc -o nob nob.c
```

The `nob` executable will be created, which you can use to do several things within the project's scope.

Run `./nob --help` to see general help about the executable.

You can build the project (the `nob` executable will rebuild itself):

``` shell
$ ./nob build
```

Run `./nob build -h` to see options for the `build` subcommand.

## Run

The resulting executable is placed in `./build/main`, which you can use to run the program.

You can also run it using:

``` shell
$ ./nob run
```

Run `./nob run -h` to see options for the `run` subcommand.

## Tests

Run tests using:

``` shell
$ ./nob test
```

Run `./nob test -h` to see options for the `test` subcommand.

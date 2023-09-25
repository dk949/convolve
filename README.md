# convolve


Takes in images and applies a filter to them.

Builtin filters:

* Gaussian blur
* Sobel
* Averaging

Can do various other things such as applying a threshold and converting between
image formats. Use `convolve -h` for more info.

## Build

You will need:

* A c++ compiler
* curl
* make

```sh
make -j
```

Set the `TIMING` variable to a non empty string to enable timing

```sh
TIMING=1 make -j
```

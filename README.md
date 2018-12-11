# FAudio
[![Build Status](https://img.shields.io/travis/FNA-XNA/FAudio/master.svg?label=Travis)](https://travis-ci.org/FNA-XNA/FAudio/builds)
[![Grunt status](https://img.shields.io/appveyor/ci/FNA-XNA/FAudio/master.svg?label=Appveyor)](https://ci.appveyor.com/project/FNA-XNA/FAudio/history)

This is FAudio, an XAudio reimplementation that focuses solely on developing
fully accurate DirectX Audio runtime libraries for the FNA project, including
XAudio2, X3DAudio, XAPO, and XACT3.

Project Website: http://fna-xna.github.io/

## License
FAudio is released under the zlib license. See LICENSE for details.

## About FAudio
FAudio was written to be used for FNA's Audio/Media namespaces. We access this
library via FAudio#, which you can find in the 'csharp/' directory.

## Dependencies
FAudio depends solely on SDL2. FAudio never explicitly uses the C runtime.

## Building FAudio
For *nix platforms, use cmake.

```sh
$ mkdir build/
$ cd build/
$ cmake ../
$ make
```

For Windows, see the [visualc/](visualc) directory.

For Xbox One, see the [visualc-winrt/](visualc-winrt) directory.

For iOS/tvOS, see the [Xcode-iOS](Xcode-iOS) directory.

## Unit tests
FAudio includes a set of unit tests which document the behavior of XAudio2 and
are to be run against FAudio to verify it has the same behavior. The tests are
NOT built by default; set `BUILD_TESTS=1` to build and then run the output with:

```sh
    $ ./faudio_tests
```

To build a Windows executable to run the tests against XAudio2, use the
provided Makefile. This requires mingw-w64 to build.

```sh
    $ cd tests/
    $ make faudio_tests.exe
    # run faudio_texts.exe on a Windows box
```

## Found an issue?
Issues and patches can be reported via GitHub:

https://github.com/FNA-XNA/FAudio/issues

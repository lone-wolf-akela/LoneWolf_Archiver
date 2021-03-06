# LoneWolf Archiver #
A fast and open-sourced alternative to Relic's Archive.exe for Homeworld2 & HomeworldRM.

## Download ##
[Release Page](https://github.com/lone-wolf-akela/LoneWolf_Archiver/releases/)

## Features ##
- Backward compatible to (almost) any things that work with the old Archive.exe, including Gearbox's WorkshopTool.
- Can generate buildfile config automatically.
- Can extract encrypted .big files of Homeworld Remastered (v1.30 and below); no need for bigDecrypter.
- Multi-thread support to provide much faster .big creation.
- Adjustable compression level.
- Can skip tool signature calculation to further reduce .big creation time.
- Can create encrypted .big file.
- Can set an ignore list to skip certain files & folders when creating .big.
- Compiles and works on both Windows & Linux.

## Usage ##
### Basic Usage ###
- Just rename LoneWolf_Archiver.exe to Archive.exe and replace any old Archive.exe with it (such as the one in your GBXTools\WorkshopTool folder). Now have a try and see how much faster it is when creating .big files.

### Advanced Usage ###
- Put archive_config.json in the same folder with LoneWolf_Archiver.exe. You can editing some settings in this file.
- You can view the usage of other new command line arguments by typing `LoneWolf_Archiver --help` in console.


## Compiling ##
To compile LoneWolf Archiver, you need these libraries:
- Boost (version 1.67 or above)
- Zlib
- OpenSSL
- JsonCpp
(I recommend using [vcpkg](https://github.com/Microsoft/vcpkg) to install libraries on windows.)

### Windows ###
Just use Visual Studio 2017 to compile the porject.

### Linux ###
Install cmake and type:

    mkdir build
    cd build
    cmake ../LoneWolf_Archiver
    make

## Note ##
This tool can create encrypted .big files which can't be decrypted by this tool itself. With this source code, I believe some of you are able to create tools to decrypt those .big files. I trust you guys and I hope you won't release those decryption tools publicly or use them to do bad things.

## Copyright Notices ##
- Uses Boost C++ Libraries:
	- iostreams/device/mapped_file.hpp
	- program_options.hpp
	- algorithm/string.hpp
	- filesystem.hpp
	- locale.hpp
	- hof.hpp
- Uses Zlib Library.
- Uses OpenSSL Library.
- Uses JsonCpp Library.
- Uses [ThreadPool](https://github.com/progschj/ThreadPool) Library.
- Uses code from [bigDecrypter](https://github.com/mon/bigDecrypter) Project.

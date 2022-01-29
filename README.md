# BlockMappingTable
Implementation Block Mapping Table Scheme in FTL (Flash Translation Layer)  

## What is FTL?
Refer it: https://tech.kakao.com/2016/07/13/coding-for-ssd-part-3/  

## Configuration
You can also try to implementation `flt_blockmap` using `fltsim.tar` file. This is init tar file. Try it!

1. [`header.h`](https://github.com/softho0n/BlockMappingTable/blob/main/header.h) : This is header file. You can modify only capacity of nand.
2. [`main.c`](https://github.com/softho0n/BlockMappingTable/blob/main/main.c) : Set ftl mode, option and input files path. You cannot modify.
3. [`ftl_pagemap.c`](https://github.com/softho0n/BlockMappingTable/blob/main/ftl_pagemap.c) : page-level mapping scheme FTL. Refer ftl_pagemap code.
4. [`ftl_blockmap.c`](https://github.com/softho0n/BlockMappingTable/blob/main/ftl_blockmap.c) : block-level mapping schem FTL. Code it! (My version)
5. [`nand.c`](https://github.com/softho0n/BlockMappingTable/blob/main/nand.c) : Define NAND Flash and NAND Flash operation. You cannot modify

## How to run?
### Mode 0
In Mode 0, you have to specify input file path. Input file has only random LPN number
```console
foo@bar:~$ ./make.sh
foo@bar:~$ ./ftlsim 0 <filename>
```
### Mode 1
In Mode 1, Program generates LPN number automatically.
```console
foo@bar:~$ ./make.sh
foo@bar:~$ ./ftlsim 1 <random request count>
```

## Result
<p align="center"><img width="75%" src="https://user-images.githubusercontent.com/42256738/151653569-758bb3ae-8bdb-4b45-9133-5ca4d48edfdc.jpeg"/></p>

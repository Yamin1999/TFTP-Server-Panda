<p align="center">
  <img src="icons/Panda.png" alt="Panda Logo" height="150dp">
</p>

<h1 align="center">Panda</h1>

---
```
            ____    _    _   _   ____    _    
           |  _ \  / \  | \ | | |  _ \  / \   
           | |_) |/ _ \ |  \| | | | | |/ _ \
           |  __// ___ \| |\  | | |_| / ___ \
           |_|  /_/   \_\_| \_| |____/_/   \_\

+------------------------------------------------------+
|                     Panda v1.0.1                     |
+------------------------------------------------------+
|                                                      |
|   Md. Yamin Haque                                    |
|   R&D Engineer                                       |
|   Shanghai BDCOM Information Technology Co., Ltd.    |
|   Bangladesh Office                                  |
|   yamin.haque@bdcom.com.cn                           |
|                                                      |
+------------------------------------------------------+
TFTP server : Panda listening on port 69
```
---
## Description

This is a basic independent TFTP server following the protocol codified in [RFC1350](https://datatracker.ietf.org/doc/html/rfc1350). It also support protocol extensions defined by [RFC2347](https://datatracker.ietf.org/doc/html/rfc2347), [RFC2348](https://datatracker.ietf.org/doc/html/rfc2348) and [RFC2349](https://datatracker.ietf.org/doc/html/rfc2349).

For details refer to check the refererncs directory.


## Features

### General Features of Panda

1. TFTP server for file transfer
2. Supports read and write operations
3. Concurrent read/write sessions
4. Option negotiation (block size, transfer size, timeout)
5. Timeout and retry mechanisms for reliable transfers
6. Progress bar for a single active session (read or write)

### Potential Upcoming Features

1. Progress tracking for multiple concurrent sessions
2. Configurability (settings, allowed clients)
3. Performance optimizations
4. Support for additional TFTP extensions
5. Graphical User Interface (GUI)
6. Integration with other systems/services

### Project structure
```
TFTP |--icons/         Contains icons and images
     |--include/       Contains header files (.h)
     |--res/           Contains resource files
     |--references/    Contains reference PDF files
     |--src/           Contains source code files (.c)
     |--Makefile       Project Makefile
     |--LICENSE        GNU Licenses
``` 

## Compilation

```
$ make
mkdir obj
gcc -Wall -Wextra -g -Iinclude -MMD -MP -c src/main.c -o obj/main.o
gcc -Wall -Wextra -g -Iinclude -MMD -MP -c src/tftp_read.c -o obj/tftp_read.o
gcc -Wall -Wextra -g -Iinclude -MMD -MP -c src/tftp_server.c -o obj/tftp_server.o
gcc -Wall -Wextra -g -Iinclude -MMD -MP -c src/tftp_utils.c -o obj/tftp_utils.o
gcc -Wall -Wextra -g -Iinclude -MMD -MP -c src/tftp_write.c -o obj/tftp_write.o
windres res/tftpd_icon.rc -o obj/tftpd_icon.o
windres res/tftpd_version.rc -o obj/tftpd_version.o
gcc obj/main.o obj/tftp_read.o obj/tftp_server.o obj/tftp_utils.o obj/tftp_write.o obj/tftpd_icon.o obj/tftpd_version.o -o Panda -lws2_32
```

No clean up is made after compilation. You can manually do it using:
```
$ make clean
rm -rf obj Panda obj/main.d obj/tftp_read.d obj/tftp_server.d obj/tftp_utils.d obj/tftp_write.d
rm -f obj/tftpd_icon.o obj/tftpd_version.o
```

## Usages

### Write request
```
Session 0 : Request for write file : switch.bin from 192.168.0.103
Session 0 : Received 6887 blocks: 3525996 bytes
Session 0 : Transfer complete 3525996 bytes to Panda at: Sat Aug 10 17:32:25 2024
```

### Read request
```
Session 0 : Request for read file : switch.bin from 192.168.0.103
Session 0 : Progress: [##################################################] 100.00%
Session 0 : Transfer complete 3525996 bytes from Panda at: Sat Aug 10 17:39:08 2024
```
### Retransmission and Timeout

```
Session 0 : Request for read file : switch.bin from 192.168.0.103
Session 0 : Progress: [##################                                ] 37.55%
Session 0 : TFTP timeout!
```

### Error Handling
Read request in netascii mode.
```
Session 0 : Request for read file : switch.bin from 192.168.0.103
Session 0 : Olny support octet mode!
```
Write request for the file which is already exists in the directory.
```
Session 0 : Request for write file : switch.bin from 192.168.0.103
Session 0 : File switch.bin is already exists in the current directory.
```
Read request for the file which is not exists in the directory.
```
Session 0 : Request for read file : switch.tar from 192.168.0.103
Session 0 : File switch.tar does not exist in the current directory.
```


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


```
---
## Description

This is a basic independent TFTP server following the protocol codified in [RFC1350](https://datatracker.ietf.org/doc/html/rfc1350). It also supports protocol extensions defined by [RFC2347](https://datatracker.ietf.org/doc/html/rfc2347), [RFC2348](https://datatracker.ietf.org/doc/html/rfc2348) and [RFC2349](https://datatracker.ietf.org/doc/html/rfc2349).

For details refer to the references directory.

## Features

### General Features of Panda

1. TFTP server for file transfer
2. Supports read and write operations
3. Concurrent read/write sessions
4. Option negotiation (block size, transfer size, timeout)
5. Timeout and retry mechanisms for reliable transfers
6. **Transfer duration tracking** - Times each transfer session
7. **Enhanced session logging** - Tabular display of active and completed sessions
8. **Real-time progress tracking** - Shows progress bars and statistics during transfers
9. **Formatted time display** - Shows duration in appropriate units (ms, seconds, minutes)

### Session Tracking Display

The server now displays a detailed session table showing:
- Session number
- Filename being transferred
- Client IP address
- Operation type (READ/WRITE)  
- Bytes transferred
- Transfer duration

Example session table:
```
=============================================================================================================
| No |    Filename                                     |   Client IP       | Op     | Bytes      | Duration |
=============================================================================================================
| 1  |    switch.bin                                   |   192.168.0.103   | READ   | 3525996    | 2.45 s   |
-------------------------------------------------------------------------------------------------------------
```


### Potential Upcoming Features

1. Progress tracking for multiple concurrent sessions
2. Configurability (settings, allowed clients)
3. Performance optimizations
4. Support for additional TFTP extensions
5. Graphical User Interface (GUI)
6. Integration with other systems/services

### Project Structure
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

```bash
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

Clean up after compilation:
```bash
$ make clean
rm -rf obj Panda obj/main.d obj/tftp_read.d obj/tftp_server.d obj/tftp_utils.d obj/tftp_write.d
rm -f obj/tftpd_icon.o obj/tftpd_version.o
```

## Usage Examples

### Write Request with Session Tracking
```
Session 0 : Request for write file : switch.bin from 192.168.0.103
Session 0 : Received 6887 blocks: 3525996 bytes

=============================================================================================================
| No |    Filename                                     |   Client IP       | Op     | Bytes      | Duration |
=============================================================================================================
| 1  |    switch.bin                                   |   192.168.0.103   | WRITE  | 3525996    | 1.23 s   |
-------------------------------------------------------------------------------------------------------------
```

### Read Request with Progress Bar
```
Session 0 : Request for read file : switch.bin from 192.168.0.103
Session 0 : Progress: [##################################################] 100.00%

=============================================================================================================
| No |    Filename                                     |   Client IP       | Op     | Bytes      | Duration |
=============================================================================================================
| 1  |    switch.bin                                   |   192.168.0.103   | READ   | 3525996    | 2.45 s   |
-------------------------------------------------------------------------------------------------------------
```

### Retransmission and Timeout
```
Session 0 : TFTP timeout!
```

### Error Handling

**Unsupported mode (netascii):**
```
-------------------------------------------------------------------------------------------------------------
| 1  |  Only support octet mode!
-------------------------------------------------------------------------------------------------------------
```

**File already exists (write):**
```
-------------------------------------------------------------------------------------------------------------
| 1  |  File switch.bin is already exists in the current directory.
-------------------------------------------------------------------------------------------------------------
```

**File not found (read):**
```
-------------------------------------------------------------------------------------------------------------
| 1  |  File switch.tar does not exist in the current directory.
-------------------------------------------------------------------------------------------------------------
```

## Version History

- **v1.0.2** - Added transfer duration tracking and enhanced session logging
- **v1.0.1** - Initial release with basic TFTP functionality
- **v1.0.0** - Base implementation

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.

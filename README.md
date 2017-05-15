# GetDiskInfo
A Windows command line tool to read low level disk information.
It supports both MBR and GPT and it requires administrative privileges in order to run.
Here is a typical output:
```
Information for disk \Device\Harddisk0

Disk Properties :

Cylinders       = 91201
Tracks/cylinder = 255
Sectors/track   = 63
Bytes/sector    = 512
Media Type      = Fixed
Disk size       = 750156374016 (Bytes) = 698 (Gb)


List of Partitions:

Number    Type    Start Offset        Length
======    ====    =============    =============
     1     MBR          1048576        104857600
     2     MBR        105906176     330620207104
     3     MBR     330726113280     419427254272
```


**OnionHEN Kernel Log Plugin**  
A managed PS5 kernel log server with a native OnionHEN dynamic settings UI


This repository packages the PS5 `klogsrv` payload as a standalone OnionHEN
plugin. The upstream prebuilt payload ELF (`assets/klogsrv-ps5.elf`) 


The plugin process starts automatically so it can publish its settings page.
The kernel log server is enabled by default and listens on TCP `3232` (the
klogsrv upstream default).

## Install

/data/OnionHEN/plugins/KLOG00001.elf



## Credits 

This plugin is based on
[`ps5-payload-dev/klogsrv`](https://github.com/ps5-payload-dev/klogsrv) 

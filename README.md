These sources are for the Cromwell derivative bootloader only.
We have modified the cromwell to load a 
secondary binary from flash, to the traditional vmlinuz 
entrypoint, 0x00100000, and call this address.  The call 
uses a simple function pointer, and passes a structure, 
"internals" which contains both function and data pointers 
back into cromwell.  In this way, XeniumOS can make calls back to 
Cromwell for various hardware accesses, allowing Crom 

The XeniumOS and XeniumUI directly contain no GPL code, and remain 
closed source.  The only access XeniumOS and XeniumUI have to any GPL 
code, Cromwell, is via the set of callbacks provided by the 
loader, found in xcallout.c.  
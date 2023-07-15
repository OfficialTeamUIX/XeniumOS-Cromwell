/**************************************************************************
 *
 *  "BIOS image as binary resource" ld script -
 *      (C) Milosch Meriac <xboxlinux@meriac.de> 2002
 *
 *  $log:$
 *
 **************************************************************************/

SECTIONS {
    .backdrop_focus (0x0) : AT (0x0) {

       *(.data);
    }
}

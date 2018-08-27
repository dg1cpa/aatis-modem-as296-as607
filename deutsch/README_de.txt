* Date: 13.02.07
* Author: DG1CPA@DB0CHZ.#SAX.DEU.EU
*
* OS: Linux, unix?, bsd?, macos?
* The driver is designed to run on other OS's(needs libusb), but need little bit work
* Der Treiber ist nicht vom Kernel abhängig, sondern nutzt die libusb Schnittstelle!
*
* Ham Radio Modem Treiber
* für AS296 oder AS607 USB-Modem von AAtis (9600bd FSK, 4800bd FSK, 1200bd AFSK)
*
* Dieser Treiber verbindet ein AS296 USB Modem an eine tty (pseudo-tty)
* Dadurch ist es möglich das Modem über kissattach, mit dem AX25-Kernel zu verbinden,
* indem man kissattach die andere Seite der pseudo-tty als "Serielle Schnittstelle" angibt
*
* Ein weitere Spezialanwendung ist die Verbindung zu XNET, TNN?, wampes?
* Die jeweilige Anwendung muss hier die "rmnc-crc" Kiss-Checksumme beherrschen (das Modem kann nur diese)
*
* Das steht unter der Software Lizenz GPL, version 3.
* Und kann damit frei kopiert werden
*
* Download: git clone https://github.com/dg1cpa/aatis-modem-as296-as607.git

Übersicht Schema(veraltet mit alten pseudotty's, heute: /dev/ptmx /dev/pts/*):
(nicht vollständig)
 [AS296-USB-Modem]->[Kernel:USB]->USBLib->[usb-as296-to-tty-d]->pseudotty->[Kernel]->pseudottyoutput->kissattach->[Kernel:ax25]


                                                   [Packet Terminal (Linkt)]
                                                                        |
                                       [kissattach]                     |
        +-[usb-as296-to-tty-d]-+   +----------------+                   |
        |           /dev/ptyp9 |   | /dev/ttyp9     |(ax0)              |
 -------------------------------------------------------------------------------
 | USB-Lib-API               PSEUDO-TTY     AX25/Kiss(rmnc-CRC)       ax25k    |
 |                          Linux-Kernel                                       |
 | USB-Host-driver                                                             |
 -------------------------------------------------------------------------------
         |
  [AS296-USB-Modem]


----------------------------------------------------
Zusatzinfos: 

Pseudotty: Eine spezial tty
Jede tty hat eine 2. tty mit der gleichen Nummer. 
Wenn man in die eine hinein schreibt, kann man kann man das auf der anderen Seite lesen, und umgekehrt.
 /dev/ptyp7 <-> /dev/ttyp7

HID: Human Input Device (USB)
z.B. Maus, Keyboard etc...


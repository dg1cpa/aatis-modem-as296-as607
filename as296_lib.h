
/* as296_lib.h
 *
 * Date: 30.03.06
 * Programmer: dg1cpa@web.de
 *
 * Ham Radio Modem driver
 * for AS296 USB-Modem from AAtis (9600bd FSK, 4800bd FSK, 1200bd AFSK)
 *
 * This driver connects a AS296 USB Modem to a tty (pseudo-tty)
 * from the other side of the pseudo-tty you can connect a kiss-driver (kissattach)
 * The kissdriver has to use the "rmnc-crc" protocol
 *
 * This program is distributed under the GPL, version 2.
 */


#ifndef __as296_lib_h__
#define __as296_lib_h__

#include <usb.h>
 
#define FEND 0xC0     // Kiss Frame Ende
 
#define FALSE 0
#define TRUE (!FALSE)
typedef unsigned int boolean_t;

enum modem_mode_list{
    MODEM_MODE_9600_BAUD,
    MODEM_MODE_9696_BAUD = MODEM_MODE_9600_BAUD,
    MODEM_MODE_9612_BAUD,
    MODEM_MODE_1296_BAUD,
    MODEM_MODE_1200_BAUD,
    MODEM_MODE_1212_BAUD = MODEM_MODE_1200_BAUD,
    MODEM_MODE_4800_BAUD,
    MODEM_MODE_4848_BAUD = MODEM_MODE_4800_BAUD,
    MODEM_MODE_POCS,
    MODEM_MODE_POCI, //iNVERSE?
};
 
//struct for program options
typedef struct {
    unsigned char mode; ///Modem and baud rate mode
    unsigned char debug; ///debug verbosity
    unsigned int txdelay; ///send delay time
    int persistence;    ///how often we randomize the media access 255 much
    int slottime;       ///
    int dama;           ///digi is dama
    char *ptty;         ///point to tty name and path
    char *newPttyNameSaveFileName;      ///file name where the opened other side of new pseudo tty system should be saved after opening
    int numvendor;      ///vendornum of the usb device
    int numproduct;     ///
}options_t;
 
struct as296_context {
    // USB specific
    struct usb_dev_handle *usb_dev;
    int usb_read_timeout;
    int usb_write_timeout;

    char as296_mode;
    int as296_txdelay;

    int interface;   // 0 or 1

    // Endpoints
    int in_ep;       //0x81
    int out_ep;      // 2
    char *error_str;
};

struct as296_device_list {
    struct as296_device_list *next;
    struct usb_device *dev;
};

typedef struct {
    struct as296_context as296c;
    char modemstatus;
    char modemversion;
    char resyncCounter;
    char software_state_parent; //0=ok 1=error
    char software_state_child;
}mmap_type;

#ifdef __cplusplus
extern "C" {
#endif

    int as296_init(struct as296_context *as296);

    void as296_deinit(struct as296_context *as296);
    void as296_set_usbdev (struct as296_context *as296, usb_dev_handle *usbdev);
    
    int as296_usb_find_all(struct as296_context *as296, struct as296_device_list **devlist,
                          int vendor, int product);
    void as296_list_free(struct as296_device_list **devlist);
    
    int as296_usb_open(struct as296_context *as296, int vendor, int product);
    int as296_usb_open_desc(struct as296_context *as296, int vendor, int product,
                           const char* description, const char* serial);
    int as296_usb_open_dev(struct as296_context *as296, struct usb_device *dev);
    
    int as296_usb_close(struct as296_context *as296);

    int as296_set_baudrate(struct as296_context *as296);
    int as296_read_data(struct as296_context *as296, char *buf, int size);
    int as296_write_data(struct as296_context *as296, char *buf, int size);

    char *as296_get_error_string(struct as296_context *as296);

#ifdef __cplusplus
}
#endif

/** initialise the cmd line options
 * @param argc how many options (source)
 * @param argv pointer to option array (source)
 * @param options pointer to option struct (destination)
 * @return success TRUE
 */
boolean_t lib_as296_parseCmdLineOptions(int argc, char **argv, options_t *options);


#endif /* __as296_lib_h__ */

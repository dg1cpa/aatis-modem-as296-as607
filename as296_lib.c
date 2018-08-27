
/* as296_lib.c
 *
 * Date: 31.03.06
 * Programmer: DG1CPA@DB0CHZ.#SAX.DEU.EU
 *
 * Ham Radio Modem driver
 * for AS296/AS607 USB-Modem from AAtis (9600bd FSK, 4800bd FSK, 1200bd AFSK)
 *
 * This driver connects a AS296/AS607 USB Modem to a tty (pseudo-tty)
 * from the other side of the pseudo-tty you can connect a kiss-driver (kissattach)
 * The kissdriver has to use the "rmnc-crc" protocol
 *
 * This program is distributed under the GPL, version 2.
 */


#include <usb.h>
#include <string.h>
#include <stdio.h>
#include "as296_lib.h"

#define as296_error_return(code, str) do {  \
        as296->error_str = str;             \
        return code;                       \
   } while(0);                 


/* as296_init

  Initializes a as296_context.

  Return codes:
   0: All fine
  -1: Couldn't allocate read buffer
*/
int as296_init(struct as296_context *as296)
{
    as296->usb_dev = NULL;
    as296->usb_read_timeout = 5000; //5000==5sekunden
    as296->usb_write_timeout = 0;

    as296->interface = 0;

    //Endaddressen fuer read/write
    as296->in_ep = 0x81;
    as296->out_ep = 0x02;

    as296->as296_mode = 0;
    as296->as296_txdelay = 0;
    
    as296->error_str = NULL;

    /* All fine. Now allocate the readbuffer */
    return 1; 
}

/* as296_set_usbdev
 
   Use an already open device.
*/
void as296_set_usbdev (struct as296_context *as296, usb_dev_handle *usb)
{
    as296->usb_dev = usb;
}


/* as296_usb_find_all
 
   Finds all as296 devices on the usb bus. Creates a new as296_device_list which
   needs to be deallocated by as296_list_free after use.

   Return codes:
    >0: number of devices found
    -1: usb_find_busses() failed
    -2: usb_find_devices() failed
    -3: out of memory
*/
int as296_usb_find_all(struct as296_context *as296, struct as296_device_list **devlist, int vendor, int product) 
{
    struct as296_device_list **curdev;
    struct usb_bus *bus;
    struct usb_device *dev;
    int count = 0;
    
    usb_init();
    if (usb_find_busses() < 0)
        as296_error_return(-1, "usb_find_busses() failed");
    if (usb_find_devices() < 0)
        as296_error_return(-2, "usb_find_devices() failed");

    curdev = devlist;
    for (bus = usb_busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == vendor
                    && dev->descriptor.idProduct == product)
            {
                *curdev = (struct as296_device_list*)malloc(sizeof(struct as296_device_list));
                if (!*curdev)
                    as296_error_return(-3, "out of memory");
                
                (*curdev)->next = NULL;
                (*curdev)->dev = dev;

                curdev = &(*curdev)->next;
                count++;
            }
        }
    }
    
    return count;
}

/* as296_list_free

   Frees a created device list.
*/
void as296_list_free(struct as296_device_list **devlist) 
{
    struct as296_device_list **curdev;
    for (; *devlist == NULL; devlist = curdev) 
    {
        curdev = &(*devlist)->next;
        free(*devlist);
    }

    devlist = NULL;
}

/* as296_usb_open_dev 

   Opens a as296 device given by a usb_device.
   
   Return codes:
     0: all fine
    -4: unable to open device
    -5: unable to claim device
    -6: reset failed
    -7: set baudrate failed
*/
int as296_usb_open_dev(struct as296_context *as296, struct usb_device *dev)
{
    int test=0;
    
    if (!(as296->usb_dev = usb_open(dev)))
        as296_error_return(-4, "usb_open() failed");
#if defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP)   
    //Problem: the modem looks like a HID device, so its connected automaticly to usbhid.ko
    //so we need to disconnect it fro the HID driver (it could be, this dont work in bsd/macosx?)
    if(0 != (test = usb_detach_kernel_driver_np(as296->usb_dev,0))) //disconnect the modem from usbhid.ko WE ARE NO KEYBOARD!!!
    {
//        printf("%d remove kerneldriver 2nd times?\n",test);
    }
#endif
    
    if (usb_claim_interface(as296->usb_dev, as296->interface) != 0) 
    {
        usb_close (as296->usb_dev);
        as296_error_return(-5, "unable to claim usb device.");
    }

    if (as296_set_baudrate (as296) != 0) 
    {  // baudrate is to set in the as296 structur
        usb_close (as296->usb_dev);
        as296_error_return(-7, "set baudrate failed");
    }

    as296_error_return(0, "all fine");
}

/* as296_usb_open
   
   Opens the first device with a given vendor and product ids.
   
   Return codes:
   See as296_usb_open_desc()
*/  
int as296_usb_open(struct as296_context *as296, int vendor, int product)
{
    return as296_usb_open_desc(as296, vendor, product, NULL, NULL);
}

/* as296_usb_open_desc

   Opens the first device with a given, vendor id, product id,
   description and serial.
   
   Return codes:
     0: all fine
    -1: usb_find_busses() failed
    -2: usb_find_devices() failed
    -3: usb device not found
    -4: unable to open device
    -5: unable to claim device
    -6: reset failed
    -7: set baudrate failed
    -8: get product description failed
    -9: get serial number failed
    -10: unable to close device
*/
int as296_usb_open_desc(struct as296_context *as296, int vendor, int product,
                       const char* description, const char* serial)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    char string[256];

    usb_init();

    if (usb_find_busses() < 0)
        as296_error_return(-1, "usb_find_busses() failed");
    if (usb_find_devices() < 0)
        as296_error_return(-2, "usb_find_devices() failed");

    for (bus = usb_busses; bus; bus = bus->next) 
    {
        for (dev = bus->devices; dev; dev = dev->next) 
        {
            if (dev->descriptor.idVendor == vendor
                    && dev->descriptor.idProduct == product) 
            {
                if (!(as296->usb_dev = usb_open(dev)))
                    as296_error_return(-4, "usb_open() failed");

                if (description != NULL) 
                {
                    if (usb_get_string_simple(as296->usb_dev, dev->descriptor.iProduct, string, sizeof(string)) <= 0) 
                    {
                        usb_close (as296->usb_dev);
                        as296_error_return(-8, "unable to fetch product description");
                    }
                    if (strncmp(string, description, sizeof(string)) != 0) 
                    {
                        if (usb_close (as296->usb_dev) != 0)
                            as296_error_return(-10, "unable to close device");
                        continue;
                    }
                }
                if (serial != NULL) 
                {
                    if (usb_get_string_simple(as296->usb_dev, dev->descriptor.iSerialNumber, string, sizeof(string)) <= 0) 
                    {
                        usb_close (as296->usb_dev);
                        as296_error_return(-9, "unable to fetch serial number");
                    }
                    if (strncmp(string, serial, sizeof(string)) != 0) 
                    {
                        if (usb_close (as296->usb_dev) != 0)
                            as296_error_return(-10, "unable to close device");
                        continue;
                    }
                }

                if (usb_close (as296->usb_dev) != 0)
                    as296_error_return(-10, "unable to close device");
                
                return as296_usb_open_dev(as296, dev);
            }
        }
    }

    // device not found
    as296_error_return(-3, "device not found");
}

/* as296_usb_close
   
   Closes the as296 device.
   
   Return codes:
     0: all fine
    -1: usb_release failed
    -2: usb_close failed
*/
int as296_usb_close(struct as296_context *as296)
{
    int rtn = 0;

    if (usb_release_interface(as296->usb_dev, as296->interface) != 0)
        rtn = -1;

    if (usb_close (as296->usb_dev) != 0)
        rtn = -2;

    return rtn;
}


/*
    as296_set_baudrate - Sets channel baudrate and mode #todo
    
    Return codes:
     0: all fine
    -1: invalid baudrate
    -2: setting baudrate failed
*/
int as296_set_baudrate(struct as296_context *as296)
{
    unsigned char as296_config[8]={0};

    as296_config[0]=0x10|as296->as296_mode;  //0x10|Mode 0=9k6 3=1k2 ...
    as296_config[1]=as296->as296_txdelay;    //txd96 jeweils durch 10
    as296_config[2]=as296->as296_txdelay;    //txd12
    as296_config[3]=2;                       //rxd96
    as296_config[4]=2;                       //rxd12
    as296_config[5]=as296->as296_txdelay;    //txd48
    as296_config[6]=2;                       //rxd48
    as296_config[7]=0xC0;                    //FEND

    int ret = usb_control_msg(
                as296->usb_dev,                                  // usb_dev_handle *dev
                USB_TYPE_CLASS|USB_RECIP_INTERFACE,             //0x00000021,                                     // int requesttype c8 (0x01<<5 TYPE_CLASS)|(0x01 RECIP_INTERFACE)
                USB_REQ_SET_CONFIGURATION,                      //0x00000009,                                     // int request 12 (9=set_config)
                0x0300,                                              // int value
                0,                                              // int index
                (char *) as296_config,                          // char *bytes
                0x00000008,                                     // int size
                5000                                            // int timeout
        );

    if(ret != 8) 
        as296_error_return (-2, "Setting new config/baudrate failed");
    
//    if (ret == 8) printf("Baudrate/Mode ist set\n");
    return 0;
}


int as296_write_data(struct as296_context *as296, char *buf, int size)
{
    int ret;

    ret = usb_interrupt_write(as296->usb_dev, as296->out_ep, buf, size, as296->usb_write_timeout);
    if (ret < 0)
        as296_error_return(ret, "usb bulk write failed");

    return ret;
}

int as296_read_data(struct as296_context *as296, char *buf, int size)
{
    int ret;

        /* returns how much received */
//        printf("usbdev=%X in_ep=%X buf=%X size=%d timeout=%d\n",(int)as296->usb_dev, as296->in_ep, (int)buf, size, as296->usb_read_timeout);
        ret = usb_interrupt_read (as296->usb_dev, as296->in_ep, buf, size, as296->usb_read_timeout);

        if (ret < 0)
            as296_error_return(ret, "usb bulk read failed");

        return ret;
}


char *as296_get_error_string (struct as296_context *as296)
{
    return as296->error_str;
}

/** initialise the cmd line options
 * @param argc how many options (source)
 * @param argv pointer to option array (source)
 * @param options pointer to option struct (destination)
 * @return success TRUE
 */
boolean_t lib_as296_parseCmdLineOptions(int argc, char **argv, options_t *options) 
{
    int optres, len;

    //parse command line options
    while ((optres = getopt (argc, argv, "m:t:hy:d:s:p:D:u:Y:")) != -1) {
	    switch (optres)
        {
            case 'm':
                printf("selected mode %s\n",optarg);
                len = strlen(optarg);
                if      (!strncasecmp(optarg, "9600", len))
                    options->mode = MODEM_MODE_9600_BAUD;
                else if (!strncasecmp(optarg, "9696", len))
                    options->mode = MODEM_MODE_9696_BAUD;
                else if (!strncasecmp(optarg, "9612", len))
                    options->mode = MODEM_MODE_9612_BAUD;
                else if (!strncasecmp(optarg, "1296", len))
                    options->mode = MODEM_MODE_1296_BAUD;
                else if (!strncasecmp(optarg, "1200", len))
                    options->mode = MODEM_MODE_1200_BAUD;
                else if (!strncasecmp(optarg, "1212", len))
                    options->mode = MODEM_MODE_1212_BAUD;
                else if (!strncasecmp(optarg, "4800", len))
                    options->mode = MODEM_MODE_4800_BAUD;
                else if (!strncasecmp(optarg, "4848", len))
                    options->mode = MODEM_MODE_4848_BAUD;
                else if (!strncasecmp(optarg, "pocs", len))
                    options->mode = MODEM_MODE_POCS;
                else if (!strncasecmp(optarg, "poci", len))
                    options->mode = MODEM_MODE_POCI;
                else {printf("invalid mode - modes available:\n9600\n1200\n4800\n9696\n9612\n1296\n1212\n4848\npocs\npoci\n"); return FALSE;}
                break;
	
            //DAMA
            case 'D':
                if(atoi(optarg)>0)
                {
                    printf("use DAMA parameter\n");
                    options->persistence=255;
                    options->slottime=1;
                    options->dama=1;
                }
            break;
	     
            //slottime
            case 's':
                if(options->dama==1) 
                    break; //if we have dama we dont need it
                options->slottime = atoi(optarg);
                if ((options->slottime < 2000) && (options->slottime >= 0 ))
                {
                    printf("use slottime %d ms\n", options->slottime);
                }
                else
                {
                    printf("Error: please set a slottime less than 2000 ms\n");
                    return FALSE;
                }
            break;
	
            //persistence
            case 'p':
                if(options->dama==1) 
                    break; //bei dama we dont need it
                options->persistence = atoi(optarg);
                if ((options->persistence < 256) & (options->persistence >= 0 ))
                {
                    printf("use persistence %d\n", options->persistence);
                }
                else
                {
                    printf("Error: please set a persistence 0...255\n");
                    return FALSE;
                }
            break;
            //txdelay
            case 't':
                options->txdelay = atoi(optarg)/10;
                printf("set txdelay %s ms\n", optarg);
                break;
            //debug
            case 'd':
                options->debug = atoi(optarg);
                printf("set debug mode %d\n", options->debug);
                break;
            //tty name
            case 'y':
                options->ptty = optarg;
                printf("use old pseudotty %s\n", options->ptty);
                break;
	     
            //new ptty name
            case 'Y':
                options->newPttyNameSaveFileName = optarg;
                printf("path where the name of the new pseudotty should be saved %s\n", options->newPttyNameSaveFileName);
                break;
         
            case 'u':
                if (2 != sscanf(optarg, "%X:%X", &options->numvendor, &options->numproduct))
                {
                    fprintf(stderr, "-u requires vendor:product format ->auto usb-id search\n");
                    options->numvendor = 0;
                    options->numproduct = 0;
                } 
                //   printf("usbid=%X:%X",numvendor,numproduct);
                return FALSE;
	
            //help
            case 'h':
                printf("usb-as296-to-tty-d -t TXDELAY -m MODE -y PTTY -Y newPtty -s Slottime -p Persistence -D DAMA -u USB_ID\nfor PTTY you can use a pseudo-tty example: /dev/ptyp9  ...(/dev/ptyp9 <-> /dev/ttyp9)\ntxdelay in ms\n");
                return FALSE;
        }
    }
	  
    if((options->ptty == NULL) && (options->newPttyNameSaveFileName == NULL))
    {
        printf("you need a kiss tty! (-y ...) for old pseudo tty or -Y for new pseudotty\n");
        return FALSE;
    }
    
    if((options->ptty != NULL) && (options->newPttyNameSaveFileName != NULL))
    {
        printf("you cant use param (-y ...) AND -Y this make no sense! read the help!\n");
        return FALSE;
    }
	  
    return TRUE;
 }

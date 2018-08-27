
/* as296_main.c
 *
 * Author: dg1cpa@web.de
 *
 * Ham Radio Modem driver
 * for AS296/AS607 USB-Modem from AAtis (9600bd FSK, 4800bd FSK, 1200bd AFSK)
 *
 * This driver connects a AS296/AS607 USB Modem to a tty (pseudo-tty)
 * from the other side of the pseudo-tty you can connect a kiss-driver (kissattach)
 * The kissdriver has to use the "rmnc-crc" protocol
 *
 * Todo:
 * Slottime implemention
 *
 * This program is distributed under the GPL, version 2.
 */

#define VERSION "0.4.1"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>
#include "as296_lib.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <pty.h>

void as296_fatal (struct as296_context *as296, char *str)
{
    fprintf (stderr, "%s: %s\n", str, as296_get_error_string (as296));
    exit (1); 
}

/** prints a timestamp to stdout
 */
void display_timestamp(void)
{
    time_t timenowx;
    struct tm *timenow;
    struct timeval tv;
    struct timezone tz;

    time(&timenowx);
    timenow = localtime(&timenowx);

    gettimeofday(&tv, &tz);
    printf("%02d:%02d:%02d:%03d ", timenow->tm_hour, timenow->tm_min, timenow->tm_sec, (int)tv.tv_usec/1000);
}

//print kiss packets
void printkiss(unsigned char *ptr,int len)
{
    int i;
    
    display_timestamp();
    for (i=0;i<len;i++)
        printf("%02X/",*ptr++);
    printf("\n");
}

/** gibt Zufallszahl aus /dev/urandom zurück
 * @param randfd opend filedescriptor to /dev/urandom
 * @return random number
 */
unsigned int random_num(int randfd)
{
    static unsigned int random_seed=0;
    //random_seed = 28629 * random_seed + 157;
    if(0>read(randfd, &random_seed, sizeof(random_seed)))
    {
        printf("can't read form /dev/urandom!");
    }
    return random_seed;
}

void open_modem(mmap_type *mmap_addr, int numvendor, int numproduct)
{
    int res;
    //search/wait for a modem as296 or as607
    do
    {
        //auto id search?
        if (numvendor == 0)
        {
            //probing modems
            res = as296_usb_open(&mmap_addr->as296c, 0x7355, 0x0296); //as296
            if(res < 0) res = as296_usb_open(&mmap_addr->as296c, 0x7355, 0x0607); //as607
        }
        else //direct id
        {
            res = as296_usb_open(&mmap_addr->as296c, numvendor, numproduct);
        }
        if (res < 0)
        {
            printf("wait for modem...\n");
            usleep(100); //if no modem sleep a moment
        }
    }while(res < 0);
    printf("found Modem\n");
    mmap_addr->software_state_parent = 0; //ok child process can continue
}

int main(int argc, char **argv)
{
    int fd_tty, aslave, fd, ret, randfd, rxsize, usb_write, txsize, resu;
    unsigned char rxdata[10], txdata[340*7];
    options_t options = {
        .mode = 10, .debug=0, .txdelay=22, .persistence=64, .slottime=100, .dama=0, .ptty = NULL, 
        .newPttyNameSaveFileName = NULL, .numvendor=0, .numproduct=0 };
    mmap_type *mmap_addr;
    caddr_t  adr;
    int errcount=0;
    struct termios tm;
    struct pollfd poll_fd0;
    char newPttyName[300];
    boolean_t readFirstTimeFromTty = FALSE;
    //we send a rmnc kiss packet to the kernel kiss modul to tell him we use kiss crc, else we send only 1 times (kissmodul tries all kiss crc variants)
    unsigned char rmnccrcPacket[]={0xC0, 0x20, 0x88, 0x84, 0x60, 0x86, 0x90, 0xB4, 0x74, 0x88, 0x8E, 0x62, 0x86, 0xA0, 0x82, 0xF5, 0x1F, 0xE7, 0x91, 0xC0}; 
//-------------------------------------------------------------DO1SJR-----
    int child_pid = 0;
    int wait_status;
//-------------------------------------------------------------DO1SJR-----

    printf("AS296 Modem driver - Version: %s\n", VERSION);
  
    if (FALSE == lib_as296_parseCmdLineOptions(argc, argv, &options))
    {
        printf("option Error - Program exit\n");
        exit(-1);
    }

    /* set tty mode to raw */
    memset(&tm, 0, sizeof(tm));
    tm.c_cflag = CS8 | CREAD | CLOCAL;
    
    if(options.newPttyNameSaveFileName != NULL)
    {
        printf("open new ptty\n");
        if(-1 == openpty(&fd_tty, &aslave, newPttyName, &tm, NULL))
        {
           printf("can't open new ptty - is the tty in use???\n");
           exit(-1);
        }
        else
        {
            fd = open(options.newPttyNameSaveFileName, O_RDWR|O_CREAT, 0600);
            if(fd < 0)
            {
                printf("cant open newPttyNameSaveFileName = %s for writing", options.newPttyNameSaveFileName);  
                exit(-1); 
            }
            printf("other side ptty name: %s\n", newPttyName); //print the name of the new opened ptty
            strcat(newPttyName, "\n");
            resu = write(fd, newPttyName, strlen(newPttyName)); 
            if(resu <= 0)
            {
                printf("Err: cant write tty name to tmp file!\n");
            }
            close(fd);
        }
    }
    else
    {
        //open pseudotty
        //open tty with O_NONBLOCK to prevent blocking problems with kissattach
        //              ->needs self block with poll()
        fd_tty = open(options.ptty, O_RDWR|O_NONBLOCK);
        if (fd_tty < 0)
        {
           printf("can't open tty - is the tty in use???\n");
           exit(-1);
        }
        
        
        if (tcsetattr(fd_tty, TCSANOW, &tm)) 
        {
           perror("master: tcsetattr");
           exit(-1);
        }
    }

    //set tty not blocking
    fcntl(fd_tty, F_SETFL, fcntl(fd_tty, F_GETFL) & ~O_NONBLOCK);
    
    //open random device
    if((randfd = open("/dev/urandom", O_RDONLY))<0)
    printf("Can't open /dev/urandom\n");
    
    //Shared memory init
    if((fd = open("/dev/zero", O_RDWR))<0)
    printf("Can't open /dev/zero\n");

    if((adr = mmap(0, sizeof(mmap_type), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) == (caddr_t)-1)
    printf("Can't init shared memory\n");
    
    mmap_addr = (mmap_type *)adr;
    close(fd); //Mapping ready
    
    sleep(6); //nötig? damit kissattach schnell genug startet  
    //init process fault check
    mmap_addr->software_state_parent = 0;
    mmap_addr->software_state_child = 0;
    
    as296_init(&mmap_addr->as296c);

    mmap_addr->as296c.as296_mode    = options.mode;    // mode 9600FSK usw...
    mmap_addr->as296c.as296_txdelay = options.txdelay;  
    
restart:    
    open_modem(mmap_addr, options.numvendor, options.numproduct);

    if (child_pid > 0) 
    {
        printf("Child (%d)\n", child_pid);
        if (wait(&wait_status) != child_pid) 
        {
            perror("wait()");
            return EXIT_FAILURE;
        }
        printf("Child is dead\n");
    }        
    
    child_pid = fork();
   
    switch(child_pid) //split process in rx-usb(parent) and tx-usb(child)
    {
/******* child process - USB-TX *****************************************/
     case -1:
        printf("Can't fork process\n");
        break;
        
     case 0: // child-process code tx-usb <- tty
        if(1 < options.debug) printf("TX child-process started (usb<-tty)\n");
        for(;;)
    	{ 
            if (readFirstTimeFromTty == FALSE)
            {
                //kiss modul should recognise we use rmnc kiss crc (kiss modul auto recognise it...)
                //else we can send only 1 packet. if get no answer we never send again because the kiss modul use non crc per default
                resu = write( fd_tty, rmnccrcPacket, sizeof(rmnccrcPacket) );
                if(resu <= 0)
                {
                    printf("Warn: cant write rmnccrc init to tty\n");
                }
                else
                {
                    readFirstTimeFromTty = TRUE;
                }
            }   
            
            //set tty filedescriptor poll struct
            poll_fd0.fd = fd_tty;
            poll_fd0.events = POLLIN|POLLPRI;
            poll_fd0.revents = 0;
            
            ret = poll(&poll_fd0, 1, 150000);	   
            //printf("Pollstat %X\n", poll_fd0.revents);
                
            if(1 == mmap_addr->software_state_parent)
            {
                printf("parent process signals a problem...     child->exit\n");
                return EXIT_FAILURE;
                //sleep(1);
                //continue;
            }
            
            if(ret == 0)
            {
                //printf("child poll timeout\n");
                continue; //poll timeout
            }
            
            if(ret < 0)
            {
                if (errno == EINTR)  continue; //wiederhole bei EINTR
                mmap_addr->software_state_child = 1; //sonst error
                printf("tty poll error\007\n");
            }
    
            if(poll_fd0.revents & POLLIN)
            {
                txsize = read(fd_tty, txdata, sizeof(txdata)-1);
                
                if (txsize < 0)
                {
                    mmap_addr->software_state_child = 1; //tell parent there is a problem
                    printf("tty read error (child)\007\n");
                    exit(EXIT_FAILURE);
                }
            
                if (txsize > 0)
                {
                    if (2 < options.debug)
                    {
                        printf("tx:Modemstat: %02X\n",mmap_addr->modemstatus);
                        printf("%d Bytes read from tty\n",txsize);
                        printkiss(txdata, txsize);
                    }
                
                    //check kiss-packet comming from tty?
    /*              if ( (txdata[0] != 0xC0) || (txsize < 4) || (txdata[txsize-1] != 0xC0) )
                    {
                        if (1<debug)
                        {
                            printf("wrong packet(no kiss) from tty! datasize=%d\n",txsize-1);
                        }
                        continue;
                    }*/
                    //printf("kisscheck ok!\n");
                    
                    //wait a slottime if a persistence try is negativ
                    //and do it again 
                    while((random_num(randfd) % 256) > options.persistence)
                    {
                        if(3 < options.debug) printf("wait one slottime [slottime=%d persistence=%d]\n", options.slottime, options.persistence);
                        usleep(options.slottime * 1000);
                    }
    
                    //wait DCD	      
                    while((mmap_addr->modemstatus & 0x04) && (!(mmap_addr->modemstatus & 0x08))) //wenn dcd & nicht TX ->warten
                    {
                        if(4 < options.debug) printf("DCD-Wait stat %02X\n", mmap_addr->modemstatus);
                        usleep(250); //250us
                    }
        
                    //Modem ready?
                    if(!(mmap_addr->modemstatus & 0x02)) //firmware 1.0g and later
                    {
                        //printf("tx\n");
                        usb_write = as296_write_data(&mmap_addr->as296c, (char *)txdata, txsize);
                    
                        if(usb_write < 0) 
                        {
                            if (0 < options.debug)
                            { 
                                display_timestamp();
                                printf("usbtxerr (-110==timeout) %d\n", usb_write);
                            }
                            
                            if (-110 != usb_write)
                            {
                                mmap_addr->software_state_child = 1; //signaling parent process error
                                printf("error child: as296 usbwrite process ->wait\n");
                                //sleep(1);
                                exit(EXIT_FAILURE);
                            }
                        }
                        //as296_fatal (&as296c, "USB tx error");
                        //printf("txready\n");
                    }
                    else
                    {
                        printf("Modem not ready!\n");
                    }
                }
            }
        }//for(;;)   
        break;
     
/*********** parent process - USB-RX *************************************************+++*/
    default: // parent process code rx-usb -> tty
     
        if(1 < options.debug) printf("RX parent-process activ (usb->tty)\n");
        
        for(;;)
        {
            rxsize = as296_read_data(&mmap_addr->as296c, (char *)rxdata, 8);
            
            if(1 == mmap_addr->software_state_child)
            {
                printf("child signals problem\007\n");
                as296_usb_close(&mmap_addr->as296c);
                usleep(200);
                goto restart;
                //exit(EXIT_FAILURE);           
            }
            
            if ((rxsize < 0) && (rxsize!= (-110))) // -110 ist timeout nach ca. 5sek ->timeout parameter
            {
                if (0 < options.debug) printf("usb_rx_ret=%d errcnt=%d\n", rxsize, errcount); //#,prob
                mmap_addr->software_state_parent = 1;
                printf("error parent: usb read process (restart - wait for connecting modem)\007\n");
                goto restart;
                //versuche modem wieder zu öffnen, wenn nicht warte
                //open_modem(mmap_addr, options.numvendor, options.numproduct);
                //continue;
            }
            if (rxsize > 0) 
            {
                if(2 < options.debug)
                {		   
                    printf("p:%02X\n", mmap_addr->modemstatus);
                    printf("%d Bytes read from usb\n", rxsize);
                    printkiss(rxdata, rxsize);
                }
                    
                //controllpaket from modem (DCD...)
                if ((rxdata[0] == FEND) && 
                    (rxdata[1] == 0x21) &&
                    (rxdata[7] == FEND))
                {
                                                            // [2] device address
                    mmap_addr->modemversion = rxdata[3]; // modem version
                    mmap_addr->modemstatus  = rxdata[4]; // tx and dcd status 0x04(dcd?) 0x08(modem_ptt?)
                                                            // [6]lsb [7]msb modem buffer
                    if (mmap_addr->modemversion > 5)
                    {
                        mmap_addr->resyncCounter = rxdata[5]; // resync counter, what todo with it???
                    }
                    continue;
                }
                if (write(fd_tty, rxdata, 8)<0) //#, ab und falsche rxsize, es sind in der regel immer 8    rxsize)<0)
                    printf("error parent: tty tx error\007\n");
            }       
    	}
    }
    return 0;
}

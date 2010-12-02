#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IOMedia.h>

typedef void (*ripcallback)(void *buffer, unsigned int sectors, void *user_data);
typedef void (*toccallback)(void *trackinfo, void *user_data);

static char *
drive_nodename(io_object_t drive)
{
    char *nodename = NULL;
    CFTypeRef ref = IORegistryEntryCreateCFProperty(drive,
        CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
    unsigned int len = (unsigned int)CFStringGetLength(ref) + 1;
    char buffer[len];
    if(ref == NULL) return NULL;
    //CFShow(ref);
    if(CFStringGetCString(ref, buffer, len, kCFStringEncodingASCII))
        nodename = strdup(buffer);
    CFRelease(ref);
    return nodename;
}

static int
iterate_devices(void (*callback)(char *, io_object_t, void *),
    void *user_data)
{
    CFMutableDictionaryRef matchdict = IOServiceMatching(kIOCDMediaClass);
    CFDictionarySetValue(matchdict,
        CFSTR(kIOMediaEjectableKey),
        kCFBooleanTrue);
    io_iterator_t iterator;
    io_object_t device;
    int num_devices_found = 0;
    char *nodename;

    if(IOServiceGetMatchingServices(kIOMasterPortDefault,
        matchdict, &iterator) != KERN_SUCCESS) return -1;

    while((device = IOIteratorNext(iterator)) != 0)
    {
        num_devices_found ++;
        nodename = drive_nodename(device);
        if(callback) callback(nodename, device, user_data);
        IOObjectRelease(device);
        free(nodename);
    }
    IOObjectRelease(iterator);
    return num_devices_found;
}

static int
readcd(int fd,
       void *buffer,
       unsigned int start_sector,
       unsigned int sector_count)
{
    dk_cd_read_t cd_read;
    int blocksize = kCDSectorSizeCDDA;

    bzero(&cd_read, sizeof(cd_read));
    bzero(buffer, blocksize * sector_count);

    cd_read.offset = blocksize * start_sector;
    cd_read.sectorArea = kCDSectorAreaUser;
    cd_read.sectorType = kCDSectorTypeCDDA;
    cd_read.buffer = buffer;
    cd_read.bufferLength = blocksize * sector_count;

    if (ioctl(fd, DKIOCCDREAD, &cd_read) == -1) return -1;

    return cd_read.bufferLength / blocksize;
}

static int
ripsectors(int fd,
           unsigned int sector_start,
           unsigned int sector_end,
           ripcallback callback,
           void *user_data)
{
    int result = 0;
    int16_t *buffer = NULL;
    unsigned int num_sectors = sector_end - sector_start + 1;
    unsigned int sectors_ripped = 0;
    unsigned int sectors_left = num_sectors;
    unsigned int buffer_len = num_sectors < 1024 ? num_sectors : 1024;
    unsigned int sectors_read = 0;

    buffer = calloc(buffer_len, kCDSectorSizeCDDA);
    if(buffer == NULL) return -1;

    if(ioctl(fd, DKIOCCDSETSPEED, kCDSpeedMax) == -1) return -1;

    while(sectors_ripped < num_sectors)
    {
        sectors_read = readcd(fd,
            buffer,
            sector_start + sectors_ripped,
            sectors_left > buffer_len ? buffer_len : sectors_left);
        if (sectors_ripped == -1){
            result = -1;
            break;
        }
        sectors_ripped += sectors_read;
        sectors_left -= sectors_read;
        callback(buffer, sectors_read, user_data);
    }
    free(buffer);
    return result;
}

static int
read_toc_descriptor(CDTOCDescriptor *descr)
{
    unsigned int session = descr->session;
    unsigned int number = descr->point;
    unsigned int first_sector = CDConvertMSFToLBA(descr->p);

    return 0;
}

static int
read_toc(int fd, toccallback callback, void *user_data)
{
    dk_cd_read_toc_t cd_read_toc;
    uint8_t buffer[2048];
    CDTOC *toc = NULL;
    unsigned int i;
    unsigned int descr_count;
    unsigned int num_tracks = 0;

    bzero(&cd_read_toc, sizeof(cd_read_toc));
    bzero(buffer, sizeof(buffer));

    cd_read_toc.format = kCDTOCFormatTOC;
    cd_read_toc.buffer = buffer;
    cd_read_toc.bufferLength = sizeof(buffer);

    if (ioctl(fd, DKIOCCDREADTOC, &cd_read_toc) == -1) return -1;
    toc = (CDTOC*)buffer;

    descr_count = CDTOCGetDescriptorCount(toc);
    for(i = 0; i < descr_count; i ++)
    {
        read_toc_descriptor(&toc->descriptors[i]);
        if (callback) callback(NULL, user_data);
    }

    return num_tracks;
}

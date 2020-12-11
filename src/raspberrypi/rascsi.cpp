//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ RaSCSI main ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "devices/disk.h"
#include "devices/sasihd.h"
#include "devices/scsihd.h"
#include "devices/scsihd_apple.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_nuvolink.h"
#include "controllers/scsidev_ctrl.h"
#include "controllers/sasidev_ctrl.h"
#include "gpiobus.h"
#include "spdlog/spdlog.h"

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define CtrlMax 8 // Maximum number of SCSI controllers
#define UnitNum 2 // Number of units around controller
#ifdef BAREMETAL
#define FPRT(fp, ...) printf(__VA_ARGS__)
#else
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__)
#endif // BAREMETAL

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static volatile BOOL running;  // Running flag
static volatile BOOL active;   // Processing flag
SASIDEV *ctrl[CtrlMax];        // Controller
Disk *disk[CtrlMax * UnitNum]; // Disk
GPIOBUS *bus;                  // GPIO Bus
#ifdef BAREMETAL
FATFS fatfs; // FatFS
#else
int monsocket;       // Monitor Socket
pthread_t monthread; // Monitor Thread
static void *MonThread(void *param);
#endif // BAREMETAL

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
    // Stop instruction
    running = FALSE;
}
#endif // BAREMETAL

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char *argv[])
{
    FPRT(stdout, "SCSI Target Emulator RaSCSI(*^..^*) ");
    FPRT(stdout, "version %01d.%01d%01d(%s, %s)\n",
         (int)((VERSION >> 8) & 0xf),
         (int)((VERSION >> 4) & 0xf),
         (int)((VERSION)&0xf),
         __DATE__,
         __TIME__);
    FPRT(stdout, "Powered by XM6 TypeG Technology / ");
    FPRT(stdout, "Copyright (C) 2016-2020 GIMONS\n");
    FPRT(stdout, "Connect type : %s\n", CONNECT_DESC);

    if ((argc > 1 && strcmp(argv[1], "-h") == 0) ||
        (argc > 1 && strcmp(argv[1], "--help") == 0))
    {
        FPRT(stdout, "\n");
        FPRT(stdout, "Usage: %s [-IDn FILE] ...\n\n", argv[0]);
        FPRT(stdout, " n is SCSI identification number(0-7).\n");
        FPRT(stdout, " FILE is disk image file.\n\n");
        FPRT(stdout, "Usage: %s [-HDn FILE] ...\n\n", argv[0]);
        FPRT(stdout, " n is X68000 SASI HD number(0-15).\n");
        FPRT(stdout, " FILE is disk image file.\n\n");
        FPRT(stdout, " Image type is detected based on file extension.\n");
        FPRT(stdout, "  hdf : SASI HD image(XM6 SASI HD image)\n");
        FPRT(stdout, "  hds : SCSI HD image(XM6 SCSI HD image)\n");
        FPRT(stdout, "  hdn : SCSI HD image(NEC GENUINE)\n");
        FPRT(stdout, "  hdi : SCSI HD image(Anex86 HD image)\n");
        FPRT(stdout, "  nhd : SCSI HD image(T98Next HD image)\n");
        FPRT(stdout, "  hda : SCSI HD image(APPLE GENUINE)\n");
        FPRT(stdout, "  mos : SCSI MO image(XM6 SCSI MO image)\n");
        FPRT(stdout, "  iso : SCSI CD image(ISO 9660 image)\n");

#ifndef BAREMETAL
        exit(0);
#endif // BAREMETAL
    }
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL Init()
{
    int i;

#ifndef BAREMETAL
    struct sockaddr_in server;
    int yes;

    // Create socket for monitor
    monsocket = socket(PF_INET, SOCK_STREAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = PF_INET;
    server.sin_port = htons(6868);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // allow address reuse
    yes = 1;
    if (setsockopt(
            monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        return FALSE;
    }

    // Bind
    if (bind(monsocket, (struct sockaddr *)&server,
             sizeof(struct sockaddr_in)) < 0)
    {
        FPRT(stderr, "Error : Already running?\n");
        return FALSE;
    }

    // Create Monitor Thread
    pthread_create(&monthread, NULL, MonThread, NULL);

    // Interrupt handler settings
    if (signal(SIGINT, KillHandler) == SIG_ERR)
    {
        return FALSE;
    }
    if (signal(SIGHUP, KillHandler) == SIG_ERR)
    {
        return FALSE;
    }
    if (signal(SIGTERM, KillHandler) == SIG_ERR)
    {
        return FALSE;
    }
#endif // BAREMETAL

    // GPIOBUS creation
    bus = new GPIOBUS();

    // GPIO Initialization
    if (!bus->Init())
    {
        return FALSE;
    }

    // Bus Reset
    bus->Reset();

    // Controller initialization
    for (i = 0; i < CtrlMax; i++)
    {
        ctrl[i] = NULL;
    }

    // Disk Initialization
    for (i = 0; i < CtrlMax; i++)
    {
        disk[i] = NULL;
    }

    // Other
    running = FALSE;
    active = FALSE;

    return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void Cleanup()
{
    int i;

    // Delete the disks
    for (i = 0; i < CtrlMax * UnitNum; i++)
    {
        if (disk[i])
        {
            delete disk[i];
            disk[i] = NULL;
        }
    }

    // Delete the Controllers
    for (i = 0; i < CtrlMax; i++)
    {
        if (ctrl[i])
        {
            delete ctrl[i];
            ctrl[i] = NULL;
        }
    }

    // Cleanup the Bus
    bus->Cleanup();

    // Discard the GPIOBUS object
    delete bus;

#ifndef BAREMETAL
    // Close the monitor socket
    if (monsocket >= 0)
    {
        close(monsocket);
    }
#endif // BAREMETAL
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void Reset()
{
    int i;

    // Reset all of the controllers
    for (i = 0; i < CtrlMax; i++)
    {
        if (ctrl[i])
        {
            ctrl[i]->Reset();
        }
    }

    // Reset the bus
    bus->Reset();
}

//---------------------------------------------------------------------------
//
//	List Devices
//
//---------------------------------------------------------------------------
void ListDevice(FILE *fp)
{
    int i;
    int id;
    int un;
    Disk *pUnit;
    Filepath filepath;
    BOOL find;
    char type[5];

    find = FALSE;
    type[4] = 0;
    for (i = 0; i < CtrlMax * UnitNum; i++)
    {
        // Initialize ID and unit number
        id = i / UnitNum;
        un = i % UnitNum;
        pUnit = disk[i];

        // skip if unit does not exist or null disk
        if (pUnit == NULL || pUnit->IsNULL())
        {
            continue;
        }

        // Output the header
        if (!find)
        {
            FPRT(fp, "\n");
            FPRT(fp, "+----+----+------+-------------------------------------\n");
            FPRT(fp, "| ID | UN | TYPE | DEVICE STATUS\n");
            FPRT(fp, "+----+----+------+-------------------------------------\n");
            find = TRUE;
        }

        // ID,UNIT,Type,Device Status
        type[0] = (char)(pUnit->GetID() >> 24);
        type[1] = (char)(pUnit->GetID() >> 16);
        type[2] = (char)(pUnit->GetID() >> 8);
        type[3] = (char)(pUnit->GetID());
        FPRT(fp, "|  %d |  %d | %s | ", id, un, type);

        // mount status output
        if (pUnit->GetID() == MAKEID('S', 'C', 'B', 'R'))
        {
            FPRT(fp, "%s", "HOST BRIDGE");
        }
        else if (pUnit->GetID() == MAKEID('S', 'C', 'N', 'L'))
        {
            FPRT(fp, "%s", "NuvoLink SC");
        }
        else
        {
            pUnit->GetPath(filepath);
            FPRT(fp, "%s",
                 (pUnit->IsRemovable() && !pUnit->IsReady()) ? "NO MEDIA" : filepath.GetPath());
        }

        // Write protection status
        if (pUnit->IsRemovable() && pUnit->IsReady() && pUnit->IsWriteP())
        {
            FPRT(fp, "(WRITEPROTECT)");
        }

        // Goto the next line
        FPRT(fp, "\n");
    }

    // If there is no controller, find will be null
    if (!find)
    {
        FPRT(fp, "No images currently attached.\n");
        return;
    }

    FPRT(fp, "+----+----+------+-------------------------------------\n");
}

//---------------------------------------------------------------------------
//
//	Controller Mapping
//
//---------------------------------------------------------------------------
void MapControler(FILE *fp, Disk **map)
{
    int i;
    int j;
    int unitno;
    int sasi_num;
    int scsi_num;

    // Replace the changed unit
    for (i = 0; i < CtrlMax; i++)
    {
        for (j = 0; j < UnitNum; j++)
        {
            unitno = i * UnitNum + j;
            if (disk[unitno] != map[unitno])
            {
                // Check if the original unit exists
                if (disk[unitno])
                {
                    // Disconnect it from the controller
                    if (ctrl[i])
                    {
                        ctrl[i]->SetUnit(j, NULL);
                    }

                    // Free the Unit
                    delete disk[unitno];
                }

                // Setup a new unit
                disk[unitno] = map[unitno];
            }
        }
    }

    // Reconfigure all of the controllers
    for (i = 0; i < CtrlMax; i++)
    {
        // Examine the unit configuration
        sasi_num = 0;
        scsi_num = 0;
        for (j = 0; j < UnitNum; j++)
        {
            unitno = i * UnitNum + j;
            // branch by unit type
            if (disk[unitno])
            {
                if (disk[unitno]->IsSASI())
                {
                    // Drive is SASI, so increment SASI count
                    sasi_num++;
                }
                else
                {
                    // Drive is SCSI, so increment SCSI count
                    scsi_num++;
                }
            }

            // Remove the unit
            if (ctrl[i])
            {
                ctrl[i]->SetUnit(j, NULL);
            }
        }

        // If there are no units connected
        if (sasi_num == 0 && scsi_num == 0)
        {
            if (ctrl[i])
            {
                delete ctrl[i];
                ctrl[i] = NULL;
                continue;
            }
        }

        // Mixture of SCSI and SASI
        if (sasi_num > 0 && scsi_num > 0)
        {
            FPRT(fp, "Error : SASI and SCSI can't be mixed\n");
            continue;
        }

        if (sasi_num > 0)
        {
            // Only SASI Unit(s)

            // Release the controller if it is not SASI
            if (ctrl[i] && !ctrl[i]->IsSASI())
            {
                delete ctrl[i];
                ctrl[i] = NULL;
            }

            // Create a new SASI controller
            if (!ctrl[i])
            {
                ctrl[i] = new SASIDEV();
                ctrl[i]->Connect(i, bus);
            }
        }
        else
        {
            // Only SCSI Unit(s)

            // Release the controller if it is not SCSI
            if (ctrl[i] && !ctrl[i]->IsSCSI())
            {
                delete ctrl[i];
                ctrl[i] = NULL;
            }

            // Create a new SCSI controller
            if (!ctrl[i])
            {
                ctrl[i] = new SCSIDEV();
                ctrl[i]->Connect(i, bus);
            }
        }

        // connect all units
        for (j = 0; j < UnitNum; j++)
        {
            unitno = i * UnitNum + j;
            if (disk[unitno])
            {
                // Add the unit connection
                ctrl[i]->SetUnit(j, disk[unitno]);
            }
        }
    }
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------
BOOL ProcessCmd(FILE *fp, int id, int un, int cmd, int type, char *file)
{
    Disk *map[CtrlMax * UnitNum];
    int len;
    char *ext;
    Filepath filepath;
    Disk *pUnit;

    // Copy the Unit List
    memcpy(map, disk, sizeof(disk));

    // Check the Controller Number
    if (id < 0 || id >= CtrlMax)
    {
        FPRT(fp, "%s Error : Invalid ID %d\n", __PRETTY_FUNCTION__, id);
        return FALSE;
    }

    // Check the Unit Number
    if (un < 0 || un >= UnitNum)
    {
        FPRT(fp, "Error : Invalid unit number\n");
        return FALSE;
    }

    // Connect Command
    if (cmd == 0)
    { // ATTACH
        // Distinguish between SASI and SCSI
        ext = NULL;
        pUnit = NULL;
        if (type == 0)
        {
            // Passed the check
            if (!file)
            {
                return FALSE;
            }

            // Check that command is at least 5 characters long
            len = strlen(file);
            if (len < 5)
            {
                return FALSE;
            }

            // Check the extension
            if (file[len - 4] != '.')
            {
                return FALSE;
            }

            // If the extension is not SASI type, replace with SCSI
            ext = &file[len - 3];
            if (xstrcasecmp(ext, "hdf") != 0)
            {
                type = 1;
            }
        }

        // Create a new drive, based upon type
        switch (type)
        {
        case 0: // HDF
            pUnit = new SASIHD();
            break;
        case 1: // HDS/HDN/HDI/NHD/HDA
            if (ext == NULL)
            {
                break;
            }
            if (xstrcasecmp(ext, "hdn") == 0 ||
                xstrcasecmp(ext, "hdi") == 0 || xstrcasecmp(ext, "nhd") == 0)
            {
                pUnit = new SCSIHD_NEC();
            }
            else if (xstrcasecmp(ext, "hda") == 0)
            {
                pUnit = new SCSIHD_APPLE();
            }
            else
            {
                pUnit = new SCSIHD();
            }
            break;
        case 2: // MO
            pUnit = new SCSIMO();
            break;
        case 3: // CD
            pUnit = new SCSICD();
            break;
        case 4: // BRIDGE
            pUnit = new SCSINuvolink();
            break;
        default:
            FPRT(fp, "Error : Invalid device type\n");
            return FALSE;
        }

        // drive checks files
        if (type <= 1 || (type <= 3 && xstrcasecmp(file, "-") != 0))
        {
            // Set the Path
            filepath.SetPath(file);

            // Open the file path
            if (!pUnit->Open(filepath))
            {
                FPRT(fp, "Error : File open error [%s]\n", file);
                delete pUnit;
                return FALSE;
            }
        }

        // Set the cache to write-through
        pUnit->SetCacheWB(FALSE);

        // Replace with the newly created unit
        map[id * UnitNum + un] = pUnit;

        // Re-map the controller
        MapControler(fp, map);
        return TRUE;
    }

    // Is this a valid command?
    if (cmd > 4)
    {
        FPRT(fp, "Error : Invalid command\n");
        return FALSE;
    }

    // Does the controller exist?
    if (ctrl[id] == NULL)
    {
        FPRT(fp, "Error : No such device\n");
        return FALSE;
    }

    // Does the unit exist?
    pUnit = disk[id * UnitNum + un];
    if (pUnit == NULL)
    {
        FPRT(fp, "Error : No such device\n");
        return FALSE;
    }

    // Disconnect Command
    if (cmd == 1)
    { // DETACH
        // Free the existing unit
        map[id * UnitNum + un] = NULL;

        // Re-map the controller
        MapControler(fp, map);
        return TRUE;
    }

    // Valid only for MO or CD
    if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O') &&
        pUnit->GetID() != MAKEID('S', 'C', 'C', 'D'))
    {
        FPRT(fp, "Error : Operation denied(Deveice isn't removable)\n");
        return FALSE;
    }

    switch (cmd)
    {
    case 2: // INSERT
        // Set the file path
        filepath.SetPath(file);

        // Open the file
        if (!pUnit->Open(filepath))
        {
            FPRT(fp, "Error : File open error [%s]\n", file);
            return FALSE;
        }
        break;

    case 3: // EJECT
        pUnit->Eject(TRUE);
        break;

    case 4: // PROTECT
        if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O'))
        {
            FPRT(fp, "Error : Operation denied(Deveice isn't MO)\n");
            return FALSE;
        }
        pUnit->WriteP(!pUnit->IsWriteP());
        break;
    default:
        ASSERT(FALSE);
        return FALSE;
    }

    return TRUE;
}

bool has_suffix(const char *string, const char *suffix)
{
    int string_len = strlen(string);
    int suffix_len = strlen(suffix);
    return (string_len >= suffix_len) && (xstrcasecmp(string + (string_len - suffix_len), suffix) == 0);
}

//---------------------------------------------------------------------------
//
//	Argument Parsing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
BOOL ParseConfig(int argc, char *argv[])
{
    FRESULT fr;
    FIL fp;
    char line[512];
    int id;
    int un;
    int type;
    char *argID;
    char *argPath;
    int len;
    char *ext;

    // Mount the SD card
    fr = f_mount(&fatfs, "", 1);
    if (fr != FR_OK)
    {
        FPRT(stderr, "Error : SD card mount failed.\n");
        return FALSE;
    }

    // If there is no setting file, the processing is interrupted
    fr = f_open(&fp, "rascsi.ini", FA_READ);
    if (fr != FR_OK)
    {
        return FALSE;
    }

    // Start Decoding

    while (TRUE)
    {
        // Get one Line
        memset(line, 0x00, sizeof(line));
        if (f_gets(line, sizeof(line) - 1, &fp) == NULL)
        {
            break;
        }

        // Delete the CR/LF
        len = strlen(line);
        while (len > 0)
        {
            if (line[len - 1] != '\r' && line[len - 1] != '\n')
            {
                break;
            }
            line[len - 1] = '\0';
            len--;
        }

        // Get the ID and Path
        argID = &line[0];
        argPath = &line[4];
        line[3] = '\0';

        // Check if the line is an empty string
        if (argID[0] == '\0' || argPath[0] == '\0')
        {
            continue;
        }

        if (strlen(argID) == 3 && xstrncasecmp(argID, "id", 2) == 0)
        {
            // ID or ID Format

            // Check that the ID number is valid (0-7)
            if (argID[2] < '0' || argID[2] > '7')
            {
                FPRT(stderr,
                     "Error : Invalid argument(IDn n=0-7) [%c]\n", argID[2]);
                goto parse_error;
            }

            // The ID unit is good
            id = argID[2] - '0';
            un = 0;
        }
        else if (xstrncasecmp(argID, "hd", 2) == 0)
        {
            // HD or HD format

            if (strlen(argID) == 3)
            {
                // Check that the HD number is valid (0-9)
                if (argID[2] < '0' || argID[2] > '9')
                {
                    FPRT(stderr,
                         "Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
                    goto parse_error;
                }

                // ID was confirmed
                id = (argID[2] - '0') / UnitNum;
                un = (argID[2] - '0') % UnitNum;
            }
            else if (strlen(argID) == 4)
            {
                // Check that the HD number is valid (10-15)
                if (argID[2] != '1' || argID[3] < '0' || argID[3] > '5')
                {
                    FPRT(stderr,
                         "Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
                    goto parse_error;
                }

                // The ID unit is good - create the id and unit number
                id = ((argID[3] - '0') + 10) / UnitNum;
                un = ((argID[3] - '0') + 10) % UnitNum;
                argPath++;
            }
            else
            {
                FPRT(stderr,
                     "Error : Invalid argument(IDn or HDn) [%s]\n", argID);
                goto parse_error;
            }
        }
        else
        {
            FPRT(stderr,
                 "Error : Invalid argument(IDn or HDn) [%s]\n", argID);
            goto parse_error;
        }

        // Skip if there is already an active device
        if (disk[id * UnitNum + un] &&
            !disk[id * UnitNum + un]->IsNULL())
        {
            continue;
        }

        // Initialize device type
        type = -1;

        // Check ethernet and host bridge
        if (xstrcasecmp(argPath, "bridge") == 0)
        {
            type = 4;
        }
        else
        {
            // Check the path length
            len = strlen(argPath);
            if (len < 5)
            {
                FPRT(stderr,
                     "Error : Invalid argument(File path is short) [%s]\n",
                     argPath);
                goto parse_error;
            }

            // Does the file have an extension?
            if (argPath[len - 4] != '.')
            {
                FPRT(stderr,
                     "Error : Invalid argument(No extension) [%s]\n", argPath);
                goto parse_error;
            }

            // Figure out what the type is
            ext = &argPath[len - 3];
            if (xstrcasecmp(ext, "hdf") == 0 ||
                xstrcasecmp(ext, "hds") == 0 ||
                xstrcasecmp(ext, "hdn") == 0 ||
                xstrcasecmp(ext, "hdi") == 0 || xstrcasecmp(ext, "nhd") == 0 ||
                xstrcasecmp(ext, "hda") == 0)
            {
                // HD(SASI/SCSI)
                type = 0;
            }
            else if (strcasecmp(ext, "mos") == 0)
            {
                // MO
                type = 2;
            }
            else if (strcasecmp(ext, "iso") == 0)
            {
                // CD
                type = 3;
            }
            else
            {
                // Cannot determine the file type
                FPRT(stderr,
                     "Error : Invalid argument(file type) [%s]\n", ext);
                goto parse_error;
            }
        }

        // Execute the command
        if (!ProcessCmd(stderr, id, un, 0, type, argPath))
        {
            goto parse_error;
        }
    }

    // Close the configuration file
    f_close(&fp);

    // Display the device list
    ListDevice(stdout);

    return TRUE;

parse_error:

    // Close the configuration file
    f_close(&fp);

    return FALSE;
}
#else
bool ParseArgument(int argc, char *argv[])
{
    int id = -1;
    bool is_sasi = false;
    int max_id = 7;

    int opt;
    while ((opt = getopt(argc, argv, "-IiHhD:d:")) != -1)
    {
        switch (opt)
        {
        case 'I':
        case 'i':
            is_sasi = false;
            max_id = 7;
            id = -1;
            continue;

        case 'H':
        case 'h':
            is_sasi = true;
            max_id = 15;
            id = -1;
            continue;

        case 'D':
        case 'd':
        {
            char *end;
            id = strtol(optarg, &end, 10);
            if (*end || (id < 0) || (max_id < id))
            {
                fprintf(stderr, "%s: invalid %s (0-%d)\n",
                        optarg, is_sasi ? "HD" : "ID", max_id);
                return false;
            }
            continue;
        }

        default:
            return false;

        case 1:
            break;
        }

        if (id < 0)
        {
            fprintf(stderr, "%s: ID not specified\n", optarg);
            return false;
        }
        else if (disk[id] && !disk[id]->IsNULL())
        {
            fprintf(stderr, "%d: duplicate ID\n", id);
            return false;
        }

        char *path = optarg;
        int type = -1;
        if (has_suffix(path, ".hdf") || has_suffix(path, ".hds") || has_suffix(path, ".hdn") || has_suffix(path, ".hdi") || has_suffix(path, ".hda") || has_suffix(path, ".nhd"))
        {
            type = 0;
        }
        else if (has_suffix(path, ".mos"))
        {
            type = 2;
        }
        else if (has_suffix(path, ".iso"))
        {
            type = 3;
        }
        else if (xstrcasecmp(path, "bridge") == 0)
        {
            type = 4;
        }
        else
        {
            // Cannot determine the file type
            fprintf(stderr,
                    "%s: unknown file extension\n", path);
            return false;
        }

        int un = 0;
        if (is_sasi)
        {
            un = id % UnitNum;
            id /= UnitNum;
        }

        // Execute the command
        if (!ProcessCmd(stderr, id, un, 0, type, path))
        {
            return false;
        }
        id = -1;
    }

    // Display the device list
    ListDevice(stdout);
    return true;
}
#endif // BAREMETAL

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
    LOGCRITICAL("asdf");
    cpu_set_t cpuset;
    int cpus;

    // Get the number of CPUs
    CPU_ZERO(&cpuset);
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
    cpus = CPU_COUNT(&cpuset);

    // Set the thread affinity
    if (cpu < cpus)
    {
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    }
}

//---------------------------------------------------------------------------
//
//	Monitor Thread
//
//---------------------------------------------------------------------------
static void *MonThread(void *param)
{
    struct sched_param schedparam;
    struct sockaddr_in client;
    socklen_t len;
    int fd;
    FILE *fp;
    char buf[BUFSIZ];
    char *p;
    int i;
    char *argv[5];
    int id;
    int un;
    int cmd;
    int type;
    char *file;

    // Scheduler Settings
    schedparam.sched_priority = 0;
    sched_setscheduler(0, SCHED_IDLE, &schedparam);

    // Set the affinity to a specific processor core
    FixCpu(2);

    // Wait for the execution to start
    while (!running)
    {
        usleep(1);
    }

    // Setup the monitor socket to receive commands
    listen(monsocket, 1);

    while (1)
    {
        // Wait for connection
        memset(&client, 0, sizeof(client));
        len = sizeof(client);
        fd = accept(monsocket, (struct sockaddr *)&client, &len);
        if (fd < 0)
        {
            break;
        }

        // Fetch the command
        fp = fdopen(fd, "r+");
        p = fgets(buf, BUFSIZ, fp);

        // Failed to get the command
        if (!p)
        {
            goto next;
        }

        // Remove the newline character
        p[strlen(p) - 1] = 0;

        // List all of the devices
        if (xstrncasecmp(p, "list", 4) == 0)
        {
            ListDevice(fp);
            goto next;
        }

        // Parameter separation
        argv[0] = p;
        for (i = 1; i < 5; i++)
        {
            // Skip parameter values
            while (*p && (*p != ' '))
            {
                p++;
            }

            // Replace spaces with null characters
            while (*p && (*p == ' '))
            {
                *p++ = 0;
            }

            // The parameters were lost
            if (!*p)
            {
                break;
            }

            // Recognized as a parameter
            argv[i] = p;
        }

        // Failed to get all parameters
        if (i < 5)
        {
            goto next;
        }

        // ID, unit, command, type, file
        id = atoi(argv[0]);
        un = atoi(argv[1]);
        cmd = atoi(argv[2]);
        type = atoi(argv[3]);
        file = argv[4];

        // Wait until we becom idle
        while (active)
        {
            usleep(500 * 1000);
        }

        // Execute the command
        ProcessCmd(fp, id, un, cmd, type, file);

    next:
        // Release the connection
        fclose(fp);
        close(fd);
    }

    return NULL;
}
#endif // BAREMETAL

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
extern "C" int startrascsi(void)
{
    int argc = 0;
    char **argv = NULL;
#else
int main(int argc, char *argv[])
{
#endif // BAREMETAL
    int i;
    int ret;
    int actid;
    DWORD now;
    BUS::phase_t phase;
    BYTE data;
#ifndef BAREMETAL
    struct sched_param schparam;
#endif // BAREMETAL

    spdlog::set_level(spdlog::level::trace);
    LOGTRACE("LOGTRACE")
    LOGDEBUG("LOGDEBUG");
    LOGINFO("LOGINFO");
    LOGWARN("LOGWARN");
    LOGERROR("LOGERROR");
    LOGCRITICAL("LOGCRITICAL");

    spdlog::set_level(spdlog::level::trace);
    LOGTRACE("Entering the function %s with %d arguments", __PRETTY_FUNCTION__, argc);
    // Output the Banner
    Banner(argc, argv);

    // Initialize
    ret = 0;
    if (!Init())
    {
        ret = EPERM;
        goto init_exit;
    }

    // Reset
    Reset();

#ifdef BAREMETAL
    // BUSY assert (to hold the host side)
    bus->SetBSY(TRUE);

    // Argument parsing
    if (!ParseConfig(argc, argv))
    {
        ret = EINVAL;
        goto err_exit;
    }
#else
    // Argument parsing
    if (!ParseArgument(argc, argv))
    {
        ret = EINVAL;
        goto err_exit;
    }
#endif

#ifdef BAREMETAL
    // Release the busy signal
    bus->SetBSY(FALSE);
#endif

#ifndef BAREMETAL
    // Set the affinity to a specific processor core
    FixCpu(3);

#ifdef USE_SEL_EVENT_ENABLE
    // Scheduling policy setting (highest priority)
    schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif // USE_SEL_EVENT_ENABLE
#endif // BAREMETAL

    // Start execution
    running = TRUE;

    // Main Loop
    while (running)
    {
        // Work initialization
        actid = -1;
        phase = BUS::busfree;

        // If any of the controllers are an Ethernet
        // device, check to see if there is new data
        // received.
        for (int x = 0; x < CtrlMax; x++)
        {
            if (!ctrl[x])
            {
                continue;
            }

            for (int y = 0; y < UnitNum; y++)
            {
                int unitno = x * UnitNum + y;
                // branch by unit type
                if (disk[unitno])
                {
                    if (disk[unitno]->GetID() == MAKEID('S', 'C', 'N', 'L'))
                    {
                        // Receive the data....
                        int packet_len = ((SCSINuvolink *)disk[unitno])->ReceivePacket();
                        if (packet_len > 0)
                        {
                            ((SCSIDEV *)ctrl[x])->TransferPacketToHost(packet_len);
                        }
                    }
                }
            }
        }

#ifdef USE_SEL_EVENT_ENABLE
        // SEL signal polling
        if (bus->PollSelectEvent() < 0)
        {
            // Stop on interrupt
            if (errno == EINTR)
            {
                break;
            }
            continue;
        }

        // Get the bus
        bus->Aquire();
#else
        bus->Aquire();
        if (!bus->GetSEL())
        {
#if !defined(BAREMETAL)
            usleep(0);
#endif // !BAREMETAL
            continue;
        }
#endif // USE_SEL_EVENT_ENABLE

        // Wait until BSY is released as there is a possibility for the
        // initiator to assert it while setting the ID (for up to 3 seconds)
        if (bus->GetBSY())
        {
            now = SysTimer::GetTimerLow();
            while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000)
            {
                bus->Aquire();
                if (!bus->GetBSY())
                {
                    break;
                }
            }
        }

        // Stop because it the bus is busy or another device responded
        if (bus->GetBSY() || !bus->GetSEL())
        {
            continue;
        }

        // Notify all controllers
        data = bus->GetDAT();
        for (i = 0; i < CtrlMax; i++)
        {
            if (!ctrl[i] || (data & (1 << i)) == 0)
            {
                continue;
            }

            // Find the target that has moved to the selection phase
            if (ctrl[i]->Process() == BUS::selection)
            {
                // Get the target ID
                actid = i;

                // Bus Selection phase
                phase = BUS::selection;
                break;
            }
        }

        // Return to bus monitoring if the selection phase has not started
        if (phase != BUS::selection)
        {
            continue;
        }

        // Start target device
        active = TRUE;

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
        // Scheduling policy setting (highest priority)
        schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
        sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif // !USE_SEL_EVENT_ENABLE && !BAREMETAL

        // Loop until the bus is free
        while (running)
        {
            // Target drive
            phase = ctrl[actid]->Process();

            // End when the bus is free
            if (phase == BUS::busfree)
            {
                break;
            }
        }

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
        // Set the scheduling priority back to normal
        schparam.sched_priority = 0;
        sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif // !USE_SEL_EVENT_ENABLE && !BAREMETAL

        // End the target travel
        active = FALSE;
    }

err_exit:
    // Cleanup
    Cleanup();

init_exit:
#if !defined(BAREMETAL)
    exit(ret);
#else
    return ret;
#endif // BAREMETAL
}

//---------------------------------------------------------------------------
// MVNC API C
//---------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#include "libusb.h"
#include "mvnc.h"
#include "usb_link.h"
#include "usb_boot.h"
#include "common.h"

//---------------------------------------------------------------------------
// Graph file structure
//---------------------------------------------------------------------------
#define HEADER_LENGTH       264
#define STAGE_LENGTH        227
#define VERSION_OFFSET      36
#define GRAPH_VERSION       2
#define N_STAGES_OFFSET     240
#define FIRST_SHAVE_OFFSET  248
#define N_OUTPUTS_OFFSET    (HEADER_LENGTH + 136)
#define X_OUT_STRIDE_OFFSET (HEADER_LENGTH + 172)

#define THERMAL_BUFFER_SIZE 100
#define DEBUG_BUFFER_SIZE       120

#define MAX_OPTIMISATIONS               40
#define OPTIMISATION_NAME_LEN   50
#define OPTIMISATION_LIST_BUFFER_SIZE (MAX_OPTIMISATIONS * OPTIMISATION_NAME_LEN)

#define MAX_PATH_LENGTH                 255
#define STATUS_WAIT_TIMEOUT     15

static int initialized = 0;

int mvnc_loglevel = 0;

//---------------------------------------------------------------------------
// Structs
//---------------------------------------------------------------------------
struct Graph;

struct Device {
        int     backoff_time_normal, backoff_time_high, backoff_time_critical;
        int     temperature_debug, throttle_happened;
        float   temp_lim_upper, temp_lim_lower;
        float   *thermal_stats;
        char    *dev_addr;              // Device USB address as returned by usb_
        char    *dev_file;              // Device filename in /dev directory
        char    *optimisation_list;
        void    *usb_link;
        struct  Device *next;   // Next device in chain
        struct  Graph *graphs;  // List of associated graphs
} *devices;

static void *device_usb_link;

struct Graph {
        int     started;
        int     have_data;
        int     dont_block;
        int     input_idx;
        int     output_idx;
        int     failed;
        int     iterations;
        int     network_throttle;
        unsigned noutputs;
        unsigned nstages;
        struct Device *dev;
        struct Graph *next;
        char    *aux_buffer;
        char    *debug_buffer;
        float   *time_taken;
        void    *user_param[2];
        void    *output_data;
};

//---------------------------------------------------------------------------
// Function Prototypes
//---------------------------------------------------------------------------
static double time_in_seconds(void);
static void initialize(void);
void usblink_resetall(void);

//---------------------------------------------------------------------------
// return time in seconds
//---------------------------------------------------------------------------
static double time_in_seconds(void)
{
        static double s;
        struct timespec ts;

//      clock_gettime(&ts);
        clock_gettime(CLOCK_MONOTONIC, &ts);
                
        if(!s) s = ts.tv_sec + ts.tv_nsec * 1e-9;
        return ts.tv_sec + ts.tv_nsec * 1e-9 - s;
}

//---------------------------------------------------------------------------
// We sanitize the situation by trying to reset the devices that have been left open
//---------------------------------------------------------------------------
static void initialize(void)
{
        initialized = 1;
        usblink_resetall();
}

//---------------------------------------------------------------------------
mvncStatus mvncGetDeviceName(int index, char *name, unsigned int nameSize)
{
    int rc;

        if(index < 0 || !name || nameSize < MVNC_MAX_NAME_SIZE) {
                return MVNC_INVALID_PARAMETERS;
    }
        if(!initialized) initialize();
        rc = usb_find_device(index, name, nameSize, 0, 0, 0);
        return rc;
}

//---------------------------------------------------------------------------
static int is_device_opened(const char *name)
{
        struct Device *d = devices;
        while (d) {
                if (strcmp(d->dev_addr, name) == 0)
                        return 0;
                d = d->next;
        }
        return -1;
}

//---------------------------------------------------------------------------
static mvncStatus load_fw_file(const char *name)
{
        int rc;
        FILE *fp;
        char *tx_buf;
        unsigned file_size;
        char mv_cmd_file[MAX_PATH_LENGTH], *p;

    strcpy(mv_cmd_file, "MvNCAPI.mvcmd");       // dont bother with Searching around, just hardcode the file name
        fp = fopen(mv_cmd_file, "rb");          // Load the mvnc executable
        if(fp == NULL) {
                if(mvnc_loglevel) perror(mv_cmd_file);
                return MVNC_MVCMD_NOT_FOUND;
        }
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        rewind(fp);
        if(!(tx_buf = malloc(file_size))) {
                if(mvnc_loglevel) perror("buffer");
                fclose(fp);
                return MVNC_OUT_OF_MEMORY;
        }
        if(fread(tx_buf, 1, file_size, fp) != file_size) {
                if(mvnc_loglevel) perror(mv_cmd_file);
                fclose(fp);
                free(tx_buf);
                return MVNC_MVCMD_NOT_FOUND;
        }
        fclose(fp);
        rc = usb_boot(name, tx_buf, file_size);         // Boot it
        free(tx_buf);
        if(rc) {
                return rc;
        }
        PRINT_DEBUG("Boot successful, device address %s\n", name);
        return MVNC_OK;
}

//---------------------------------------------------------------------------
static void allocate_device(const char* name, void **deviceHandle, void* f)
{
        struct Device *d = calloc(1, sizeof(*d));

        d->dev_addr             = strdup(name);
        d->usb_link             = f;
        d->next                 = devices;
        d->temp_lim_upper       = 95;
        d->temp_lim_lower       = 85;
        d->backoff_time_normal   = 0;
        d->backoff_time_high     = 100;
        d->backoff_time_critical = 10000;
        d->temperature_debug     = 0;
        devices = d;
        *deviceHandle = d;
        PRINT_DEBUG("done\n");
        PRINT_INFO("Booted %s -> %s\n", d->dev_addr, d->dev_file ? d->dev_file : "VSC");
}
//---------------------------------------------------------------------------
#define OPENWITHVIDPID 1
//---------------------------------------------------------------------------
#if OPENWITHVIDPID
mvncStatus mvncOpenDevice(const char *name, void **deviceHandle)
{
        libusb_device_handle *handle;
        libusb_device *dev;

        int rc, rd;
    void *f;
        char *device_name = (char *)name;

        if(!name || !deviceHandle) return MVNC_INVALID_PARAMETERS;

        if(device_name == NULL) {
                return MVNC_INVALID_PARAMETERS;
        }
        if(!initialized) {
                initialize();
    }
        rc = load_fw_file(device_name);
        if(rc != MVNC_OK) {
                return rc;
        }
        Sleep(2000);
        // Now we should have a new device, try to open it
    handle = libusb_open_device_with_vid_pid(NULL, DEFAULT_OPEN_VID, DEFAULT_OPEN_PID);
        if(handle == NULL) {
        PRINT_INFO("ERROR: Could not open device =  %x:%x\n", DEFAULT_OPEN_VID, DEFAULT_OPEN_PID);
    }
    else {
        PRINT_INFO("device =  %x:%x opened\n", DEFAULT_OPEN_VID, DEFAULT_OPEN_PID);
        allocate_device(device_name, deviceHandle, handle);
        device_usb_link = handle;
                return MVNC_OK;
    }
        return MVNC_ERROR;
}
#else
mvncStatus mvncOpenDevice(const char *name, void **deviceHandle)
{
        int rc;
    double waittm;
        char* device_name = (char* )name;

        if(!name || !deviceHandle) return MVNC_INVALID_PARAMETERS;

        if(!initialized) {
                initialize();
    }
        rc = load_fw_file(device_name);
        if(rc != MVNC_OK) {
                return rc;
        }
    Sleep(2000);
        waittm = time_in_seconds() + STATUS_WAIT_TIMEOUT;
        while(time_in_seconds() < waittm) {
                void *f = usblink_open(device_name);   // Now we should have a new device, try to open it
                if(f) {
                        myriadStatus_t status;
                        if(!usblink_getmyriadstatus(f, &status) && status == MYRIAD_WAITING) {
                PRINT_INFO("device status =  %d\n", status);
                        allocate_device(device_name, deviceHandle, f);
                device_usb_link = f;
                                return MVNC_OK;
                        }
            else {
                                PRINT_DEBUG("found, but cannot get status\n");
                                usblink_close(f);
                        }
                }
                usleep(10000);                  // Error opening it, try again
        }
        return MVNC_ERROR;
}
#endif
//---------------------------------------------------------------------------
mvncStatus getDeviceStatus(void)
{
        myriadStatus_t status;
    int retCode;

        retCode = usblink_getmyriadstatus(device_usb_link, &status);
        PRINT_DEBUG("return code = %d  status = %d\n", retCode, status);
    return status;
}
//---------------------------------------------------------------------------
static int find_device(void *deviceHandle)
{
        struct Device *d = devices;
        while(d) {
                if(d == deviceHandle) return 0;
                d = d->next;
        }
        return -1;
}

//---------------------------------------------------------------------------
static int find_graph(void *graphHandle)
{
        struct Device *d = devices;
        while(d) {
                struct Graph *g = d->graphs;
                while (g) {
                        if (g == graphHandle)
                                return 0;
                        g = g->next;
                }
                d = d->next;
        }
        return -1;
}

//---------------------------------------------------------------------------
// Defined here as it will be used twice
//---------------------------------------------------------------------------
static int deallocate_graph(struct Graph *g)
{
        int found = 0;

        // Remove it from the list of the associated device
        if (g->dev->graphs == g) {
                g->dev->graphs = g->next;
                found = 1;
        }
    else {
                struct Graph *gp = g->dev->graphs;
                while (gp->next) {
                        if (gp->next == g) {
                                found = 1;
                                gp->next = gp->next->next;
                                break;
                        }
                        gp = gp->next;
                }
        }

        // Free it with all its data
        if(found) {
                free(g->aux_buffer);
                free(g->output_data);
                g->dev->thermal_stats = 0;
                free(g);
        }
        return -!found;
}

//---------------------------------------------------------------------------
mvncStatus mvncCloseDevice(void *deviceHandle)
{
    struct Device *d;
        int found = 0;

        if(!deviceHandle)
                return MVNC_INVALID_PARAMETERS;

        if(find_device(deviceHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }

        d = (struct Device *) deviceHandle;

        // Remove it from our list
        if(devices == d) {
                devices = d->next;
                found = 1;
        }
    else {
                struct Device *dp = devices;
                while (dp->next) {
                        if (dp->next == d) {
                                found = 1;
                                dp->next = dp->next->next;
                                break;
                        }
                        dp = dp->next;
                }
        }
        if(!found) {
                return MVNC_INVALID_PARAMETERS;
        }
        // Deallocate all associated graphs
        while(d->graphs) deallocate_graph(d->graphs);

        // Reset
        usblink_resetmyriad(d->usb_link);
        usblink_close(d->usb_link);
        if(d->optimisation_list) free(d->optimisation_list);

        free(d->dev_addr);
        free(d->dev_file);
        free(d);

        usleep(500000);
        return MVNC_OK;
}

//---------------------------------------------------------------------------
static unsigned read_32bits(const unsigned char *ptr)
{
        return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}
//---------------------------------------------------------------------------
mvncStatus mvncAllocateGraph(void *deviceHandle, void **graphHandle, const void *graphFile, unsigned int graphFileLength)
{
    struct Device *d;
    unsigned char *graph;
        unsigned nstages;
    unsigned noutputs;
    struct Graph *g;
        myriadStatus_t status;
    double timeout;

        if(!deviceHandle || !graphHandle || !graphFile) return MVNC_INVALID_PARAMETERS;

        if(graphFileLength < HEADER_LENGTH + STAGE_LENGTH || graphFileLength > 512 * 1024 * 1024) {
                return MVNC_UNSUPPORTED_GRAPH_FILE;
    }

        graph = (unsigned char *) graphFile;
        if(graph[VERSION_OFFSET] != GRAPH_VERSION) return MVNC_UNSUPPORTED_GRAPH_FILE;

        nstages = graph[N_STAGES_OFFSET] + (graph[N_STAGES_OFFSET + 1] << 8);
        noutputs = read_32bits(graph + N_OUTPUTS_OFFSET + (nstages - 1) * STAGE_LENGTH) *
                                                read_32bits(graph + N_OUTPUTS_OFFSET + (nstages - 1) * STAGE_LENGTH + 4) *
                                                read_32bits(graph + X_OUT_STRIDE_OFFSET + (nstages - 1) * STAGE_LENGTH) / 2;

        // A reasonable check on graph correctness
        if(noutputs > 64 * 1024 * 1024) return MVNC_UNSUPPORTED_GRAPH_FILE;

        d = devices;
        while(d) {
                if(d == deviceHandle) break;
                d = d->next;
        }
        if(!d) {
                return MVNC_INVALID_PARAMETERS;
        }
        PRINT_DEBUG("Device %s -> %s\n", d->dev_addr, d->dev_file ? d->dev_file : "VSC");
        if(d->graphs) {
                return MVNC_BUSY;
        }
        timeout = time_in_seconds() + 10;
        do {
                if(usblink_getmyriadstatus(d->usb_link, &status)) {
                        PRINT_DEBUG("Myriad status error (1) %d\n", status);
                        return MVNC_ERROR;
                }
                usleep(10000);
        } while(status != MYRIAD_WAITING && time_in_seconds() < timeout);

        if(status != MYRIAD_WAITING) {
        PRINT_DEBUG("Myriad status error (2) %d\n", status);
                return MVNC_ERROR;
        }

        if(usblink_setdata(d->usb_link, "blobFile", graphFile, graphFileLength, 0)) {
        PRINT_DEBUG("Set Status error (3) %d\n", status);
                return MVNC_ERROR;
        }

        g = calloc(1, sizeof(*g));
        g->dev = d;
        g->nstages = nstages;
        g->noutputs = noutputs;

        // aux_buffer
        g->aux_buffer = calloc(1, 224 + nstages * sizeof(*g->time_taken));
        if(!g->aux_buffer) {
                free(g);
                return MVNC_OUT_OF_MEMORY;
        }

        if(usblink_setdata(g->dev->usb_link, "auxBuffer", g->aux_buffer, 224 + nstages * sizeof(*g->time_taken), 0)) {
                free(g->aux_buffer);
                free(g);
                return MVNC_ERROR;
        }

        g->debug_buffer = g->aux_buffer;
        g->time_taken = (float *) (g->aux_buffer + 224);

        // output_data
        g->output_data = calloc(noutputs, 2);
        if (!g->output_data) {
                free(g->aux_buffer);
                free(g);
                return MVNC_OUT_OF_MEMORY;
        }

        g->dev->thermal_stats = (float *) (g->aux_buffer + DEBUG_BUFFER_SIZE);

        g->iterations = 1;
        g->network_throttle = 1;
        if(d->graphs) g->next = d->graphs;
        d->graphs = g;
        *graphHandle = g;
        return MVNC_OK;
}
//---------------------------------------------------------------------------
mvncStatus mvncDeallocateGraph(void *graphHandle)
{
//    struct Device *d;

        if(!graphHandle) return MVNC_INVALID_PARAMETERS;

        if(find_graph(graphHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }
//      d = ((struct Graph *) graphHandle)->dev;
        if(deallocate_graph((struct Graph *) graphHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }
        return MVNC_OK;
}
//---------------------------------------------------------------------------
mvncStatus mvncSetGraphOption(void *graphHandle, int option, const void *data, unsigned int dataLength)
{
    struct Graph *g;

        if (!graphHandle || !data || dataLength != 4)
                return MVNC_INVALID_PARAMETERS;

        g = (struct Graph *) graphHandle;
        if(find_graph(graphHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }
        switch (option) {
        case MVNC_ITERATIONS:
                g->iterations = *(int *) data;
                break;
        case MVNC_NETWORK_THROTTLE:
                g->network_throttle = *(int *) data;
                break;
        case MVNC_DONT_BLOCK:
                g->dont_block = *(int *) data;
                break;
        default:
                return MVNC_INVALID_PARAMETERS;
        }
        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncGetGraphOption(void *graphHandle, int option, void *data, unsigned int *dataLength)
{
    struct Graph *g;

        if(!graphHandle || !data || !dataLength)
                return MVNC_INVALID_PARAMETERS;

        g = (struct Graph *) graphHandle;
        if(find_graph(graphHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }
        switch (option) {
        case MVNC_ITERATIONS:
                *(int *) data = g->iterations;
                *dataLength = sizeof(int);
                break;
        case MVNC_NETWORK_THROTTLE:
                *(int *) data = g->network_throttle;
                *dataLength = sizeof(int);
                break;
        case MVNC_DONT_BLOCK:
                *(int *) data = g->dont_block;

                *dataLength = sizeof(int);
                break;
        case MVNC_TIME_TAKEN:
                *(float **) data = g->time_taken;
                *dataLength = sizeof(*g->time_taken) * g->nstages;
                break;
        case MVNC_DEBUG_INFO:
                *(char **) data = g->debug_buffer;
                *dataLength = DEBUG_BUFFER_SIZE;
                break;
        default:
                return MVNC_INVALID_PARAMETERS;
        }
        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncSetGlobalOption(int option, const void *data, unsigned int dataLength)
{
        if(!data || dataLength != 4) return MVNC_INVALID_PARAMETERS;

        switch (option) {
        case MVNC_LOG_LEVEL:
                mvnc_loglevel = *(int *) data;
                break;
        default:
                return MVNC_INVALID_PARAMETERS;
        }

        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncGetGlobalOption(int option, void *data, unsigned int *dataLength)
{
        if(!data || !dataLength)
                return MVNC_INVALID_PARAMETERS;

        switch (option) {
        case MVNC_LOG_LEVEL:
                *(int *) data = mvnc_loglevel;
                *dataLength = sizeof(mvnc_loglevel);
                break;
        default:
                return MVNC_INVALID_PARAMETERS;
        }
        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncSetDeviceOption(void *deviceHandle, int option, const void *data, unsigned int dataLength)
{
    struct Device *d;

        if(deviceHandle == 0 && option == MVNC_LOG_LEVEL) {
                PRINT("Warning: MVNC_LOG_LEVEL is not a Device Option, please use mvncSetGlobalOption()!\n");
                return mvncSetGlobalOption(option, data, dataLength);
        }

        if(!deviceHandle || !data || dataLength != 4)
                return MVNC_INVALID_PARAMETERS;

        d = (struct Device *) deviceHandle;
        if(find_device(d)) {
                return MVNC_INVALID_PARAMETERS;
        }

        switch (option) {
        case MVNC_TEMP_LIM_LOWER:
                d->temp_lim_lower = *(float *) data;
                break;
        case MVNC_TEMP_LIM_HIGHER:
                d->temp_lim_upper = *(float *) data;
                break;
        case MVNC_BACKOFF_TIME_NORMAL:
                d->backoff_time_normal = *(int *) data;
                break;
        case MVNC_BACKOFF_TIME_HIGH:
                d->backoff_time_high = *(int *) data;
                break;
        case MVNC_BACKOFF_TIME_CRITICAL:
                d->backoff_time_critical = *(int *) data;
                break;
        case MVNC_TEMPERATURE_DEBUG:
                d->temperature_debug = *(int *) data;
                break;
        default:
                return MVNC_INVALID_PARAMETERS;
        }

        return MVNC_OK;
}

//---------------------------------------------------------------------------
static mvncStatus get_optimisation_list(struct Device *d)
{
        int i, config[10];
        double timeout;
        myriadStatus_t status;
        char *p;

        if (d->optimisation_list)
                return MVNC_OK;

        d->optimisation_list = calloc(OPTIMISATION_LIST_BUFFER_SIZE, 1);
        if (!d->optimisation_list)
                return MVNC_OUT_OF_MEMORY;

        memset(config, 0, sizeof(config));
        config[0] = 1;
        config[1] = 1;
        if(usblink_setdata(d->usb_link, "config", config, sizeof(config), 1))
                return MVNC_ERROR;

        timeout = time_in_seconds() + STATUS_WAIT_TIMEOUT;
        do {
                if(usblink_getmyriadstatus(d->usb_link, &status))  return MVNC_ERROR;
                usleep(10000);
        } while (status != MYRIAD_WAITING && status != MYRIAD_FINISHED && time_in_seconds() < timeout);

        if(status != MYRIAD_WAITING && status != MYRIAD_FINISHED)
                return MVNC_TIMEOUT;

        if(usblink_getdata(d->usb_link, "optimizationList", d->optimisation_list, OPTIMISATION_LIST_BUFFER_SIZE, 0, 0))
                return MVNC_ERROR;

        for(i = 0; i < MAX_OPTIMISATIONS; i++) {
                p = strchr(d->optimisation_list + i * OPTIMISATION_NAME_LEN, '~');
                if(p) *p = 0;
        }

        config[1] = 0;
        if(usblink_setdata(d->usb_link, "config", config, sizeof(config), 0))
                return MVNC_ERROR;
        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncGetDeviceOption(void *deviceHandle, int option, void *data, unsigned int *dataLength)
{
        mvncStatus rc;
    struct Device *d;

        if(deviceHandle == 0 && option == MVNC_LOG_LEVEL) {
                PRINT("Warning: MVNC_LOG_LEVEL is not a Device Option, please use mvncGetGlobalOption()!\n");
                return mvncGetGlobalOption(option, data, dataLength);
        }

        if(!deviceHandle || !data || !dataLength)
                return MVNC_INVALID_PARAMETERS;

        d = (struct Device *) deviceHandle;
        if(find_device(d)) {
                return MVNC_INVALID_PARAMETERS;
        }

        switch (option) {
        case MVNC_TEMP_LIM_LOWER:
                *(float *) data = d->temp_lim_lower;
                *dataLength = sizeof(int);
                break;
        case MVNC_TEMP_LIM_HIGHER:
                *(float *) data = d->temp_lim_upper;
                *dataLength = sizeof(int);
                break;
        case MVNC_BACKOFF_TIME_NORMAL:
                *(int *) data = d->backoff_time_normal;
                *dataLength = sizeof(int);
                break;
        case MVNC_BACKOFF_TIME_HIGH:
                *(int *) data = d->backoff_time_high;
                *dataLength = sizeof(int);
                break;
        case MVNC_BACKOFF_TIME_CRITICAL:
                *(int *) data = d->backoff_time_critical;
                *dataLength = sizeof(int);
                break;
        case MVNC_TEMPERATURE_DEBUG:
                *(int *) data = d->temperature_debug;
                *dataLength = sizeof(int);
                break;
        case MVNC_THERMAL_STATS:
                if(!d->thermal_stats) {
                        return MVNC_NO_DATA;
                }
                *(float **) data = d->thermal_stats;
                *dataLength = THERMAL_BUFFER_SIZE;
                break;
        case MVNC_OPTIMISATION_LIST:
                rc = get_optimisation_list(d);
                if(rc) {
                        return rc;
                }
                *(char **) data = d->optimisation_list;
                *dataLength = OPTIMISATION_LIST_BUFFER_SIZE;
                break;
        case MVNC_THERMAL_THROTTLING_LEVEL:
                *(int *) data = d->throttle_happened;
                *dataLength = sizeof(int);
                break;
        default:
                return MVNC_INVALID_PARAMETERS;
        }
        return MVNC_OK;
}

//---------------------------------------------------------------------------
static int send_opt_data(struct Graph *g)
{
        int config[10];

        config[0] = 1;          // Version
        config[1] = 0;          // Query disable
        config[2] = g->iterations;
        config[3] = g->dev->temp_lim_upper;
        config[4] = g->dev->temp_lim_lower;
        config[5] = g->dev->backoff_time_normal;
        config[6] = g->dev->backoff_time_high;
        config[7] = g->dev->backoff_time_critical;
        config[8] = g->dev->temperature_debug;
        config[9] = g->network_throttle;

        if(usblink_setdata(g->dev->usb_link, "config", config, sizeof(config), 0)) return MVNC_ERROR;

        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncLoadTensor(void *graphHandle, const void *inputTensor, unsigned int inputTensorLength, void *userParam)
{
    struct Graph *g;

        if (!graphHandle || !inputTensor || inputTensorLength < 2)
                return MVNC_INVALID_PARAMETERS;

        g = (struct Graph *) graphHandle;
        if(find_graph(graphHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }

        if(!g->started) {
                if(send_opt_data(g)) {
                        return MVNC_ERROR;
                }
                g->started = 1;
        }

        while(g->have_data == 2) {
                if(g->dont_block) {
                        return MVNC_BUSY;
                }
                if(g->failed) {
                        return MVNC_ERROR;
                }
                usleep(1000);
                if (find_graph(g)) {
                        return MVNC_GONE;
                }
        }
        if(usblink_setdata(g->dev->usb_link, g->input_idx ? "input2" : "input1", inputTensor, inputTensorLength, g->have_data == 0)) {
                return MVNC_ERROR;
        }

        g->user_param[g->input_idx] = userParam;
        g->input_idx = !g->input_idx;
        g->have_data++;
        return MVNC_OK;
}

//---------------------------------------------------------------------------
mvncStatus mvncGetResult(void *graphHandle, void **outputData, unsigned int *outputDataLength, void **userParam)
{
    struct Graph *g;
    double timeout;

        int rc, unlock_own = 0;

        if(!graphHandle || !outputData || !outputDataLength) return MVNC_INVALID_PARAMETERS;

        g = (struct Graph *) graphHandle;
        if(find_graph(graphHandle)) {
                return MVNC_INVALID_PARAMETERS;
        }

        while(!g->have_data) {
                if(g->dont_block) {
                        return MVNC_NO_DATA;
                }
                usleep(1000);
                if(find_graph(g)) {
                        return MVNC_GONE;
                }
        }

        timeout = time_in_seconds() + STATUS_WAIT_TIMEOUT;
        do {
                if(!usblink_getdata(g->dev->usb_link, "output", g->output_data, 2 * g->noutputs, 0, 0)) {
                        unsigned int length = DEBUG_BUFFER_SIZE + THERMAL_BUFFER_SIZE + sizeof(int) + sizeof(*g->time_taken) * g->nstages;

                        if(usblink_getdata(g->dev->usb_link, "auxBuffer", g->aux_buffer, length, 0, g->have_data == 2)) {
                                g->failed = 1;
                                return MVNC_ERROR;
                        }
                        unlock_own = 1;
                        break;
                }
                usleep(1000);
                if(find_graph(g)) {
                        return MVNC_GONE;
                }
        } while (time_in_seconds() < timeout);

        g->dev->throttle_happened = *(int *) (g->aux_buffer + DEBUG_BUFFER_SIZE + THERMAL_BUFFER_SIZE);
        *outputData = g->output_data;
        *outputDataLength = 2 * g->noutputs;
        *userParam = g->user_param[g->output_idx];
        g->output_idx = !g->output_idx;
        g->have_data--;

        if(unlock_own) {
                rc = *g->debug_buffer ? MVNC_MYRIAD_ERROR : MVNC_OK;
                if(rc) g->failed = 1;
        }
    else {
                rc = MVNC_TIMEOUT;
                g->failed = 1;
        }

        return rc;
}

//---------------------------------------------------------------------------


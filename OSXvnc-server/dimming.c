#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include <Carbon/Carbon.h>

#include "rfb.h"

Bool rfbNoDimming = FALSE;
Bool rfbNoSleep   = TRUE;
IOPMAssertionID userActivityLastAssertionId;

static pthread_mutex_t  dimming_mutex;
static unsigned long    dim_time;
static unsigned long    sleep_time;
static mach_port_t      master_dev_port;
static io_connect_t     power_mgt;
static Bool initialized            = FALSE;
static Bool dim_time_saved         = FALSE;
static Bool sleep_time_saved       = FALSE;

// OSXvnc 0.8 - Disable ScreenSaver
void rfbScreensaverTimer(EventLoopTimerRef timer, void *userData)
{
#pragma unused (timer, userData)
    if (rfbNoSleep && rfbClientsConnected()) {
        UpdateSystemActivity(IdleActivity);
        // UpdateSystemActivity's seeming replacement:
        IOPMAssertionDeclareUserActivity(CFSTR("VNC user is logged in"), kIOPMUserActiveLocal, &userActivityLastAssertionId);
    }
}

static int
saveDimSettings(void)
{
    if (IOPMGetAggressiveness(power_mgt,
                              kPMMinutesToDim,
                              &dim_time) != kIOReturnSuccess)
        return -1;

    dim_time_saved = TRUE;
    return 0;
}

static int
restoreDimSettings(void)
{
    if (!dim_time_saved)
        return -1;

    if (IOPMSetAggressiveness(power_mgt,
                              kPMMinutesToDim,
                              dim_time) != kIOReturnSuccess)
        return -1;

    dim_time_saved = FALSE;
    dim_time = 0;
    return 0;
}

static int
saveSleepSettings(void)
{
    if (IOPMGetAggressiveness(power_mgt,
                              kPMMinutesToSleep,
                              &sleep_time) != kIOReturnSuccess)
        return -1;

    sleep_time_saved = TRUE;
    return 0;
}

static int
restoreSleepSettings(void)
{
    if (!sleep_time_saved)
        return -1;

    if (IOPMSetAggressiveness(power_mgt,
                              kPMMinutesToSleep,
                              sleep_time) != kIOReturnSuccess)
        return -1;

    sleep_time_saved = FALSE;
    sleep_time = 0;
    return 0;
}


int
rfbDimmingInit(void)
{
    pthread_mutex_init(&dimming_mutex, NULL);

    if (IOMasterPort(bootstrap_port, &master_dev_port) != kIOReturnSuccess)
        return -1;

    if (!(power_mgt = IOPMFindPowerManagement(master_dev_port)))
        return -1;

    if (rfbNoDimming) {
        if (saveDimSettings() < 0)
            return -1;
        if (IOPMSetAggressiveness(power_mgt,
                                  kPMMinutesToDim, 0) != kIOReturnSuccess)
            return -1;
    }

    if (rfbNoSleep) {
        if (saveSleepSettings() < 0)
            return -1;
        if (IOPMSetAggressiveness(power_mgt,
                                  kPMMinutesToSleep, 0) != kIOReturnSuccess)
            return -1;
    }

    initialized = TRUE;
    return 0;
}


int
rfbUndim(void)
{
    int result = -1;

    pthread_mutex_lock(&dimming_mutex);

    if (!initialized)
        goto DONE;

    if (!rfbNoDimming) {
        if (saveDimSettings() < 0)
            goto DONE;
        if (IOPMSetAggressiveness(power_mgt, kPMMinutesToDim, 0) != kIOReturnSuccess)
            goto DONE;
        if (restoreDimSettings() < 0)
            goto DONE;
    }

    if (!rfbNoSleep) {
        if (saveSleepSettings() < 0)
            goto DONE;
        if (IOPMSetAggressiveness(power_mgt, kPMMinutesToSleep, 0) != kIOReturnSuccess)
            goto DONE;
        if (restoreSleepSettings() < 0)
            goto DONE;
    }

    result = 0;

 DONE:
    pthread_mutex_unlock(&dimming_mutex);
    return result;
}


int
rfbDimmingShutdown(void)
{
    int result = -1;

    if (!initialized)
        goto DONE;

    pthread_mutex_lock(&dimming_mutex);
    if (dim_time_saved)
        if (restoreDimSettings() < 0)
            goto DONE;
    if (sleep_time_saved)
        if (restoreSleepSettings() < 0)
            goto DONE;

    result = 0;

 DONE:
    pthread_mutex_unlock(&dimming_mutex);
    return result;
}

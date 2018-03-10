/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017-2018 The LineageOS Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NIDEBUG 0

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define LOG_TAG "QCOM PowerHAL Libra"
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <cutils/properties.h>


#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"

#define POWER_NR_OF_SUPPORTED_PROFILES 5

#define POWER_PROFILE_PROPERTY  "sys.perf.profile"
#define POWER_NON_PROP          "non"
#define POWER_SAVE_PROP         "0"
#define BALANCED_PROP           "1"
#define HIGH_PERFORMANCE_PROP   "2"
#define BIAS_POWER_PROP         "3"
#define BIAS_PERFORMANCE_PROP   "4"
#define POWER_NO_PROFILE         2E

#define BIG_MIN_CPU_PATH "/sys/devices/system/cpu/cpu4/core_ctl/min_cpus"
#define BIG_MAX_CPU_PATH "/sys/devices/system/cpu/cpu4/core_ctl/max_cpus"

static int current_power_profile = POWER_NO_PROFILE;

static int display_hint_sent;

int get_number_of_profiles() {
    return POWER_NR_OF_SUPPORTED_PROFILES;
}

static void set_power_profile(int profile)
{
    if (profile == current_power_profile)
        return;
    
    ALOGV("%s: Profile=%d", __func__, profile);

    switch (profile) {
    case PROFILE_POWER_SAVE:
        property_set(POWER_PROFILE_PROPERTY, POWER_SAVE_PROP);
        ALOGD("%s: Set powersave mode", __func__);
        break;
    case PROFILE_BALANCED:
        property_set(POWER_PROFILE_PROPERTY, BALANCED_PROP);
        ALOGD("%s: Set balanced mode", __func__);
        break;
    case PROFILE_HIGH_PERFORMANCE:
        property_set(POWER_PROFILE_PROPERTY, HIGH_PERFORMANCE_PROP);
        ALOGD("%s: Set performance mode", __func__);
        break;
    case PROFILE_BIAS_POWER:
        property_set(POWER_PROFILE_PROPERTY, BIAS_POWER_PROP);
        ALOGD("%s: Set bias power mode", __func__);
        break;
    case PROFILE_BIAS_PERFORMANCE:
        property_set(POWER_PROFILE_PROPERTY, BIAS_PERFORMANCE_PROP);
        ALOGD("%s: Set bias perf mode", __func__);
        break;
    case POWER_NO_PROFILE:
        property_set(POWER_PROFILE_PROPERTY, POWER_NON_PROP);
        ALOGD("%s: Set normal mode", __func__);
        break;
    }

    current_power_profile = profile;
}

static int process_sustained_hint(void *data){
    if(*(*int32)data > 0)
        set_power_profile(PROFILE_POWER_SAVE);
    else
        set_power_profile(POWER_NO_PROFILE);
    return HINT_HANDLED;
}

static int process_low_power_hint(void *data){
    if(*(*int32)data > 0)
        set_power_profile(PROFILE_BIAS_PERFORMANCE);
    else
        set_power_profile(POWER_NO_PROFILE);
    return HINT_HANDLED;
}

static int process_video_encode_hint(void *metadata)
{
    char governor[80];
    struct video_encode_metadata_t video_encode_metadata;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");

        return HINT_NONE;
    }

    /* Initialize encode metadata struct fields */
    memset(&video_encode_metadata, 0, sizeof(struct video_encode_metadata_t));
    video_encode_metadata.state = -1;
    video_encode_metadata.hint_id = DEFAULT_VIDEO_ENCODE_HINT_ID;

    if (metadata) {
        if (parse_video_encode_metadata((char *)metadata, &video_encode_metadata) ==
            -1) {
            ALOGE("Error occurred while parsing metadata.");
            return HINT_NONE;
        }
    } else {
        return HINT_NONE;
    }

    if (video_encode_metadata.state == 1) {
        if (is_interactive_governor(governor)) {
            /* sched and cpufreq params
             * hispeed freq - 768 MHz
             * target load - 90
             * above_hispeed_delay - 40ms
             * sched_small_tsk - 50
             */
            int resource_values[] = {0x2C07, 0x2F5A, 0x2704, 0x4032};

            perform_hint_action(video_encode_metadata.hint_id,
                    resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
            return HINT_HANDLED;
        }
    } else if (video_encode_metadata.state == 0) {
        if (is_interactive_governor(governor)) {
            undo_hint_action(video_encode_metadata.hint_id);
            return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}

int power_hint_override(power_hint_t hint, void *data)
{
    int ret_val = HINT_NONE;
#ifdef CAN_SET_PROFILE
    if (hint == POWER_HINT_SET_PROFILE) {
        set_power_profile(*(int32_t *)data);
        return HINT_HANDLED;
    }
#else
    set_power_profile(POWER_NO_PROFILE);
#endif
    switch(hint) {
        case POWER_HINT_VIDEO_ENCODE:
            ret_val = process_video_encode_hint(data);
            break;
        case POWER_HINT_SUSTAINED_PERFORMANCE:
            ret_val = process_sustained_hint(data);
            break;
        case POWER_HINT_LOW_POWER:
            ret_val = process_low_power_hint(data);
            break;
        default:
            break;
    }
    return ret_val;
}


int set_interactive_override(int on)
{
    ALOGD("%sabling big CPU cluster", on ? "En" : "Dis");
    sysfs_write(BIG_MAX_CPU_PATH, on ? "2" : "0");
    sysfs_write(BIG_MIN_CPU_PATH, on ? "1" : "0");
    return HINT_HANDLED; /* Don't excecute this code path, not in use */
    
    char governor[80];

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");

        return HINT_NONE;
    }

    if (!on) {
        /* Display off */
        if (is_interactive_governor(governor)) {
            int resource_values[] = {}; /* dummy node */
            if (!display_hint_sent) {
                perform_hint_action(DISPLAY_STATE_HINT_ID,
                resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
                display_hint_sent = 1;
                return HINT_HANDLED;
            }
        }
    } else {
        /* Display on */
        if (is_interactive_governor(governor)) {
            undo_hint_action(DISPLAY_STATE_HINT_ID);
            display_hint_sent = 0;
            return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}

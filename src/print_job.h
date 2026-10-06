// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings.h"
#include "core/bitmap.h"

/* Background job that talks to the printer through the bridge. Progress is
 * reported through a callback invoked from the job thread. */

typedef enum {
    JobKindPrint,
    JobKindInfo,
} JobKind;

typedef enum {
    JobStatusStarting,
    JobStatusConnecting, /* arg: BridgeState */
    JobStatusChecking,
    JobStatusSending, /* arg: percent 0..100 */
    JobStatusFinishing,
    JobStatusDone,
    JobStatusFailed, /* arg: JobFailure */
    JobStatusCancelled,
} JobStatus;

typedef enum {
    JobFailureNoBridge,
    JobFailureNotFound,
    JobFailureConnect,
    JobFailureNoPaper,
    JobFailureOverheated,
    JobFailureWrite,
    JobFailureNoResponse,
    JobFailureNoMemory,
    JobFailureCoverOpen,
    JobFailureNoRadio, /* direct BLE selected but the firmware lacks the full stack */
} JobFailure;

typedef void (*JobCallback)(JobStatus status, uint8_t arg, void* context);

typedef struct PrintJob PrintJob;

/* `bitmap` is only read (and must stay alive) while the job runs. */
PrintJob* print_job_start(
    JobKind kind,
    const Settings* settings,
    const Bitmap* bitmap,
    JobCallback callback,
    void* context);

/* Ask the job to stop; returns once the thread has exited. */
void print_job_cancel_and_free(PrintJob* job);
/* For a job that already reported Done/Failed/Cancelled. */
void print_job_free(PrintJob* job);

/* Text produced by JobKindInfo, valid after JobStatusDone. */
const char* print_job_info(const PrintJob* job);

const char* job_failure_text(JobFailure f);

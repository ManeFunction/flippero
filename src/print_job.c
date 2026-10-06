// SPDX-License-Identifier: GPL-3.0-or-later
#include "print_job.h"
#include "link.h"
#include "core/fichero_proto.h"

#include <furi.h>

#define TAG "PrintJob"

#define CONNECT_TIMEOUT_MS 20000
#define HELLO_TIMEOUT_MS   1000
#define HELLO_TRIES        3
#define RESPONSE_TIMEOUT   3000
#define STOP_TIMEOUT_MS    60000
#define SEND_SLICE         600 /* bytes per progress update */

struct PrintJob {
    JobKind kind;
    Settings settings;
    const Bitmap* bitmap;
    JobCallback callback;
    void* context;
    FuriThread* thread;
    const LinkOps* ops;
    FuriMutex* lock; /* guards link + cancel */
    Link* link; /* non-NULL only while the job thread owns a live link */
    bool cancel;
    char info[128];
};

static void report(PrintJob* j, JobStatus s, uint8_t arg) {
    if(j->callback) j->callback(s, arg, j->context);
}

static void on_link_state(LinkState state, void* ctx) {
    report(ctx, JobStatusConnecting, state);
}

/* Write a command, then pause so the printer can digest it. */
static LinkResult send(PrintJob* j, Link* b, const uint8_t* data, size_t len, uint32_t post_delay_ms) {
    LinkResult r = j->ops->write(b, data, len);
    if(r == LinkOk && post_delay_ms) furi_delay_ms(post_delay_ms);
    return r;
}

/* Command that the web app sends with wait=true: write it, then wait for the printer's
 * reply (a missing reply is not an error). */
static LinkResult send_wait(
    PrintJob* j,
    Link* b,
    const uint8_t* data,
    size_t len,
    uint32_t timeout_ms,
    uint8_t* reply,
    size_t* reply_len) {
    j->ops->drain_notify(b);
    LinkResult r = j->ops->write(b, data, len);
    if(r != LinkOk) return r;

    uint8_t local[16];
    size_t cap = sizeof(local);
    uint8_t* out = reply ? reply : local;
    if(reply_len) cap = *reply_len < sizeof(local) ? *reply_len : sizeof(local);
    size_t got = cap;
    r = j->ops->wait_notify(b, out, &got, timeout_ms);
    if(r == LinkOk) {
        if(reply_len) *reply_len = got;
    } else {
        if(reply_len) *reply_len = 0;
        if(r == LinkTimeout) r = LinkOk;
    }
    if(r == LinkOk) furi_delay_ms(50); /* the web app lets fragments settle */
    return r;
}

static JobFailure failure_from(LinkResult r) {
    return r == LinkTimeout ? JobFailureNoResponse : JobFailureWrite;
}

static void format_info(PrintJob* j, Link* b) {
    const LinkOps* ops = j->ops;
    uint8_t cmd[FICHERO_CMD_MAX], resp[32];
    size_t n, rl;
    char model[24] = "?";
    int battery = -1;
    int status = -1;

    n = fichero_cmd_model(cmd);
    ops->drain_notify(b);
    if(send(j, b, cmd, n, 0) == LinkOk) {
        rl = sizeof(resp) - 1;
        if(ops->wait_notify(b, resp, &rl, RESPONSE_TIMEOUT) == LinkOk) {
            resp[rl] = 0;
            snprintf(model, sizeof(model), "%.20s", (char*)resp);
        }
    }

    n = fichero_cmd_battery(cmd);
    ops->drain_notify(b);
    if(send(j, b, cmd, n, 0) == LinkOk) {
        rl = sizeof(resp);
        if(ops->wait_notify(b, resp, &rl, RESPONSE_TIMEOUT) == LinkOk && rl >= 1)
            battery = resp[rl - 1]; /* [status, percent] */
    }

    n = fichero_cmd_status(cmd);
    ops->drain_notify(b);
    if(send(j, b, cmd, n, 0) == LinkOk) {
        rl = sizeof(resp);
        if(ops->wait_notify(b, resp, &rl, RESPONSE_TIMEOUT) == LinkOk && rl >= 1)
            status = resp[rl - 1];
    }

    char st[24] = "no reply";
    if(status >= 0) {
        if(status & FICHERO_ST_COVER_OPEN)
            snprintf(st, sizeof(st), "cover open");
        else if(status & FICHERO_ST_NO_PAPER)
            snprintf(st, sizeof(st), "no paper");
        else if(status & FICHERO_ST_OVERHEATED)
            snprintf(st, sizeof(st), "overheated");
        else if(status & FICHERO_ST_LOW_BATT)
            snprintf(st, sizeof(st), "low battery");
        else if(status & FICHERO_ST_PRINTING)
            snprintf(st, sizeof(st), "printing");
        else
            snprintf(st, sizeof(st), "ready");
    }
    char batt[12] = "?";
    if(battery >= 0) snprintf(batt, sizeof(batt), "%d%%", battery);

    snprintf(
        j->info,
        sizeof(j->info),
        "%s\nModel: %s\nBattery: %s\nStatus: %s",
        ops->peer_name(b),
        model,
        batt,
        st);
}

/* Returns JobStatusDone, or JobStatusFailed with *failure set. */
static JobStatus run_print(PrintJob* j, Link* b, JobFailure* failure, bool* enabled) {
    const LinkOps* ops = j->ops;
    uint8_t cmd[FICHERO_CMD_MAX], resp[16];
    size_t n, rl;
    LinkResult r;

    /* Build header + raster in one buffer. */
    size_t rows = j->bitmap->w;
    size_t total = FICHERO_RASTER_HEADER_LEN + rows * FICHERO_HEAD_BYTES;
    uint8_t* job_buf = malloc(total);
    if(!job_buf) {
        *failure = JobFailureNoMemory;
        return JobStatusFailed;
    }
    fichero_cmd_raster_header(job_buf, rows);
    fichero_bitmap_to_raster(j->bitmap, j->settings.flip, job_buf + FICHERO_RASTER_HEADER_LEN);

    JobStatus result = JobStatusDone;

    /* Same sequence as the reference web app (print_task.ts). */

    /* Refuse early on obvious hardware problems. No reply is not fatal. */
    report(j, JobStatusChecking, 0);
    n = fichero_cmd_status(cmd);
    rl = sizeof(resp);
    if((r = send_wait(j, b, cmd, n, RESPONSE_TIMEOUT, resp, &rl)) != LinkOk) goto fail_r;
    if(rl >= 1) {
        uint8_t st = resp[rl - 1];
        if(st & FICHERO_ST_COVER_OPEN) {
            *failure = JobFailureCoverOpen;
            result = JobStatusFailed;
        } else if(st & FICHERO_ST_NO_PAPER) {
            *failure = JobFailureNoPaper;
            result = JobStatusFailed;
        } else if(st & FICHERO_ST_OVERHEATED) {
            *failure = JobFailureOverheated;
            result = JobStatusFailed;
        }
    }

    if(result == JobStatusDone) {
        n = fichero_cmd_density(cmd, j->settings.density);
        if((r = send_wait(j, b, cmd, n, RESPONSE_TIMEOUT, NULL, NULL)) != LinkOk) goto fail_r;
        furi_delay_ms(100);
    }

    for(uint8_t copy = 0; result == JobStatusDone && copy < j->settings.copies; copy++) {
        n = fichero_cmd_paper(cmd, j->settings.paper);
        if((r = send_wait(j, b, cmd, n, RESPONSE_TIMEOUT, NULL, NULL)) != LinkOk) goto fail_r;
        furi_delay_ms(50);

        n = fichero_cmd_wake(cmd);
        if((r = send(j, b, cmd, n, 50)) != LinkOk) goto fail_r;

        /* The web app sends enable without waiting for a reply; waiting for "OK" made
         * printing fail because this printer does not send one. */
        n = fichero_cmd_enable(cmd);
        if((r = send(j, b, cmd, n, 50)) != LinkOk) goto fail_r;
        *enabled = true;

        for(size_t off = 0; off < total; off += SEND_SLICE) {
            size_t slice = total - off < SEND_SLICE ? total - off : SEND_SLICE;
            if((r = ops->write(b, job_buf + off, slice)) != LinkOk) goto fail_r;
            uint32_t done = (uint32_t)(off + slice) * 100 / total;
            uint32_t overall = (copy * 100 + done) / j->settings.copies;
            report(j, JobStatusSending, overall);
        }
        furi_delay_ms(500);

        report(j, JobStatusFinishing, 0);
        n = fichero_cmd_feed(cmd);
        if((r = send(j, b, cmd, n, 300)) != LinkOk) goto fail_r;

        n = fichero_cmd_stop(cmd);
        rl = sizeof(resp);
        r = send_wait(j, b, cmd, n, STOP_TIMEOUT_MS, resp, &rl);
        *enabled = false;
        if(r == LinkCancelled) goto fail_r;
        if(r != LinkOk) goto fail_r;
    }
    goto finish;

fail_r:
    if(r == LinkCancelled) {
        result = JobStatusCancelled;
    } else {
        *failure = failure_from(r);
        result = JobStatusFailed;
    }

finish:
    free(job_buf);
    return result;
}

static int32_t job_thread(void* context) {
    PrintJob* j = context;
    const LinkOps* ops = j->ops;
    JobFailure failure = JobFailureConnect;
    JobStatus final = JobStatusFailed;
    bool enabled = false;

    report(j, JobStatusStarting, 0);
    Link* b = ops->alloc();
    if(!b) {
        failure = j->settings.link == LinkKindBle ? JobFailureNoRadio : JobFailureNoBridge;
        goto out;
    }
    ops->set_state_callback(b, on_link_state, j);
    furi_mutex_acquire(j->lock, FuriWaitForever);
    j->link = b;
    if(j->cancel) ops->cancel(b); /* Back was pressed while the link was being set up */
    furi_mutex_release(j->lock);

    LinkResult r = LinkFailed;
    for(int i = 0; i < HELLO_TRIES && (r = ops->ready(b, HELLO_TIMEOUT_MS)) == LinkTimeout; i++)
        ;
    if(r == LinkCancelled) {
        final = JobStatusCancelled;
        goto out;
    }
    if(r != LinkOk) {
        failure = JobFailureNoBridge;
        goto out;
    }

    report(j, JobStatusConnecting, BridgeStateIdle);
    LinkState state = BridgeStateIdle;
    r = ops->connect(b, PRINTER_NAME_PREFIX, CONNECT_TIMEOUT_MS, &state);
    if(r == LinkCancelled) {
        final = JobStatusCancelled;
        goto out;
    }
    if(r != LinkOk) {
        failure = state == BridgeStateNotFound ? JobFailureNotFound : JobFailureConnect;
        goto out;
    }

    if(j->kind == JobKindInfo) {
        format_info(j, b);
        final = JobStatusDone;
    } else {
        final = run_print(j, b, &failure, &enabled);
    }

out:
    /* No more cancels can reach the link once it is detached from the job. */
    furi_mutex_acquire(j->lock, FuriWaitForever);
    j->link = NULL;
    furi_mutex_release(j->lock);
    if(b && enabled) {
        /* Cancelled/failed mid-job: best-effort release the printer. */
        uint8_t cmd[FICHERO_CMD_MAX];
        ops->clear_cancel(b);
        size_t n = fichero_cmd_stop(cmd);
        ops->write(b, cmd, n);
    }
    if(b) ops->free(b);

    if(final == JobStatusFailed)
        report(j, JobStatusFailed, failure);
    else
        report(j, final, 0);
    return 0;
}

PrintJob* print_job_start(
    JobKind kind,
    const Settings* settings,
    const Bitmap* bitmap,
    JobCallback callback,
    void* context) {
    PrintJob* j = malloc(sizeof(PrintJob));
    memset(j, 0, sizeof(PrintJob));
    j->kind = kind;
    j->settings = *settings;
    j->ops = link_ops_for(settings->link);
    j->bitmap = bitmap;
    j->callback = callback;
    j->context = context;
    j->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    j->thread = furi_thread_alloc_ex("FicheroJob", 3072, job_thread, j);
    furi_thread_start(j->thread);
    return j;
}

void print_job_cancel_and_free(PrintJob* j) {
    if(!j) return;
    furi_mutex_acquire(j->lock, FuriWaitForever);
    j->cancel = true;
    if(j->link) j->ops->cancel(j->link);
    furi_mutex_release(j->lock);
    print_job_free(j); /* joins the thread */
}

void print_job_free(PrintJob* j) {
    if(!j) return;
    furi_thread_join(j->thread);
    furi_thread_free(j->thread);
    furi_mutex_free(j->lock);
    free(j);
}

const char* print_job_info(const PrintJob* j) {
    return j->info;
}

const char* job_failure_text(JobFailure f) {
    switch(f) {
    case JobFailureNoBridge:
        return "ESP32 bridge not\nresponding. Check wiring.";
    case JobFailureNoRadio:
        return "Full BLE stack not\nfound. Install Blue++.";
    case JobFailureNotFound:
        return "Printer not found.\nIs it on and in range?";
    case JobFailureConnect:
        return "Could not connect\nto the printer.";
    case JobFailureCoverOpen:
        return "Printer cover is open.";
    case JobFailureNoPaper:
        return "Printer is out\nof paper.";
    case JobFailureOverheated:
        return "Printer overheated.\nLet it cool down.";
    case JobFailureWrite:
        return "Lost connection\nwhile sending.";
    case JobFailureNoResponse:
        return "Printer did not\nacknowledge.";
    case JobFailureNoMemory:
        return "Out of memory.";
    }
    return "Unknown error.";
}

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "serial.h"
#include "utils.h"
#include "global_state.h"
#include "nvs_config.h"
#include "system.h"
#include "boards/board.h"

#include "simple_ring64.hpp"
#include "utils.h"

static const char *TAG = "asic_result";

static SimpleRing64<32> s_seen_keys;

static uint64_t duplicateHWNonces = 0;

static void countDuplicateHWNonces() {
    duplicateHWNonces++;
}

// [PROBE] BM1373 core-distribution / extended-register diagnostics. Disabled
// for now — findings are saved in BM1373_registros_explorados.md. Set to 1
// (here AND in hashrate_monitor_task.cpp) to re-enable the probe.
#define PROBE_ENABLED 0

#if PROBE_ENABLED
// [PROBE] Per-chip histogram of nonces by core_id_7b and small_core_id.
// Tracks distribution of nonce production across the chip's internal cores
// to discover how many domains/cores are actually active on BM1373.
#define PROBE_MAX_CHIPS 8
#define PROBE_MAX_CORE_ID 128       // core_id_7b is 7 bits → 0..127
#define PROBE_MAX_SMALL_CORE_ID 16  // small_core_id is nibble → 0..15
// Cumulative histograms — never reset. Accumulate forever so the distribution
// converges as more nonces arrive.
static uint32_t probe_core_hits[PROBE_MAX_CHIPS][PROBE_MAX_CORE_ID]   = {0};
static uint32_t probe_small_core_hits[PROBE_MAX_CHIPS][PROBE_MAX_SMALL_CORE_ID] = {0};
static uint32_t probe_total_nonces = 0;
static int64_t  probe_last_dump_us = 0;
static constexpr int64_t PROBE_DUMP_INTERVAL_US = 60LL * 1000000LL; // 60 seconds

static void probe_log_nonce(uint8_t chip, uint8_t core_id_7b, uint8_t small_core_id)
{
    if (chip < PROBE_MAX_CHIPS && core_id_7b < PROBE_MAX_CORE_ID) {
        probe_core_hits[chip][core_id_7b]++;
    }
    if (chip < PROBE_MAX_CHIPS && small_core_id < PROBE_MAX_SMALL_CORE_ID) {
        probe_small_core_hits[chip][small_core_id]++;
    }
    probe_total_nonces++;
}

static void probe_dump_histogram_if_due(void)
{
    int64_t now = esp_timer_get_time();
    if (probe_last_dump_us == 0) {
        probe_last_dump_us = now;
        return;
    }
    if ((now - probe_last_dump_us) < PROBE_DUMP_INTERVAL_US) {
        return;
    }
    probe_last_dump_us = now;

    ESP_LOGI(TAG, "[PROBE-HIST] === %u total nonces accumulated ===", (unsigned) probe_total_nonces);

    for (int chip = 0; chip < PROBE_MAX_CHIPS; chip++) {
        int active_cores = 0;
        uint8_t min_core = 0xFF, max_core = 0;
        uint32_t chip_total = 0;
        // Split stats: how many in low half (0-63) vs high half (64-127)
        int active_low = 0, active_high = 0;
        uint32_t total_low = 0, total_high = 0;

        for (int c = 0; c < PROBE_MAX_CORE_ID; c++) {
            if (probe_core_hits[chip][c] > 0) {
                active_cores++;
                if (c < min_core) min_core = c;
                if (c > max_core) max_core = c;
                chip_total += probe_core_hits[chip][c];
                if (c < 64) {
                    active_low++;
                    total_low += probe_core_hits[chip][c];
                } else {
                    active_high++;
                    total_high += probe_core_hits[chip][c];
                }
            }
        }
        if (chip_total == 0) continue;

        ESP_LOGI(TAG, "[PROBE-HIST] chip=%d active_core_ids=%d range=[%u..%u] total_nonces=%u",
                 chip, active_cores, (unsigned) min_core, (unsigned) max_core, (unsigned) chip_total);
        ESP_LOGI(TAG, "[PROBE-HIST] chip=%d low_half(0-63)=%d cores %u nonces  |  high_half(64-127)=%d cores %u nonces",
                 chip, active_low, (unsigned) total_low, active_high, (unsigned) total_high);

        // Compact dump: only show non-zero entries
        for (int c = 0; c < PROBE_MAX_CORE_ID; c++) {
            if (probe_core_hits[chip][c] > 0) {
                ESP_LOGI(TAG, "[PROBE-HIST]   chip=%d core_id_7b=%3d hits=%u",
                         chip, c, (unsigned) probe_core_hits[chip][c]);
            }
        }
        for (int s = 0; s < PROBE_MAX_SMALL_CORE_ID; s++) {
            if (probe_small_core_hits[chip][s] > 0) {
                ESP_LOGI(TAG, "[PROBE-HIST]   chip=%d small_core_id=%2d hits=%u",
                         chip, s, (unsigned) probe_small_core_hits[chip][s]);
            }
        }
    }

    // NOTE: histograms are NOT reset — keep accumulating so distribution
    // converges with more samples. Reboot to clear.
}
#endif // PROBE_ENABLED

uint64_t getDuplicateHWNonces() {
    return duplicateHWNonces;
}

// Combine nonce + version into a single 64-bit key
static inline uint64_t make_key(uint32_t nonce, uint32_t version)
{
    // This order must be consistent everywhere
    return (uint64_t(nonce) << 32) | uint64_t(version);
}

void ASIC_result_task(void *pvParameters)
{
    Board* board = SYSTEM_MODULE.getBoard();
    Asic* asics = board->getAsics();

    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(TAG, "suspended");
            vTaskSuspend(NULL);
        }
        //ESP_LOGI("Memory", "%lu", esp_get_free_heap_size()); test
        task_result asic_result;

        // get the result
        if (!asics->processWork(&asic_result)) {
            continue;
        }

        if (asic_result.is_reg_resp) {
            switch (asic_result.reg) {
                case 0xb4: {
                    if (asic_result.data & 0x80000000) {
                        float ftemp = (float) (asic_result.data & 0x0000ffff) * 0.171342f - 299.5144f;
                        ESP_LOGI(TAG, "asic %d temp: %.3f", (int) asic_result.asic_nr, ftemp);
                        board->setChipTemp(asic_result.asic_nr, ftemp);
                    }
                    break;
                }
                case 0x48: {
                    // maybe the upper 32bit of a 64bit counter and 0x90 returns the lower 32bit
                    break;
                }
                case 0x8C: {
                    // 0x8C = REGISTER_TOTAL_COUNT (chip-wide total). Only this one
                    // feeds the hashrate monitor — passing multiple registers would
                    // cause m_prevCounter to be overwritten between cycles and
                    // produce wildly wrong delta calculations.
                    HASHRATE_MONITOR.onRegisterReply(asic_result.asic_nr, asic_result.data);
#if PROBE_ENABLED
                    ESP_LOGI(TAG, "[PROBE] reg=0x%02X asic=%d value=%lu (0x%08lX)",
                             (unsigned int) asic_result.reg,
                             (int) asic_result.asic_nr,
                             (unsigned long) asic_result.data,
                             (unsigned long) asic_result.data);
#endif
                    break;
                }
#if PROBE_ENABLED
                // [PROBE] Log responses from extended register addresses to find
                // hidden domain counters for BM1373. If any of these return a
                // counter-like value that grows over time, the chip is exposing
                // additional domains we haven't been summing.
                case 0x88:
                case 0x89:
                case 0x8A:
                case 0x8B:
                case 0x8D:
                case 0x8E:
                case 0x8F:
                case 0x90:    // 0x90 returns a non-zero value on BM1373 — investigate what it represents
                case 0x91:
                case 0x92:
                case 0x93:
                case 0x94:
                case 0x95:
                case 0x4C: {  // ERROR_COUNT candidate per bitaxe docs
                    ESP_LOGI(TAG, "[PROBE] reg=0x%02X asic=%d value=%lu (0x%08lX)",
                             (unsigned int) asic_result.reg,
                             (int) asic_result.asic_nr,
                             (unsigned long) asic_result.data,
                             (unsigned long) asic_result.data);
                    break;
                }
#endif // PROBE_ENABLED
                default: {
#if PROBE_ENABLED
                    // log anything else unexpected, with reduced detail
                    ESP_LOGI(TAG, "[PROBE-UNK] reg=0x%02X asic=%d value=0x%08lX",
                             (unsigned int) asic_result.reg,
                             (int) asic_result.asic_nr,
                             (unsigned long) asic_result.data);
#endif
                    break;
                }
            }
            continue;
        }

        uint8_t asic_job_id = asic_result.job_id;

#if PROBE_ENABLED
        // [PROBE] Track every received nonce in the histogram so we know which
        // physical cores of the chip are actually producing nonces. This runs
        // before the job-clone lookup so we also count "orphan" nonces.
        probe_log_nonce((uint8_t) asic_result.asic_nr,
                        asic_result.core_id_7b,
                        asic_result.small_core_id);
        probe_dump_histogram_if_due();
#endif

        bm_job *job = asicJobs.getClone(asic_job_id);
        if (!job) {
            //ESP_LOGI(TAG, "Invalid job id found, 0x%02X", asic_job_id);
            continue;
        }

        // now we have the original job and can `or` the version
        asic_result.rolled_version |= job->version;

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(job, asic_result.nonce, asic_result.rolled_version);

        // get best known session diff
        char bestDiffString[16];
        suffixString(STRATUM_MANAGER->getBestSessionDiff(), bestDiffString, sizeof(bestDiffString), 3);

        const char *pool_str = job->pool_id ? "Sec" : "Pri";

        // log the ASIC response, including pool and best session difficulty using human-readable SI formatting
        // we only show responses >= maxAsicDifficulty to avoid spamming the log
        // change for dual pool because the pool with lower % can reduce asic HW difficulty
        if (nonce_diff >= board->getAsicMaxDifficulty() || nonce_diff >= job->pool_diff) {
            ESP_LOGI(TAG, "(%s) Job ID: %02X AsicNr: %d Ver: %08" PRIX32 " Nonce %08" PRIX32 "; Extranonce2 %s diff %.1f/%lu/%s",
                pool_str, asic_job_id, asic_result.asic_nr, asic_result.rolled_version, asic_result.nonce, job->extranonce2,
                nonce_diff, job->pool_diff, bestDiffString);
        }

        uint64_t key = make_key(asic_result.nonce, asic_result.rolled_version);
        bool duplicate = !s_seen_keys.insert_if_absent(key);
        if (duplicate) {
            ESP_LOGW(TAG, "(%s) duplicate share detected!", pool_str);
            countDuplicateHWNonces();
        }

        if (!duplicate && nonce_diff >= board->getAsicMaxDifficulty()) {
            SYSTEM_MODULE.pushShare(asic_result.asic_nr);
        }

        // send duplicates to the server (they will get rejected and counted as rejected)
        if (nonce_diff >= job->pool_diff) {
            STRATUM_MANAGER->submitShare(job->pool_id, job->jobid, job->extranonce2, job->ntime, asic_result.nonce,
                                    asic_result.rolled_version, job->version);
        }

        STRATUM_MANAGER->checkForBestDiff(job->pool_id, nonce_diff, job->target);

        STRATUM_MANAGER->checkForFoundBlock(job->pool_id, nonce_diff, job->target);


        free_bm_job(job);
    }
}

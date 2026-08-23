#!/usr/bin/env bash
# Script: capture-dsi-diag-smoketest.sh
# Author: kelexine <https://github.com/kelexine>
# Date: 2026-08-23
# Purpose: Live DSI/panel diagnostic smoke test via ADB for Cubot P50 (MT6765).
#          Captures /d/disp/dsi_diag (or /proc/disp/dsi_diag) across three boot
#          phases (T0: post-reboot, T1: boot animation, T2: +30s steady state),
#          diffs the captures, and highlights changes in DSI power, ULPS, and FSM.
# Usage: ./capture-dsi-diag-smoketest.sh [--reboot | --no-reboot] [output_dir]

set -euo pipefail

DO_REBOOT=1
OUTPUT_DIR=""

for arg in "$@"; do
    case "${arg}" in
        --no-reboot)
            DO_REBOOT=0
            ;;
        --reboot)
            DO_REBOOT=1
            ;;
        -h|--help)
            echo "Usage: $0 [--reboot | --no-reboot] [output_dir]"
            echo ""
            echo "Options:"
            echo "  --reboot      Trigger 'adb reboot' and wait for device (default)"
            echo "  --no-reboot   Skip reboot; start sampling immediately on connected device"
            echo "  output_dir    Directory to save diagnostic captures (default: ./dsi_diag_logs/<timestamp>)"
            exit 0
            ;;
        *)
            OUTPUT_DIR="${arg}"
            ;;
    esac
done

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
if [[ -z "${OUTPUT_DIR}" ]]; then
    OUTPUT_DIR="./dsi_diag_logs/${TIMESTAMP}"
fi
mkdir -p "${OUTPUT_DIR}"

LOG_T0="${OUTPUT_DIR}/01_early_boot_t0.txt"
LOG_T1="${OUTPUT_DIR}/02_boot_anim_t1.txt"
LOG_T2="${OUTPUT_DIR}/03_steady_state_30s_t2.txt"
SUMMARY_FILE="${OUTPUT_DIR}/dsi_diag_summary.txt"

echo "============================================================"
echo " Cubot P50 Live DSI Diagnostics Smoke Test"
echo " Target Output: ${OUTPUT_DIR}"
echo "============================================================"

# Ensure ADB is available
if ! command -v adb >/dev/null 2>&1; then
    echo "[-] Error: 'adb' command not found in PATH." >&2
    exit 1
fi

wait_for_adb_root() {
    echo "[*] Waiting for ADB device..."
    adb wait-for-device
    echo "[*] Waiting for shell / su access..."
    local attempts=0
    while ! adb shell "su -c 'id'" 2>/dev/null | grep -q "uid=0"; do
        sleep 1
        attempts=$((attempts + 1))
        if (( attempts > 120 )); then
            echo "[-] Error: Timeout waiting for root access via adb su." >&2
            exit 1
        fi
    done
    echo "[+] Root shell accessible."
}

capture_dsi_node() {
    local outfile="$1"
    local stage="$2"
    echo "[*] Capturing Stage: ${stage} -> ${outfile}..."

    # Read from /d/disp/dsi_diag or fallback to /proc/disp/dsi_diag
    local cmd="if [ -f /d/disp/dsi_diag ]; then cat /d/disp/dsi_diag; elif [ -f /proc/disp/dsi_diag ]; then cat /proc/disp/dsi_diag; elif [ -f /sys/kernel/debug/disp/dsi_diag ]; then cat /sys/kernel/debug/disp/dsi_diag; else echo 'ERR: dsi_diag node not found'; fi"

    adb shell "su -c \"${cmd}\"" > "${outfile}" 2>&1 || true

    if grep -q "ERR: dsi_diag node not found" "${outfile}"; then
        echo "[-] Warning: dsi_diag node not found on device!" >&2
    else
        echo "[+] Capture complete ($(wc -l < "${outfile}") lines)."
    fi
}

extract_key_fields() {
    local file="$1"
    echo "--- Summary for $(basename "${file}") ---"
    grep -E "Timestamp:|_is_power_on_status|is_mipi_enterulps|Driver Name:|disp_lcm_is_inited|DSI Mode:|DSI_START:|DSI_INTSTA:|DSI State" "${file}" || echo "  (no matching fields found)"
    echo ""
}

# --- Phase 1: Reboot and T0 Capture ---
if [[ "${DO_REBOOT}" -eq 1 ]]; then
    echo "[*] Triggering device reboot..."
    adb reboot || true
    sleep 3
    wait_for_adb_root
else
    wait_for_adb_root
fi

echo "[*] [Point 1/3] Sampling immediately after boot handoff (T0)..."
capture_dsi_node "${LOG_T0}" "T0_Early_Boot"

# --- Phase 2: T1 Capture (Boot Animation Stage) ---
echo "[*] [Point 2/3] Waiting ~10s for boot animation stage (T1)..."
sleep 10
capture_dsi_node "${LOG_T1}" "T1_Boot_Animation"

# --- Phase 3: T2 Capture (+30s Steady State) ---
echo "[*] [Point 3/3] Waiting +30s for steady-state / black screen check (T2)..."
sleep 30
capture_dsi_node "${LOG_T2}" "T2_Steady_State_30s"

# --- Analysis & Diff Generation ---
echo ""
echo "============================================================"
echo " DIAGNOSTIC SMOKE TEST ANALYSIS & FIELD DIFF"
echo "============================================================"

{
    echo "============================================================"
    echo " Cubot P50 DSI Diagnostic Smoke Test Summary"
    echo " Timestamp: ${TIMESTAMP}"
    echo "============================================================"
    echo ""
    extract_key_fields "${LOG_T0}"
    extract_key_fields "${LOG_T1}"
    extract_key_fields "${LOG_T2}"

    echo "============================================================"
    echo " DIFF: Stage T0 (Early Boot) vs Stage T1 (Boot Animation)"
    echo "============================================================"
    diff -u "${LOG_T0}" "${LOG_T1}" || true
    echo ""

    echo "============================================================"
    echo " DIFF: Stage T1 (Boot Animation) vs Stage T2 (+30s Steady)"
    echo "============================================================"
    diff -u "${LOG_T1}" "${LOG_T2}" || true
    echo ""
} | tee "${SUMMARY_FILE}"

echo ""
echo "[+] Smoke test completed. All logs and diffs saved to:"
echo "    ${OUTPUT_DIR}"

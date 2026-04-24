#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SAMPLE_SRC="${ROOT_DIR}/samples/linux-user/hello_linux_tiny.S"
OUTPUT_C="${ROOT_DIR}/kernel/core/elf_sample_blob.c"

if ! command -v gcc >/dev/null 2>&1; then
    echo "[refresh-elf-sample] gcc not found"
    exit 1
fi

if ! command -v od >/dev/null 2>&1; then
    echo "[refresh-elf-sample] od not found"
    exit 1
fi

if [ ! -f "${SAMPLE_SRC}" ]; then
    echo "[refresh-elf-sample] source not found: ${SAMPLE_SRC}"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

ELF_OUT="${TMP_DIR}/hello_linux_tiny.elf"

gcc -nostdlib -static -Wl,--build-id=none -Wl,-z,max-page-size=0x10 -Wl,-N -s -o "${ELF_OUT}" "${SAMPLE_SRC}"

{
    echo "#include <stdint.h>"
    echo
    echo "const uint8_t g_linux_user_elf_sample[] = {"
    od -An -v -t x1 "${ELF_OUT}" | awk '
        {
            for (i = 1; i <= NF; ++i) {
                if ((count % 12) == 0) {
                    printf "  ";
                }
                printf "0x%s,", $i;
                count++;
                if ((count % 12) == 0) {
                    printf "\n";
                } else {
                    printf " ";
                }
            }
        }
        END {
            if ((count % 12) != 0) {
                printf "\n";
            }
        }
    '
    echo "};"
    echo
    echo "const uint64_t g_linux_user_elf_sample_len = sizeof(g_linux_user_elf_sample);"
} > "${OUTPUT_C}"

echo "[refresh-elf-sample] generated: ${OUTPUT_C}"
echo "[refresh-elf-sample] source: ${SAMPLE_SRC}"
echo "[refresh-elf-sample] size: $(wc -c < "${ELF_OUT}") bytes"

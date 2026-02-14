#include <cstring>
#include <span>
#include <algorithm> // for std::min
#include <bit> // for std::byteswap
#include <utility> // std::unreachable
#include <switch.h>
#include "minIni/minIni.h"

namespace {

constexpr u64 INNER_HEAP_SIZE = 0x1000; // Size of the inner heap (adjust as necessary).
constexpr u64 READ_BUFFER_SIZE = 0x1000; // size of static buffer which memory is read into
constexpr u32 FW_VER_ANY = 0x0;
constexpr u16 REGEX_SKIP = 0x100;

u32 FW_VERSION{}; // set on startup
u32 AMS_VERSION{}; // set on startup
u32 AMS_TARGET_VERSION{}; // set on startup
u8 AMS_KEYGEN{}; // set on startup
u64 AMS_HASH{}; // set on startup
bool VERSION_SKIP{}; // set on startup

template<typename T>
constexpr void str2hex(const char* s, T* data, u8& size) {
    // skip leading 0x (if any)
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    // invalid string will cause a compile-time error due to no return
    constexpr auto hexstr_2_nibble = [](char c) -> u8 {
        if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
        if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
        if (c >= '0' && c <= '9') { return c - '0'; }
    };

    // parse and convert string
    while (*s != '\0') {
        if (sizeof(T) == sizeof(u16) && *s == '.') {
            data[size] = REGEX_SKIP;
            s++;
        } else {
            data[size] |= hexstr_2_nibble(*s++) << 4;
            data[size] |= hexstr_2_nibble(*s++) << 0;
        }
        size++;
    }
}

struct PatternData {
    constexpr PatternData(const char* s) {
        str2hex(s, data, size);
    }

    u16 data[60]{}; // reasonable max pattern length, adjust as needed
    u8 size{};
};

struct PatchData {
    constexpr PatchData(const char* s) {
        str2hex(s, data, size);
    }

    template<typename T>
    constexpr PatchData(T v) {
        for (u32 i = 0; i < sizeof(T); i++) {
            data[size++] = v & 0xFF;
            v >>= 8;
        }
    }

    constexpr auto cmp(const void* _data) -> bool {
        return !std::memcmp(data, _data, size);
    }

    u8 data[24]{}; // reasonable max patch length, adjust as needed
    u8 size{};
};

enum class PatchResult {
    NOT_FOUND,
    SKIPPED,
    DISABLED,
    PATCHED_FILE,
    PATCHED_SYSDOCK,
    FAILED_WRITE,
};

struct Patterns {
    const char* patch_name; // name of patch (unique per variant, used for logging)
    const char* config_key; // config.ini key (shared among variants of the same logical patch)
    const PatternData byte_pattern; // the pattern to search

    const s32 inst_offset; // instruction offset relative to byte pattern
    const s32 patch_offset; // patch offset relative to inst_offset

    bool (*const cond)(u32 inst); // check condition of the instruction
    PatchData (*const patch)(u32 inst); // the patch data to be applied
    bool (*const applied)(const u8* data, u32 inst); // check to see if patch already applied

    bool enabled; // controlled by config.ini
    const s32 mutual_exclusivity_group{-1}; // patches with the same group (>= 0) are mutually exclusive

    const u32 min_fw_ver{FW_VER_ANY}; // set to FW_VER_ANY to ignore
    const u32 max_fw_ver{FW_VER_ANY}; // set to FW_VER_ANY to ignore
    const u32 min_ams_ver{FW_VER_ANY}; // set to FW_VER_ANY to ignore
    const u32 max_ams_ver{FW_VER_ANY}; // set to FW_VER_ANY to ignore

    PatchResult result{PatchResult::NOT_FOUND};
};

struct PatchEntry {
    const char* name; // name of the system title
    const u64 title_id; // title id of the system title
    const std::span<Patterns> patterns; // list of patterns to find
    const u32 min_fw_ver{FW_VER_ANY}; // set to FW_VER_ANY to ignore
    const u32 max_fw_ver{FW_VER_ANY}; // set to FW_VER_ANY to ignore
};

constexpr auto movz_cond(u32 inst) -> bool {
    return (inst >> 24) == 0x52; // MOVZ Wn, #imm
}

constexpr auto strb_cond(u32 inst) -> bool {
    return (inst >> 24) == 0x39; // STRB Wt, [Xn, #imm]
}

constexpr auto tbnz_cond(u32 inst) -> bool {
    return (inst >> 24) == 0x37; // TBNZ Rt, #imm, label
}

constexpr auto cbz_cond(u32 inst) -> bool {
    return (inst >> 24) == 0x34; // CBZ Rt, label
}

constexpr auto bcond_cond(u32 inst) -> bool {
    return (inst >> 24) == 0x54; // B.cond label
}

constexpr auto bcond_or_tbnz_cond(u32 inst) -> bool {
    return bcond_cond(inst) || tbnz_cond(inst);
}

// to view patches, use https://armconverter.com/?lock=arm64
constexpr PatchData nop_patch_data{ "0x1F2003D5" };
constexpr PatchData nop5_patch_data{ "0x1F2003D51F2003D51F2003D51F2003D51F2003D5" };
constexpr PatchData nop6_patch_data{ "0x1F2003D51F2003D51F2003D51F2003D51F2003D51F2003D5" };
constexpr PatchData mov_w9_0x406_patch_data{ "0xC9808052" };
constexpr PatchData mov_w20_0x406_patch_data{ "0xD4808052" };

constexpr auto nop_patch(u32 inst) -> PatchData { return nop_patch_data; }
constexpr auto nop5_patch(u32 inst) -> PatchData { return nop5_patch_data; }
constexpr auto nop6_patch(u32 inst) -> PatchData { return nop6_patch_data; }
constexpr auto mov_w9_0x406_patch(u32 inst) -> PatchData { return mov_w9_0x406_patch_data; }
constexpr auto mov_w20_0x406_patch(u32 inst) -> PatchData { return mov_w20_0x406_patch_data; }

constexpr auto nop_applied(const u8* data, u32 inst) -> bool { return nop_patch(inst).cmp(data); }
constexpr auto nop5_applied(const u8* data, u32 inst) -> bool { return nop5_patch(inst).cmp(data); }
constexpr auto nop6_applied(const u8* data, u32 inst) -> bool { return nop6_patch(inst).cmp(data); }
constexpr auto mov_w9_0x406_applied(const u8* data, u32 inst) -> bool { return mov_w9_0x406_patch(inst).cmp(data); }
constexpr auto mov_w20_0x406_applied(const u8* data, u32 inst) -> bool { return mov_w20_0x406_patch(inst).cmp(data); }


constinit Patterns nvservices_patterns[] = {
    // 79 01 00 34  CBZ     W25, loc_7100042540     <-- nop
    // 2A 05 91 52  MOV     W10, #0x48829           <-- nop
    // 8A 00 A0 72                                  <-- nop
    // FF 02 0A 6B  CMP     W23, W10                <-- nop
    // E3 00 00 54  B.CC    loc_7100042540          <-- nop
    // 69 EE 0C 39  STRB    W9, [X19,#(g_dp_lane_count)]
    { "21.0.0+ no_lane_downgrade", "no_lane_downgrade", "0x...34.059152.00a072...6b...54.ee0c39", 0, 0, cbz_cond, nop5_patch, nop5_applied, false, -1, MAKEHOSVERSION(21,0,0), FW_VER_ANY},
    // same as above, but there's an extra LDUR after the CBZ
    { "17.0.0-17.0.1 no_lane_downgrade", "no_lane_downgrade", "0x...34.....059152.00a072...6b...54.ee0c39", 0, 0, cbz_cond, nop6_patch, nop6_applied, false, -1, MAKEHOSVERSION(17,0,0), MAKEHOSVERSION(17,0,1)},
    // 2A 05 91 52  MOV     W10, #0x48829
    // 8A 00 A0 72
    // FF 02 0A 6B  CMP     W23, W10
    // 42 00 00 54  B.CS    loc_71000405C0
    // 49 00 80 52  MOV     W9, #2                  <-- nop
    // 69 EE 0C 39  STRB    W9, [X19,#(g_dp_lane_count)]
    { "11.0.0-16.1.0 no_lane_downgrade", "no_lane_downgrade", "0x.059152.00a072...6b...54.008052.ee0c39", 16, 0, movz_cond, nop_patch, nop_applied, false, -1, MAKEHOSVERSION(11,0,0), MAKEHOSVERSION(16,1,0)},
    // same as above, just because of no_lane_downgrade_2 we need to put next FW versions in separate table
    { "18.0.0-20.5.0 no_lane_downgrade", "no_lane_downgrade", "0x.059152.00a072...6b...54.008052.ee0c39", 16, 0, movz_cond, nop_patch, nop_applied, false, -1, MAKEHOSVERSION(18,0,0), MAKEHOSVERSION(20,5,0)},
    // 61 00 00 54  B.NE    loc_71000427B4
    // 48 01 80 52  MOV     W8, #0xA
    // 68 E6 0C 39  STRB    W8, [X19,#(g_dp_current_link_bw)]   <-- nop
    // 61 E6 0C 91  ADD     X1, X19, #0x339
    // 62 EA 0C 91  ADD     X2, X19, #0x33A
    { "no_bw_downgrade", "no_bw_downgrade", "0x.....018052.e60c39..0c91..0c91", 8, 0, strb_cond, nop_patch, nop_applied, false, 0, MAKEHOSVERSION(11,0,0), FW_VER_ANY},
    // same pattern as above, except we nop the B.NE and force the STRB
    { "force_bw_downgrade", "force_bw_downgrade", "0x.....018052.e60c39..0c91..0c91", 0, 0, bcond_or_tbnz_cond, nop_patch, nop_applied, false, 0, MAKEHOSVERSION(11,0,0), FW_VER_ANY},
};

constinit Patterns usb_patterns[] = {
    // C0 03 5F D6  RET
    // D4 00 81 52  MOV     W20, #0x806             <-- 0x406
    { "15.0.0+ force_dp_mode_c", "force_dp_mode_c", "0xC0035FD6D4008152", 4, 0, movz_cond, mov_w20_0x406_patch, mov_w20_0x406_applied, false, -1, MAKEHOSVERSION(15,0,0), FW_VER_ANY},
    // C0 10 84 52  MOV     W0, #0x2086
    // DC FF FF 17  B       loc_7100054E38
    // D4 00 81 52  MOV     W20, #0x806             <-- 0x406
    { "12.0.0-14.1.2 force_dp_mode_c", "force_dp_mode_c", "0xC0108452....D4008152", 8, 0, movz_cond, mov_w20_0x406_patch, mov_w20_0x406_applied, false, -1, MAKEHOSVERSION(12,0,0), MAKEHOSVERSION(14,1,2)},
    // 1F 01 0E 72  TST     W8, #0x40000
    // C8 80 80 52  MOV     W8, #0x406
    // C9 00 81 52  MOV     W9, #0x806              <-- 0x406
    { "11.0.0-11.0.1 force_dp_mode_c", "force_dp_mode_c", "0x1F010E72C8808052C9008152", 8, 0, movz_cond, mov_w9_0x406_patch, mov_w9_0x406_applied, false, -1, MAKEHOSVERSION(11,0,0), MAKEHOSVERSION(11,0,1)},
};

// NOTE: add system titles that you want to be patched to this table.
// a list of system titles can be found here https://switchbrew.org/wiki/Title_list
constinit PatchEntry patches[] = {
    { "nvservices", 0x0100000000000019, nvservices_patterns },
    { "usb", 0x0100000000000006, usb_patterns },
};

struct EmummcPaths {
    char unk[0x80];
    char nintendo[0x80];
};

void smcAmsGetEmunandConfig(EmummcPaths* out_paths) {
    SecmonArgs args{};
    args.X[0] = 0xF0000404; /* smcAmsGetEmunandConfig */
    args.X[1] = 0; /* EXO_EMUMMC_MMC_NAND*/
    args.X[2] = (u64)out_paths; /* out path */
    svcCallSecureMonitor(&args);
}

auto is_emummc() -> bool {
    EmummcPaths paths{};
    smcAmsGetEmunandConfig(&paths);
    return (paths.unk[0] != '\0') || (paths.nintendo[0] != '\0');
}

void patcher(Handle handle, const u8* data, size_t data_size, u64 addr, std::span<Patterns> patterns) {
    for (auto& p : patterns) {
        // skip if disabled (controller by config.ini)
        if (p.result == PatchResult::DISABLED) {
            continue;
        }

        // skip if version isn't valid
        if (VERSION_SKIP &&
            ((p.min_fw_ver && p.min_fw_ver > FW_VERSION) ||
            (p.max_fw_ver && p.max_fw_ver < FW_VERSION) ||
            (p.min_ams_ver && p.min_ams_ver > AMS_VERSION) ||
            (p.max_ams_ver && p.max_ams_ver < AMS_VERSION))) {
            p.result = PatchResult::SKIPPED;
            continue;
        }

        // skip if already patched
        if (p.result == PatchResult::PATCHED_FILE || p.result == PatchResult::PATCHED_SYSDOCK) {
            continue;
        }

        for (u32 i = 0; i < data_size; i++) {
            if (i + p.byte_pattern.size >= data_size) {
                break;
            }

            // loop through every byte of the pattern data to find a match
            // skipping over any bytes if the value is REGEX_SKIP
            u32 count{};
            while (count < p.byte_pattern.size) {
                if (p.byte_pattern.data[count] != data[i + count] && p.byte_pattern.data[count] != REGEX_SKIP) {
                    break;
                }
                count++;
            }

            // if we have found a matching pattern
            if (count == p.byte_pattern.size) {
                // fetch the instruction
                u32 inst{};
                const auto inst_offset = i + p.inst_offset;
                std::memcpy(&inst, data + inst_offset, sizeof(inst));

                // check if the instruction is the one that we want
                if (p.cond(inst)) {
                    const auto patch_data = p.patch(inst);
                    const auto patch_offset = addr + inst_offset + p.patch_offset;

                    // todo: log failed writes, although this should in theory never fail
                    if (R_FAILED(svcWriteDebugProcessMemory(handle, &patch_data, patch_offset, patch_data.size))) {
                        p.result = PatchResult::FAILED_WRITE;
                    } else {
                        p.result = PatchResult::PATCHED_SYSDOCK;
                    }
                    // move onto next pattern
                    break;
                } else if (p.applied(data + inst_offset + p.patch_offset, inst)) {
                    // patch already applied by sigpatches
                    p.result = PatchResult::PATCHED_FILE;
                    break;
                }
            }
        }
    }
}

auto apply_patch(PatchEntry& patch) -> bool {
    Handle handle{};
    DebugEventInfo event_info{};

    u64 pids[0x50]{};
    s32 process_count{};
    constexpr u64 overlap_size = 0x4f;
    static u8 buffer[READ_BUFFER_SIZE + overlap_size];

    std::memset(buffer, 0, sizeof(buffer));

    // skip if version isn't valid
    if (VERSION_SKIP &&
        ((patch.min_fw_ver && patch.min_fw_ver > FW_VERSION) ||
        (patch.max_fw_ver && patch.max_fw_ver < FW_VERSION))) {
        for (auto& p : patch.patterns) {
            p.result = PatchResult::SKIPPED;
        }
        return true;
    }

    if (R_FAILED(svcGetProcessList(&process_count, pids, 0x50))) {
        return false;
    }

    for (s32 i = 0; i < (process_count - 1); i++) {
        if (R_SUCCEEDED(svcDebugActiveProcess(&handle, pids[i])) &&
            R_SUCCEEDED(svcGetDebugEvent(&event_info, handle)) &&
            patch.title_id == event_info.info.create_process.program_id) {
            MemoryInfo mem_info{};
            u64 addr{};
            u32 page_info{};

            for (;;) {
                if (R_FAILED(svcQueryDebugProcessMemory(&mem_info, &page_info, handle, addr))) {
                    break;
                }
                addr = mem_info.addr + mem_info.size;

                // if addr=0 then we hit the reserved memory section
                if (!addr) {
                    break;
                }
                // skip memory that we don't want
                if (!mem_info.size || (mem_info.perm & Perm_Rx) != Perm_Rx || ((mem_info.type & 0xFF) != MemType_CodeStatic)) {
                    continue;
                }

                for (u64 sz = 0; sz < mem_info.size; sz += READ_BUFFER_SIZE - overlap_size) {
                    const auto actual_size = std::min(READ_BUFFER_SIZE, mem_info.size - sz);
                    if (R_FAILED(svcReadDebugProcessMemory(buffer + overlap_size, handle, mem_info.addr + sz, actual_size))) {
                        break;
                    } else {
                        patcher(handle, buffer, actual_size + overlap_size, mem_info.addr + sz - overlap_size, patch.patterns);
                        if (actual_size >= overlap_size) {
                            memcpy(buffer, buffer + READ_BUFFER_SIZE, overlap_size);
                            std::memset(buffer + overlap_size, 0, READ_BUFFER_SIZE);
                        } else {
                            const auto bytes_to_overlap = std::min<u64>(overlap_size, actual_size);
                            memcpy(buffer, buffer + READ_BUFFER_SIZE + (actual_size - bytes_to_overlap), bytes_to_overlap);
                            std::memset(buffer + bytes_to_overlap, 0, sizeof(buffer) - bytes_to_overlap);
                        }
                    }
                }
            }
            svcCloseHandle(handle);
            return true;
        } else if (handle) {
            svcCloseHandle(handle);
            handle = 0;
        }
    }

    return false;
}

// creates a directory, non-recursive!
auto create_dir(const char* path) -> bool {
    Result rc{};
    FsFileSystem fs{};
    char path_buf[FS_MAX_PATH]{};

    if (R_FAILED(fsOpenSdCardFileSystem(&fs))) {
        return false;
    }

    strcpy(path_buf, path);
    rc = fsFsCreateDirectory(&fs, path_buf);
    fsFsClose(&fs);
    return R_SUCCEEDED(rc);
}

// same as ini_get but writes out the default value instead
auto ini_load_or_write_default(const char* section, const char* key, long _default, const char* path) -> long {
    if (!ini_haskey(section, key, path)) {
        ini_putl(section, key, _default, path);
        return _default;
    } else {
        return ini_getbool(section, key, _default, path);
    }
}

auto patch_result_to_str(PatchResult result) -> const char* {
    switch (result) {
        case PatchResult::NOT_FOUND: return "Unpatched";
        case PatchResult::SKIPPED: return "Skipped";
        case PatchResult::DISABLED: return "Disabled";
        case PatchResult::PATCHED_FILE: return "Patched (file)";
        case PatchResult::PATCHED_SYSDOCK: return "Patched (sys-dock)";
        case PatchResult::FAILED_WRITE: return "Failed (svcWriteDebugProcessMemory)";
    }

    std::unreachable();
}

void num_2_str(char*& s, u16 num) {
    u16 max_v = 1000;
    if (num > 9) {
        while (max_v >= 10) {
            if (num >= max_v) {
                while (max_v != 1) {
                    *s++ = '0' + (num / max_v);
                    num -= (num / max_v) * max_v;
                    max_v /= 10;
                }
            } else {
                max_v /= 10;
            }
        }
    }
    *s++ = '0' + (num); // always add 0 or 1's
}

void ms_2_str(char* s, u32 num) {
    u32 max_v = 100;
    *s++ = '0' + (num / 1000); // add seconds
    num -= (num / 1000) * 1000;
    *s++ = '.';

    while (max_v >= 10) {
        if (num >= max_v) {
            while (max_v != 1) {
                *s++ = '0' + (num / max_v);
                num -= (num / max_v) * max_v;
                max_v /= 10;
            }
        }
        else {
           *s++ = '0'; // append 0
           max_v /= 10;
        }
    }
    *s++ = '0' + (num); // always add 0 or 1's
    *s++ = 's'; // in seconds
}

// eg, 852481 -> 13.2.1
void version_to_str(char* s, u32 ver) {
    for (int i = 0; i < 3; i++) {
        num_2_str(s, (ver >> 16) & 0xFF);
        if (i != 2) {
            *s++ = '.';
        }
        ver <<= 8;
    }
}

// eg, 0xAF66FF99 -> AF66FF99
void hash_to_str(char* s, u32 hash) {
    for (int i = 0; i < 4; i++) {
        const auto num = (hash >> 24) & 0xFF;
        const auto top = (num >> 4) & 0xF;
        const auto bottom = (num >> 0) & 0xF;

        constexpr auto a = [](u8 nib) -> char {
            if (nib >= 0 && nib <= 9) { return '0' + nib; }
            return 'a' + nib - 10;
        };

        *s++ = a(top);
        *s++ = a(bottom);

        hash <<= 8;
    }
}

void keygen_to_str(char* s, u8 keygen) {
    num_2_str(s, keygen);
}

} // namespace

int main(int argc, char* argv[]) {
    constexpr auto ini_path = "/config/sys-dock/config.ini";
    constexpr auto log_path = "/config/sys-dock/log.ini";

    create_dir("/config/");
    create_dir("/config/sys-dock/");
    ini_remove(log_path);

    // load options
    const auto patch_sysmmc = ini_load_or_write_default("options", "patch_sysmmc", 1, ini_path);
    const auto patch_emummc = ini_load_or_write_default("options", "patch_emummc", 1, ini_path);
    const auto enable_logging = ini_load_or_write_default("options", "enable_logging", 1, ini_path);
    VERSION_SKIP = ini_load_or_write_default("options", "version_skip", 1, ini_path);

    // load patch toggles
    for (auto& patch : patches) {
        for (auto& p : patch.patterns) {
            p.enabled = ini_load_or_write_default(patch.name, p.config_key, p.enabled, ini_path);
            if (!p.enabled) {
                p.result = PatchResult::DISABLED;
            }
        }
    }

    for (auto& patch : patches) {
        for (auto& p : patch.patterns) {
            if (p.mutual_exclusivity_group < 0 || !p.enabled) continue;
            for (auto& q : patch.patterns) {
                if (&q == &p) break;
                if (q.mutual_exclusivity_group == p.mutual_exclusivity_group && q.enabled) {
                    p.enabled = false;
                    p.result = PatchResult::DISABLED;
                    break;
                }
            }
        }
    }

    const auto emummc = is_emummc();
    bool enable_patching = true;

    // check if we should patch sysmmc
    if (!patch_sysmmc && !emummc) {
        enable_patching = false;
    }

    // check if we should patch emummc
    if (!patch_emummc && emummc) {
        enable_patching = false;
    }

    // speedtest
    const auto ticks_start = armGetSystemTick();

    if (enable_patching) {
        for (auto& patch : patches) {
            apply_patch(patch);
        }
    }

    const auto ticks_end = armGetSystemTick();
    const auto diff_ns = armTicksToNs(ticks_end) - armTicksToNs(ticks_start);

    if (enable_logging) {
        for (auto& patch : patches) {
            for (auto& p : patch.patterns) {
                if (!enable_patching) {
                    p.result = PatchResult::SKIPPED;
                }
                ini_puts(patch.name, p.patch_name, patch_result_to_str(p.result), log_path);
            }
        }

        // fw of the system
        char fw_version[12]{};
        // atmosphere version
        char ams_version[12]{};
        // lowest fw supported by atmosphere
        char ams_target_version[12]{};
        // ???
        char ams_keygen[3]{};
        // git commit hash
        char ams_hash[9]{};
        // how long it took to patch
        char patch_time[20]{};

        version_to_str(fw_version, FW_VERSION);
        version_to_str(ams_version, AMS_VERSION);
        version_to_str(ams_target_version, AMS_TARGET_VERSION);
        keygen_to_str(ams_keygen, AMS_KEYGEN);
        hash_to_str(ams_hash, AMS_HASH >> 32);
        ms_2_str(patch_time, diff_ns/1000ULL/1000ULL);

        // defined in the Makefile
        #define DATE (DATE_DAY "." DATE_MONTH "." DATE_YEAR " " DATE_HOUR ":" DATE_MIN ":" DATE_SEC)

        ini_puts("stats", "version", VERSION_WITH_HASH, log_path);
        ini_puts("stats", "build_date", DATE, log_path);
        ini_puts("stats", "fw_version", fw_version, log_path);
        ini_puts("stats", "ams_version", ams_version, log_path);
        ini_puts("stats", "ams_target_version", ams_target_version, log_path);
        ini_puts("stats", "ams_keygen", ams_keygen, log_path);
        ini_puts("stats", "ams_hash", ams_hash, log_path);
        ini_putl("stats", "is_emummc", emummc, log_path);
        ini_putl("stats", "heap_size", INNER_HEAP_SIZE, log_path);
        ini_putl("stats", "buffer_size", READ_BUFFER_SIZE, log_path);
        ini_puts("stats", "patch_time", patch_time, log_path);
    }

    // note: sysmod exits here.
    // to keep it running, add a for (;;) loop (remember to sleep!)
    return 0;
}

// libnx stuff goes below
extern "C" {

// Sysmodules should not use applet*.
u32 __nx_applet_type = AppletType_None;

// Sysmodules will normally only want to use one FS session.
u32 __nx_fs_num_sessions = 1;

// Newlib heap configuration function (makes malloc/free work).
void __libnx_initheap(void) {
    static char inner_heap[INNER_HEAP_SIZE];
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

// Service initialization.
void __appInit(void) {
    Result rc{};

    // Open a service manager session.
    if (R_FAILED(rc = smInitialize()))
        fatalThrow(rc);

    // Retrieve the current version of Horizon OS.
    if (R_SUCCEEDED(rc = setsysInitialize())) {
        SetSysFirmwareVersion fw{};
        if (R_SUCCEEDED(rc = setsysGetFirmwareVersion(&fw))) {
            FW_VERSION = MAKEHOSVERSION(fw.major, fw.minor, fw.micro);
            hosversionSet(FW_VERSION);
        }
        setsysExit();
    }

    // get ams version
    if (R_SUCCEEDED(rc = splInitialize())) {
        u64 v{};
        u64 hash{};
        if (R_SUCCEEDED(rc = splGetConfig((SplConfigItem)65000, &v))) {
            AMS_VERSION = (v >> 40) & 0xFFFFFF;
            AMS_KEYGEN = (v >> 32) & 0xFF;
            AMS_TARGET_VERSION = v & 0xFFFFFF;
        }
        if (R_SUCCEEDED(rc = splGetConfig((SplConfigItem)65003, &hash))) {
            AMS_HASH = hash;
        }

        splExit();
    }

    if (R_FAILED(rc = fsInitialize()))
        fatalThrow(rc);

    // Add other services you want to use here.
    if (R_FAILED(rc = pmdmntInitialize()))
        fatalThrow(rc);

    // Close the service manager session.
    smExit();
}

// Service deinitialization.
void __appExit(void) {
    pmdmntExit();
    fsExit();
}
} // extern "C"

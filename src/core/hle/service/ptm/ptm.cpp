// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "common/logging/log.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_gets.h"
#include "core/hle/service/ptm/ptm_play.h"
#include "core/hle/service/ptm/ptm_sets.h"
#include "core/hle/service/ptm/ptm_sysm.h"
#include "core/hle/service/ptm/ptm_u.h"
#include "core/settings.h"

namespace Service {
namespace PTM {

/// Values for the default gamecoin.dat file
static const GameCoin default_game_coin = {0x4F00, 42, 0, 0, 0, 2014, 12, 29};

/// Id of the SharedExtData archive used by the PTM process
static const std::vector<u8> ptm_shared_extdata_id = {0, 0, 0, 0, 0x0B, 0, 0, 0xF0, 0, 0, 0, 0};

void Module::Interface::GetAdapterState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.p_adapter_connected);
}

void Module::Interface::GetShellState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x6, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->shell_open);
}

void Module::Interface::GetBatteryLevel(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x7, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.p_battery_level);
}

void Module::Interface::GetBatteryChargeState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x8, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.p_battery_charging);
}

void Module::Interface::GetPedometerState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->pedometer_is_counting);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetStepHistory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 3, 2);

    u32 hours = rp.Pop<u32>();
    u64 start_time = rp.Pop<u64>();
    size_t steps_buff_size;
    auto& buffer = rp.PopMappedBuffer();
    ASSERT_MSG(sizeof(u16) * hours == buffer.GetSize(),
               "Buffer for steps count has incorrect size");

    // Stub: set zero steps count for every hour
    for (u32 i = 0; i < hours; ++i) {
        const u16_le steps_per_hour = 0;
        buffer.Write(&steps_per_hour, i * sizeof(u16), sizeof(u16));
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_PTM, "(STUBBED) called, from time(raw): 0x%" PRIx64 ", for %u hours",
                start_time, hours);
}

void Module::Interface::GetTotalStepCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xC, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetSoftwareClosedFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void CheckNew3DS(IPC::RequestBuilder& rb) {
    bool is_new_3ds = false;
    if (Service::CFG::GetSystemModelID() == 2 || Service::CFG::GetSystemModelID() == 4 ||
        Service::CFG::GetSystemModelID() == 5) {
        is_new_3ds = true;
    }

    if (is_new_3ds) {
        LOG_CRITICAL(
            Service_PTM,
            "The selected model in 'System' "
            "settings is New 3DS/2DS. Citra does not fully support New 3DS/2DS emulation yet!");
    }

    rb.Push(RESULT_SUCCESS);
    rb.Push(is_new_3ds);

    LOG_WARNING(Service_PTM, "(STUBBED) called isNew3DS = 0x%08x", static_cast<u32>(is_new_3ds));
}

void Module::Interface::ConfigureNew3DSCPU(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x818, 1, 0);
    u8 value = rp.Pop<u8>() & 0xF;
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(Kernel::KernelSetState(static_cast<u32>(Kernel::KernelSetStateType::ConfigureNew3DSCPU),
                                   value, 0, 0));
    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::CheckNew3DS(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x40A, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    Service::PTM::CheckNew3DS(rb);
}

Module::Module() {
    // Open the SharedExtSaveData archive 0xF000000B and create the gamecoin.dat file if it doesn't
    // exist
    FileSys::Path archive_path(ptm_shared_extdata_id);
    auto archive_result =
        Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERR_NOT_FORMATTED) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SharedExtSaveData,
                                   FileSys::ArchiveFormatInfo(), archive_path);
        // Open it again to get a valid archive now that the folder exists
        archive_result =
            Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the PTM SharedExtSaveData archive!");

        FileSys::Path gamecoin_path("/gamecoin.dat");
        Service::FS::CreateFileInArchive(*archive_result, gamecoin_path, sizeof(GameCoin));
        FileSys::Mode open_mode = {};
        open_mode.write_flag.Assign(1);
        // Open the file and write the default gamecoin information
        auto gamecoin_result =
            Service::FS::OpenFileFromArchive(*archive_result, gamecoin_path, open_mode);
        if (gamecoin_result.Succeeded()) {
            auto gamecoin = std::move(gamecoin_result).Unwrap();
            gamecoin->backend->Write(0, sizeof(GameCoin), true,
                                     reinterpret_cast<const u8*>(&default_game_coin));
            gamecoin->backend->Close();
        }
    }
}

Module::Interface::Interface(std::shared_ptr<Module> ptm, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), ptm(std::move(ptm)) {}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto ptm = std::make_shared<Module>();
    std::make_shared<PTM_Gets>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Play>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Sets>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_S>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Sysm>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_U>(ptm)->InstallAsService(service_manager);
}

} // namespace PTM
} // namespace Service

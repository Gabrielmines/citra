// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"
#include "core/hle/service/service.h"
#include "core/memory.h"

namespace Service {
namespace FRD {

static Kernel::SharedPtr<Kernel::Event> event_notification;
static Kernel::SharedPtr<Kernel::Event> completion_event;
static FriendKey my_friend_key = {1, 2, 3ull};
static MyPresence my_presence = {};
static Profile my_profile = {1, 2, 3, 4, 5};
static u8 my_mii[0x60];
static u8 logged_in;
static std::vector<FriendKey> friends;

void GetMyPresence(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 shifted_out_size = cmd_buff[64];
    u32 my_presence_addr = cmd_buff[65];

    ASSERT(shifted_out_size == ((sizeof(MyPresence) << 14) | 2));

    Memory::WriteBlock(my_presence_addr, &my_presence, sizeof(MyPresence));

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetMyPreference(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x6, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);
    rb.Push<u8>(1);
    rb.Push<u8>(1);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetFriendKeyList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 offset = cmd_buff[1];
    u32 frd_count = cmd_buff[2];

    u32 frd_keys_size = cmd_buff[64] >> 14;
    ASSERT_MSG(frd_keys_size == sizeof(FriendKey) * frd_count, "Output buffer size not match");
    u32 frd_key_addr = cmd_buff[65];

    if (offset < friends.size()) {
        for (u32 i = offset; i < frd_count; ++i) {
            if (i >= friends.size())
                break;
            Memory::WriteBlock(frd_key_addr + i * sizeof(FriendKey), &friends[i],
                               sizeof(FriendKey));
        }
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = friends.size();
    LOG_WARNING(Service_FRD, "(STUBBED) called, offset=%d, frd_count=%d, frd_key_addr=0x%08X",
                offset, frd_count, frd_key_addr);
}

void GetFriendProfile(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 count = cmd_buff[1];
    u32 frd_key_addr = cmd_buff[3];
    u32 profiles_addr = cmd_buff[65];
    u32 profiles_size = cmd_buff[64] >> 14;
    ASSERT_MSG(profiles_size == sizeof(Profile) * count, "Output buffer size not match");
    Profile zero_profile = {};
    for (u32 i = 0; i < count; ++i) {
        Memory::WriteBlock(profiles_addr + i * sizeof(Profile), &zero_profile, sizeof(Profile));
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_FRD,
                "(STUBBED) called, count=%d, frd_key_addr=0x%08X, profiles_addr=0x%08X", count,
                frd_key_addr, profiles_addr);
}

void GetFriendAttributeFlags(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 count = cmd_buff[1];
    u32 frd_key_addr = cmd_buff[3];
    u32 attr_flags_addr = cmd_buff[65];

    for (u32 i = 0; i < count; ++i) {
        // TODO:(mailwl) figure out AttributeFlag size and zero all buffer. Assume 1 byte
        Memory::Write8(attr_flags_addr + i, 0);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_FRD,
                "(STUBBED) called, count=%d, frd_key_addr=0x%08X, attr_flags_addr=0x%08X", count,
                frd_key_addr, attr_flags_addr);
}

void HasLoggedIn(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(logged_in);
}

void IsOnline(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(logged_in);
}

void Login(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x3, 0, 2);
    logged_in = 1;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Logout(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x4, 0, 0);
    logged_in = 0;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetMyFriendKey(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    std::memcpy(&cmd_buff[2], &my_friend_key, sizeof(FriendKey));
}

void GetMyScreenName(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    Service::CFG::GetUsername().copy(reinterpret_cast<char16_t*>(&cmd_buff[2]), 11);
}

void GetMyProfile(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    std::memcpy(&cmd_buff[2], &my_profile, sizeof(Profile));
}

void GetMyMii(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    std::memcpy(&cmd_buff[2], &my_mii, 0x60);
}

void GetMyPlayingGame(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xC, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    rb.Push<u32>(0);
    rb.Push<u16>(0);
    rb.Push<u32>(0);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetMyFavoriteGame(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xD, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    rb.Push<u32>(0);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetMyComment(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    Common::UTF8ToUTF16("Citra is awesome!").copy(reinterpret_cast<char16_t*>(&cmd_buff[2]), 33);
    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void IsValidFriendCode(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x26, 2, 0);
    u64 friend_code = rp.Pop<u64>();
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void UnscrambleLocalFriendCode(Service::Interface* self) {
    const size_t scrambled_friend_code_size = 12;
    const size_t friend_code_size = 8;

    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1C, 1, 2);
    const u32 friend_code_count = rp.Pop<u32>();
    size_t in_buffer_size;
    const VAddr scrambled_friend_codes = rp.PopStaticBuffer(&in_buffer_size);
    ASSERT_MSG(in_buffer_size == (friend_code_count * scrambled_friend_code_size),
               "Wrong input buffer size");

    size_t out_buffer_size;
    VAddr unscrambled_friend_codes = rp.PeekStaticBuffer(0, &out_buffer_size);
    ASSERT_MSG(out_buffer_size == (friend_code_count * friend_code_size),
               "Wrong output buffer size");

    for (u32 current = 0; current < friend_code_count; ++current) {
        // TODO(B3N30): Unscramble the codes and compare them against the friend list
        //              Only write 0 if the code isn't in friend list, otherwise write the
        //              unscrambled one
        //
        // Code for unscrambling (should be compared to HW):
        // std::array<u16, 6> scambled_friend_code;
        // Memory::ReadBlock(scrambled_friend_codes+(current*scrambled_friend_code_size),
        // scambled_friend_code.data(), scrambled_friend_code_size); std::array<u16, 4>
        // unscrambled_friend_code; unscrambled_friend_code[0] = scambled_friend_code[0] ^
        // scambled_friend_code[5]; unscrambled_friend_code[1] = scambled_friend_code[1] ^
        // scambled_friend_code[5]; unscrambled_friend_code[2] = scambled_friend_code[2] ^
        // scambled_friend_code[5]; unscrambled_friend_code[3] = scambled_friend_code[3] ^
        // scambled_friend_code[5];

        u64 result = 0ull;
        Memory::WriteBlock(unscrambled_friend_codes + (current * sizeof(result)), &result,
                           sizeof(result));
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(unscrambled_friend_codes, out_buffer_size, 0);
    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void SetClientSdkVersion(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x32, 1, 2);
    u32 version = rp.Pop<u32>();

    self->SetVersion(version);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Init() {
    using namespace Kernel;

    AddService(new FRD_A_Interface);
    AddService(new FRD_U_Interface);

    completion_event = nullptr;
    event_notification = nullptr;

    logged_in = false;
}

void Shutdown() {
    completion_event = nullptr;
    event_notification = nullptr;

    logged_in = false;
}

} // namespace FRD

} // namespace Service

// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"

enum GameType
{
  FZeroAX = 1,
  FZeroAXMonster,
  MarioKartGP,
  MarioKartGP2,
  VirtuaStriker3,
  VirtuaStriker4,
  VirtuaStriker4_2006,
  GekitouProYakyuu,
  KeyOfAvalon,
  FirmwareUpdate,
};

enum MediaType
{
  GDROM = 1,
  NAND,
};

enum MediaBoardType
{
  NANDMaskBoardHDD = 0,
  NANDMaskBoardMask = 1,
  NANDMaskBoardNAND = 2,
  DIMMBoardType3 = 4,
};

enum MediaBoardStatus
{
  Initializing = 0,
  CheckingNetwork = 1,
  SystemDisc = 2,
  TestingGameProgram = 3,
  LoadingGameProgram = 4,
  LoadedGameProgram = 5,
  Error = 6,
};

enum InquiryType
{
  Version1 = 0x21484100,
  Version2 = 0x29484100,
};

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

struct MediaBoardRanges
{
  u32 start;
  u32 end;
  u8* buffer;
  size_t buffer_size;
};

namespace AMMediaboard
{

enum class AMMBDICommand : u16
{
  Inquiry = 0x12,
  Read = 0xA8,
  Write = 0xAA,
  Execute = 0xAB,
};

enum class AMMBCommand : u16
{
  Unknown_000 = 0x000,
  GetDIMMSize = 0x001,

  GetMediaBoardStatus = 0x100,
  GetSegaBootVersion = 0x101,
  GetSystemFlags = 0x102,
  GetMediaBoardSerial = 0x103,
  Unknown_104 = 0x104,

  NetworkReInit = 0x204,

  TestHardware = 0x301,

  // Network used by Mario Kart GPs
  Accept = 0x401,
  Bind = 0x402,
  Closesocket = 0x403,
  Connect = 0x404,
  GetIPbyDNS = 0x405,
  InetAddr = 0x406,
  Ioctl = 0x407,  // Unused
  Listen = 0x408,
  Recv = 0x409,
  Send = 0x40A,
  Socket = 0x40B,
  Select = 0x40C,
  Shutdown = 0x40D,  // Unused
  SetSockOpt = 0x40E,
  GetSockOpt = 0x40F,  // Unused
  SetTimeOuts = 0x410,
  GetLastError = 0x411,
  RouteAdd = 0x412,     // Unused
  RouteDelete = 0x413,  // Unused
  GetParambyDHCPExec = 0x414,
  ModifyMyIPaddr = 0x415,
  Recvfrom = 0x416,       // Unused
  Sendto = 0x417,         // Unused
  RecvDimmImage = 0x418,  // Unused
  SendDimmImage = 0x419,  // Unused

  // Network used by F-Zero AX
  InitLink = 0x601,
  Unknown_605 = 0x605,
  SetupLink = 0x606,
  SearchDevices = 0x607,
  Unknown_608 = 0x608,
  Unknown_614 = 0x614,

  // All.Net used by Virtua Striker 4 2006 (Japan)
  AllNetInit = 0x700,
  Unknown_701 = 0x701,
  Unknown_702 = 0x702,
  Unknown_703 = 0x703,
  Unknown_704 = 0x704,

  // NETDIMM Commands
  Unknown_001 = 0x001,
  GetNetworkFirmVersion = 0x101,
  Unknown_103 = 0x103,
};

enum MediaBoardAddress : u32
{
  MediaBoardStatus1 = 0x80000000,
  MediaBoardStatus2 = 0x80000020,
  MediaBoardStatus3 = 0x80000040,

  FirmwareStatus1 = 0x80000120,
  FirmwareStatus2 = 0x80000140,

  BackupMemory = 0x000006A0,

  DIMMMemory = 0x1F000000,
  DIMMMemory2 = 0xFF000000,

  DIMMExtraSettings = 0x1FFEFFE0,

  AllNetSettings = 0x1EFF8000,

  NetworkControl = 0xFFFF0000,

  DIMMCommandVersion1 = 0x1F900000,
  DIMMCommandVersion2 = 0x84000000,
  DIMMCommandVersion2_2 = 0x89000000,
  DIMMCommandExecute1 = 0x84000040,
  DIMMCommandExecute2 = 0x88000000,

  NetworkCommandAddress1 = 0x1F800200,
  NetworkCommandAddress2 = 0x89040200,

  NetworkBufferAddress1 = 0x1FA00000,
  NetworkBufferAddress2 = 0x1FD00000,
  NetworkBufferAddress3 = 0x89100000,
  NetworkBufferAddress4 = 0x89180000,
  NetworkBufferAddress5 = 0x1FB00000,

  AllNetBuffer = 0x89010000,

  FirmwareAddress = 0x84800000,

  FirmwareMagicWrite1 = 0x00600000,
  FirmwareMagicWrite2 = 0x00700000,
};

void Init();
void FirmwareMap(bool on);
u8* InitDIMM(u32 size);
void InitKeys(u32 key_a, u32 key_b, u32 key_c);
u32 ExecuteCommand(std::array<u32, 3>& dicmd_buf, u32* diimm_buf, u32 address, u32 length);
u32 GetGameType();
u32 GetMediaType();
bool GetTestMenu();
void Shutdown();

};  // namespace AMMediaboard

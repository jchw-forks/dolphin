// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <optional>

#include <SFML/Network.hpp>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"

namespace AMMediaboard
{

// Mario Kart GP2 has a complete list of them
// but in japanese
// They somewhat match WSA errors codes
enum SocketStatusCodes
{
  SSC_E_4 = -4,  // Failure (abnormal argument)
  SSC_E_3 = -3,  // Success (unsupported command)
  SSC_E_2 =
      -2,  // Failure (failed to send, abnormal argument, or communication condition violation)
  SSC_E_1 = -1,  // Failure (error termination)

  SSC_EINTR = 4,    // An interrupt occurred before data reception was completed
  SSC_EBADF = 9,    // Invalid descriptor
  SSC_EAGAIN = 11,  // Send operation was blocked on a non-blocking mode socket
  SSC_EACCES = 13,  // The socket does not support broadcast addresses, but the destination address
                    // is a broadcast address
  SSC_EFAULT =
      14,  // The name argument specifies a location other than an address used by the process.
  SSC_E_23 = 23,     // System file table is full.
  SSC_AEMFILE = 24,  // Process descriptor table is full.
  SSC_EMSGSIZE =
      36,  // Socket tried to send message without splitting, but message size is too large.
  SSC_EAFNOSUPPORT = 47,   // Address prohibited for use on this socket.
  SSC_EADDRINUSE = 48,     // Address already in use.
  SSC_EADDRNOTAVAIL = 49,  // Prohibited address.
  SSC_E_50 = 50,           // Non-socket descriptor.
  SSC_ENETUNREACH = 51,    // Cannot access specified network.
  SSC_ENOBUFS = 55,        // Insufficient buffer
  SSC_EISCONN = 56,        // Already connected socket
  SSC_ENOTCONN = 57,       // No connection for connection-type socket
  SSC_ETIMEDOUT = 60,      // Timeout
  SSC_ECONNREFUSED = 61,   // Connection request forcibly rejected
  SSC_EHOSTUNREACH = 65,   // Remote host cannot be reached
  SSC_EHOSTDOWN = 67,      // Remote host is down
  SSC_EWOULDBLOCK = 68,    // Socket is in non-blocking mode and connection has not been completed
  SSC_E_69 = 69,  // Socket is in non-blocking mode and a previously issued Connect command has not
                  // been completed
  SSC_SUCCESS = 70,
};

enum class AddressFamily : u16
{
  Internet = 2,
};

enum class SocketType : u16
{
  Stream = 1,
  Datagram = 2,
};

enum class SocketOptionLevel
{
  Socket = 1,
};

enum class SocketOption
{
  RecvTimeout = 20,
  SendTimeout = 21,
};

enum class IoctlCmd
{
  FIONonBlocking = 0x5421,
};

struct SocketAddress
{
  Common::BigEndianValue<u16> AddressFamily;
};

struct InternetSocketAddress : public SocketAddress
{
  Common::BigEndianValue<u16> Port;
  Common::BigEndianValue<u32> Address;
  u8 Padding[8]{};
};

struct TimeVal
{
  Common::BigEndianValue<u64> Seconds;
  Common::BigEndianValue<u32> Microseconds;
};

class NetDIMM
{
public:
  static constexpr u32 MAX_SOCKETS = 64;
  static constexpr u32 INVALID_FD = static_cast<u32>(-1);

  NetDIMM();
  ~NetDIMM();

  u32 CreateSocket(AddressFamily af, SocketType type);
  int CloseSocket(u32 fd);
  int Bind(u32 fd, const InternetSocketAddress* addr, u32 len);
  int Listen(u32 fd, int backlog);
  u32 Accept(u32 fd, InternetSocketAddress* addr, Common::BigEndianValue<u32>* len);
  int Connect(u32 fd, const InternetSocketAddress* addr, u32 len);

  int Send(u32 fd, const char* buffer, int len, int flags);
  int Recv(u32 fd, char* buffer, int len, int flags);

  int SetSockOpt(u32 fd, SocketOptionLevel level, SocketOption optname, const void* optval,
                 int optlen);
  int IoctlSocket(u32 fd, IoctlCmd cmd, u_long* argp);

  void SetTimeout(u32 timeout);

  int Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
             const TimeVal* timeout);

  int GetStatusCode() const { return m_status_code; }

  bool IsValidSocket(u32 fd) const;

private:
  struct SocketEntry
  {
    std::unique_ptr<sf::Socket> socket;

    std::optional<sf::Time> send_timeout;
    std::optional<sf::Time> recv_timeout;

    bool is_blocking = true;
  };

  u32 AllocateFD();

  SocketEntry* GetSocket(u32 fd);
  const SocketEntry* GetSocket(u32 fd) const;

  bool ConvertSFMLError(sf::Socket::Status status, int& out_error) const;

  std::array<SocketEntry, MAX_SOCKETS> m_sockets;
  u32 m_timeout;
  int m_status_code;
};

}  // namespace AMMediaboard

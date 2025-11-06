// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/NetDIMM.h"

#include <SFML/Network/TcpSocket.hpp>
#include <SFML/System/Time.hpp>
#include <cstring>
#include <memory>

#include "Common/Logging/Log.h"

namespace AMMediaboard
{

NetDIMM::NetDIMM() : m_timeout{20000}, m_status_code{0}
{
}

NetDIMM::~NetDIMM() = default;

u32 NetDIMM::AllocateFD()
{
  for (u32 i = 1; i < MAX_SOCKETS; ++i)
    if (!m_sockets[i].socket)
      return i;
  return INVALID_FD;
}

NetDIMM::SocketEntry* NetDIMM::GetSocket(u32 fd)
{
  if (fd >= MAX_SOCKETS)
    return nullptr;

  if (!m_sockets[fd].socket)
    return nullptr;

  return &m_sockets[fd];
}

const NetDIMM::SocketEntry* NetDIMM::GetSocket(u32 fd) const
{
  if (fd >= MAX_SOCKETS)
    return nullptr;

  if (!m_sockets[fd].socket)
    return nullptr;

  return &m_sockets[fd];
}

bool NetDIMM::IsValidSocket(u32 fd) const
{
  return GetSocket(fd) != nullptr;
}

bool NetDIMM::ConvertSFMLError(sf::Socket::Status status, int& out_error) const
{
  switch (status)
  {
  case sf::Socket::Status::Done:
    out_error = 0;
    return true;
  case sf::Socket::Status::NotReady:
    out_error = SSC_EWOULDBLOCK;
    return false;
  case sf::Socket::Status::Partial:
    out_error = 0;
    return true;  // Partial is still a success for send/recv
  case sf::Socket::Status::Disconnected:
    out_error = 0;  // Connection closed gracefully
    return false;
  case sf::Socket::Status::Error:
  default:
    out_error = SSC_E_1;
    return false;
  }
}

u32 NetDIMM::CreateSocket(AddressFamily af, SocketType type)
{
  u32 fd = AllocateFD();
  if (fd == INVALID_FD || af != AddressFamily::Internet)
  {
    m_status_code = SSC_E_1;
    return INVALID_FD;
  }
  auto& entry = m_sockets[fd];
  switch (type)
  {
  case SocketType::Stream:
    entry.socket = std::make_unique<sf::TcpSocket>();
    break;
  case SocketType::Datagram:
    entry.socket = std::make_unique<sf::UdpSocket>();
    break;
  default:
    m_status_code = SSC_E_1;
    return INVALID_FD;
  }
  m_status_code = 0;
  return fd;
}

int NetDIMM::CloseSocket(u32 fd)
{
  auto* socket = GetSocket(fd);
  if (!socket)
    return m_status_code = SSC_E_1;

  if (auto tcp_socket = dynamic_cast<sf::TcpSocket*>(socket->socket.get()))
    tcp_socket->disconnect();
  else if (auto tcp_listener = dynamic_cast<sf::TcpListener*>(socket->socket.get()))
    tcp_listener->close();
  else if (auto udp_socket = dynamic_cast<sf::UdpSocket*>(socket->socket.get()))
    udp_socket->unbind();

  socket->socket.reset();
  m_status_code = SSC_SUCCESS;
  return 0;
}

int NetDIMM::Bind(u32 fd, const InternetSocketAddress* addr, u32 len)
{
  auto* socket = GetSocket(fd);
  if (!socket)
  {
    m_status_code = SSC_E_1;
    return SSC_E_1;
  }

  sf::IpAddress bind_addr = sf::IpAddress::Any;
  u16 port = addr->Port;

  NOTICE_LOG_FMT(AMMEDIABOARD_NET, "SocketManager: Bind fd={} to port={}", fd, port);

  // Convert TCP socket to a listener
  if (dynamic_cast<sf::TcpSocket*>(socket->socket.get()))
    socket->socket = std::make_unique<sf::TcpListener>();

  if (auto tcp_listener = dynamic_cast<sf::TcpListener*>(socket->socket.get()))
  {
    tcp_listener->close();
    if (tcp_listener->listen(port, bind_addr) != sf::Socket::Status::Done)
    {
      m_status_code = SSC_E_1;
      return SSC_E_1;
    }
    tcp_listener->setBlocking(socket->is_blocking);
  }
  else if (auto udp_socket = dynamic_cast<sf::UdpSocket*>(socket->socket.get()))
  {
    udp_socket->unbind();
    if (udp_socket->bind(port, bind_addr) != sf::Socket::Status::Done)
    {
      m_status_code = SSC_E_1;
      return SSC_E_1;
    }
    udp_socket->setBlocking(socket->is_blocking);
  }
  else
  {
    m_status_code = SSC_E_1;
    return SSC_E_1;
  }
  m_status_code = SSC_SUCCESS;
  return 0;
}

int NetDIMM::Listen(u32 fd, int backlog)
{
  auto* socket = GetSocket(fd);
  if (!dynamic_cast<sf::TcpListener*>(socket->socket.get()))
    return m_status_code = SSC_E_1;
  // This is a no-op: we already called listen.
  m_status_code = 0;
  return 0;
}

u32 NetDIMM::Accept(u32 fd, InternetSocketAddress* addr, Common::BigEndianValue<u32>* len)
{
  auto* socket = GetSocket(fd);
  if (!socket)
  {
    m_status_code = SSC_E_1;
    return INVALID_FD;
  }

  auto tcp_listener = dynamic_cast<sf::TcpListener*>(socket->socket.get());
  if (!tcp_listener)
  {
    m_status_code = SSC_E_1;
    return INVALID_FD;
  }

  // Allocate new FD for accepted socket
  u32 new_fd = AllocateFD();
  if (new_fd == INVALID_FD)
  {
    m_status_code = SSC_E_1;
    return INVALID_FD;
  }

  auto& new_entry = m_sockets[new_fd];
  auto new_socket = std::make_unique<sf::TcpSocket>();

  tcp_listener->setBlocking(false);
  sf::Socket::Status status = tcp_listener->accept(*new_socket.get());
  tcp_listener->setBlocking(socket->is_blocking);

  if (status != sf::Socket::Status::Done)
  {
    new_entry.socket.reset();
    if (status == sf::Socket::Status::NotReady)
      m_status_code = SSC_EWOULDBLOCK;
    else
      m_status_code = SSC_E_1;
    return INVALID_FD;
  }
  m_status_code = SSC_SUCCESS;

  auto remote_addr = new_socket->getRemoteAddress();
  auto remote_port = new_socket->getRemotePort();

  if (addr && len)
  {
    *addr = {};
    addr->AddressFamily = static_cast<u16>(AddressFamily::Internet);
    addr->Port = remote_port;
    addr->Address = remote_addr->toInteger();
    *len = sizeof(InternetSocketAddress);
  }

  new_entry.socket = std::move(new_socket);
  m_status_code = 0;
  return new_fd;
}

int NetDIMM::Connect(u32 fd, const InternetSocketAddress* addr, u32 len)
{
  auto* socket = GetSocket(fd);
  if (!socket || len < sizeof(InternetSocketAddress))
  {
    return m_status_code = SSC_E_1;
  }
  auto tcp_socket = dynamic_cast<sf::TcpSocket*>(socket->socket.get());
  if (!tcp_socket)
  {
    return m_status_code = SSC_E_1;
  }

  const auto* addr_in = reinterpret_cast<const InternetSocketAddress*>(addr);
  u32 ip_int = addr_in->Address;
  u16 port = addr_in->Port;

  sf::IpAddress connect_addr(ip_int);

  // TODO: Probably not good to hardcode these.

  // All.Net Connect IP
  if (connect_addr == sf::IpAddress::resolve("192.168.150.16"))
  {
    connect_addr = sf::IpAddress::LocalHost;
  }
  // CyCraft Connect IP
  else if (connect_addr == sf::IpAddress::resolve("192.168.11.111"))
  {
    connect_addr = sf::IpAddress::LocalHost;
  }
  // NAMCO Camera IPs (192.168.29.104-108)
  else if (connect_addr >= sf::IpAddress::resolve("192.168.29.104") &&
           connect_addr < sf::IpAddress::resolve("192.168.29.108"))
  {
    connect_addr = sf::IpAddress::LocalHost;
  }
  // Key of Avalon Client
  else if (connect_addr == sf::IpAddress::resolve("192.168.13.1"))
  {
    connect_addr = sf::IpAddress::resolve("10.0.0.45").value();
  }

  NOTICE_LOG_FMT(AMMEDIABOARD_NET, "SocketManager: Connect fd={} to {}:{}", fd,
                 connect_addr.toString(), port);

  tcp_socket->setBlocking(true);
  sf::Socket::Status status = tcp_socket->connect(connect_addr, port, sf::milliseconds(m_timeout));
  tcp_socket->setBlocking(socket->is_blocking);

  m_status_code = 0;
  if (status != sf::Socket::Status::Done)
  {
    if (status == sf::Socket::Status::NotReady && !socket->is_blocking)
      m_status_code = SSC_EWOULDBLOCK;
    else
      m_status_code = SSC_E_1;
    return SSC_E_1;
  }
  m_status_code = SSC_SUCCESS;
  return 0;
}

int NetDIMM::Send(u32 fd, const char* buffer, int len, int flags)
{
  auto* socket = GetSocket(fd);
  if (!socket)
    return m_status_code = SSC_E_1;

  if (auto tcp_socket = dynamic_cast<sf::TcpSocket*>(socket->socket.get()))
  {
    std::size_t sent = 0;
    sf::Socket::Status status = tcp_socket->send(buffer, len, sent);

    if (status == sf::Socket::Status::Done || status == sf::Socket::Status::Partial)
    {
      m_status_code = 0;
      return static_cast<int>(sent);
    }
    else if (status == sf::Socket::Status::NotReady)
    {
      m_status_code = SSC_EWOULDBLOCK;
      return SSC_E_1;
    }
  }
  m_status_code = SSC_E_1;
  return SSC_E_1;
}

int NetDIMM::Recv(u32 fd, char* buffer, int len, int flags)
{
  auto* socket = GetSocket(fd);
  if (!socket)
  {
    m_status_code = SSC_E_1;
    return SSC_E_1;
  }

  if (auto tcp_socket = dynamic_cast<sf::TcpSocket*>(socket->socket.get()))
  {
    std::size_t received = 0;
    sf::Socket::Status status = tcp_socket->receive(buffer, len, received);

    if (status == sf::Socket::Status::Done || status == sf::Socket::Status::Partial)
    {
      m_status_code = 0;
      return static_cast<int>(received);
    }
    else if (status == sf::Socket::Status::NotReady)
    {
      m_status_code = SSC_EWOULDBLOCK;
      return SSC_E_1;
    }
    else if (status == sf::Socket::Status::Disconnected)
    {
      m_status_code = 0;
      return 0;
    }
  }

  m_status_code = SSC_E_1;
  return SSC_E_1;
}

int NetDIMM::SetSockOpt(u32 fd, SocketOptionLevel level, SocketOption optname, const void* optval,
                        int optlen)
{
  auto* socket = GetSocket(fd);
  if (!socket)
  {
    m_status_code = SSC_E_1;
    return SSC_E_1;
  }
  if (level == SocketOptionLevel::Socket)
  {
    if (optname == SocketOption::RecvTimeout && optlen >= static_cast<int>(sizeof(int)))
    {
      int timeout_ms = *reinterpret_cast<const int*>(optval);
      socket->recv_timeout = sf::milliseconds(timeout_ms);
      m_status_code = 0;
      return 0;
    }
    else if (optname == SocketOption::SendTimeout && optlen >= static_cast<int>(sizeof(int)))
    {
      int timeout_ms = *reinterpret_cast<const int*>(optval);
      socket->send_timeout = sf::milliseconds(timeout_ms);
      m_status_code = 0;
      return 0;
    }
  }

  // Ignore other socket options for now
  m_status_code = 0;
  return 0;
}

int NetDIMM::IoctlSocket(u32 fd, IoctlCmd cmd, u_long* argp)
{
  auto* socket = GetSocket(fd);
  if (!socket || !argp)
  {
    m_status_code = SSC_E_1;
    return SSC_E_1;
  }

  if (cmd == IoctlCmd::FIONonBlocking)
  {
    bool blocking = (*argp == 0);
    socket->is_blocking = blocking;
    socket->socket->setBlocking(blocking);
    m_status_code = 0;
    return 0;
  }

  m_status_code = 0;
  return 0;
}

void NetDIMM::SetTimeout(u32 timeout)
{
  m_timeout = timeout;
}

int NetDIMM::Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
                    const TimeVal* timeout)
{
  sf::SocketSelector selector;
  std::vector<u32> read_fds, write_fds, except_fds;

  // Collect sockets to monitor
  for (u32 fd = 0; fd < u32(nfds); ++fd)
  {
    auto* socket = GetSocket(fd);
    if (!socket)
      continue;

    bool monitor_read = readfds && FD_ISSET(fd, readfds);
    bool monitor_write = writefds && FD_ISSET(fd, writefds);
    bool monitor_except = exceptfds && FD_ISSET(fd, exceptfds);

    if (monitor_read || monitor_except)
    {
      selector.add(*socket->socket);
      if (monitor_read)
        read_fds.push_back(fd);
      if (monitor_except)
        except_fds.push_back(fd);
    }

    // For write, TCP sockets are usually always ready unless buffer is full
    if (monitor_write)
    {
      write_fds.push_back(fd);
    }
  }

  // Clear the fd_sets
  if (readfds)
    FD_ZERO(readfds);
  if (writefds)
    FD_ZERO(writefds);
  if (exceptfds)
    FD_ZERO(exceptfds);

  // Wait with timeout (0 means poll, null means wait forever)
  bool ready = false;
  sf::Time sfml_timeout;
  if (timeout)
    if (timeout->Seconds != 0 || timeout->Microseconds != 0)
      sfml_timeout = sf::seconds(timeout->Seconds) + sf::microseconds(timeout->Microseconds);
    else
      sfml_timeout = sf::microseconds(1);
  else
    sfml_timeout = sf::Time::Zero;
  ready = selector.wait(sfml_timeout);

  int ready_count = 0;

  if (ready)
  {
    for (u32 fd : read_fds)
    {
      auto* socket = GetSocket(fd);
      if (!socket)
        continue;

      bool is_ready = false;
      if (socket->socket && selector.isReady(*socket->socket))
        is_ready = true;

      if (is_ready && readfds)
      {
        FD_SET(fd, readfds);
        ready_count++;
      }
    }
  }

  // Always consider write "ready"
  for (u32 fd : write_fds)
  {
    if (writefds)
    {
      FD_SET(fd, writefds);
      ready_count++;
    }
  }

  m_status_code = 0;
  return ready_count;
}

}  // namespace AMMediaboard

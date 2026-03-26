#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpUdpChatExample");

// Custom TCP client application
class TcpChatClient : public Application
{
public:
  TcpChatClient () {}
  virtual ~TcpChatClient () {}

  void Setup (Ipv4Address address, uint16_t port, std::string message)
  {
    m_address = address;
    m_port = port;
    m_message = message;
  }

private:
  virtual void StartApplication ()
  {
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (m_address, m_port));

    Ptr<Packet> packet = Create<Packet> ((uint8_t*) m_message.c_str (), m_message.size ());
    m_socket->Send (packet);
    NS_LOG_INFO ("TCP client sent: " << m_message);
  }

  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_address;
  uint16_t m_port;
  std::string m_message;
};

// Custom TCP server application
class TcpChatServer : public Application
{
public:
  TcpChatServer () {}
  virtual ~TcpChatServer () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication ()
  {
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->Listen ();
    m_socket->SetRecvCallback (MakeCallback (&TcpChatServer::HandleRead, this));
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        uint8_t *buffer = new uint8_t[packet->GetSize ()];
        packet->CopyData (buffer, packet->GetSize ());
        std::string data ((char*)buffer, packet->GetSize ());
        NS_LOG_INFO ("TCP server received: " << data);
        delete[] buffer;
      }
  }

  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // TCP Chat
  uint16_t tcpPort = 9;
  Ptr<TcpChatServer> tcpServerApp = CreateObject<TcpChatServer> ();
  tcpServerApp->Setup (tcpPort);
  nodes.Get (1)->AddApplication (tcpServerApp);
  tcpServerApp->SetStartTime (Seconds (1.0));
  tcpServerApp->SetStopTime (Seconds (10.0));

  Ptr<TcpChatClient> tcpClientApp = CreateObject<TcpChatClient> ();
  tcpClientApp->Setup (interfaces.GetAddress (1), tcpPort, "Hello via TCP");
  nodes.Get (0)->AddApplication (tcpClientApp);
  tcpClientApp->SetStartTime (Seconds (2.0));
  tcpClientApp->SetStopTime (Seconds (9.0));

  // UDP Chat
  uint16_t udpPort = 10;
  UdpEchoServerHelper udpServer (udpPort);
  ApplicationContainer udpAppServer = udpServer.Install (nodes.Get (1));
  udpAppServer.Start (Seconds (1.0));
  udpAppServer.Stop (Seconds (10.0));

  UdpEchoClientHelper udpClient (interfaces.GetAddress (1), udpPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (1));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer udpAppClient = udpClient.Install (nodes.Get (0));
  udpAppClient.Start (Seconds (3.0));
  udpAppClient.Stop (Seconds (9.0));

  LogComponentEnable ("TcpUdpChatExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

//
// tcpproxy_server.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2007 Arash Partow (http://www.partow.net)
// URL: http://www.partow.net/programming/tcpproxy/index.html
//
// Distributed under the Boost Software License, Version 1.0.
//
//
// Description
// ~~~~~~~~~~~
// The  objective of  the TCP  proxy server  is to  act  as  an
// intermediary  in order  to 'forward'  TCP based  connections
// from external clients onto a singular remote server.
//
// The communication flow in  the direction from the  client to
// the proxy to the server is called the upstream flow, and the
// communication flow in the  direction from the server  to the
// proxy  to  the  client   is  called  the  downstream   flow.
// Furthermore  the   up  and   down  stream   connections  are
// consolidated into a single concept known as a bridge.
//
// In the event  either the downstream  or upstream end  points
// disconnect, the proxy server will proceed to disconnect  the
// other  end  point  and  eventually  destroy  the  associated
// bridge.
//
// The following is a flow and structural diagram depicting the
// various elements  (proxy, server  and client)  and how  they
// connect and interact with each other.

//
//                                    ---> upstream --->           +---------------+
//                                                     +---->------>               |
//                               +-----------+         |           | Remote Server |
//                     +--------->          [x]--->----+  +---<---[x]              |
//                     |         | TCP Proxy |            |        +---------------+
// +-----------+       |  +--<--[x] Server   <-----<------+
// |          [x]--->--+  |      +-----------+
// |  Client   |          |
// |           <-----<----+
// +-----------+
//                <--- downstream <---
//
//


#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/asio/ip/address.hpp>

#include <fstream>
#include <thread>
#include <chrono>
#include <time.h>

#include <redisclient/redissyncclient.h>

#define DEBUG

#define OUTGOING_FNAME  "outgoing.csv"
#define INCOMING_FNAME  "incoming.csv"

enum { max_data_length = 8192 }; //8KB

class RedisLogger{
   public:
      RedisLogger(boost::asio::io_service& ioService):redis(ioService),ioSer(ioService)
      {
         boost::asio::ip::address address = boost::asio::ip::address::from_string("127.0.0.1");
         const unsigned short port = 6379;
         endpoint = boost::asio::ip::tcp::endpoint(address, port);

         redisKey = "unique-redis-key-example";
         redisValue = "unique-redis-value";
         
         boost::system::error_code ec;
         redis.connect(endpoint, ec);

         if(ec)
         {
            std::cerr << "Can't connect to redis: " << ec.message() << std::endl;
         }
         redis.command("FLUSHALL", {});                                                //NOTE : Comment this, if you dont want to clear all redis data
      }
      std::string getValue(std::string key)
      {
         redisclient::RedisValue result;
         result = redis.command("GET", {key});

         if( result.isOk() )
         {
            std::cout << "GET: " << result.toString() << "\n";
            return result.toString();
         }
         return "";
      }
      bool setValue(std::string key, std::string value)
      {
         redisclient::RedisValue result;
         result = redis.command("SET", {key, value});

         if( result.isError() )
         {
            std::cerr << "SET error: " << result.toString() << "\n";
            return false;
         }
         return true;
      }
      bool logIP(std::string IP, int dataLen)
      {
         // get total data len
         int totalByte=0;
         std::string strTB = getValue(IP);
         if (strTB != "")
         {
            totalByte = std::stoi(strTB);
         }
         totalByte += dataLen;
         std::cout << "Total Byte of IP : " << IP << " = " << totalByte << std::endl;
         return setValue(IP, std::to_string(totalByte));
      }
   private:
      redisclient::RedisSyncClient redis;
      boost::asio::ip::tcp::endpoint endpoint;
      boost::asio::io_service& ioSer;
      
      std::string redisKey;
      std::string redisValue;
};

class DataLogger{
   public:
      DataLogger(boost::asio::io_service& ios):redisLog(ios)
      {
         initDataLogger();
      }
      ~DataLogger()
      {
         #ifdef DEBUG
            std::cout << "Saving & Clossing log ..." << std::endl;
         #endif
         ofl.flush();
         ifl.flush();
         ofl.close();
         ifl.close();
      }
      std::string getCurrentTime()
      {
         time_t rawtime;
         struct tm * timeinfo;
         char buffer [80];

         time (&rawtime);
         timeinfo = localtime (&rawtime);
         strftime(buffer, 80, "%x %X", timeinfo);

         return std::string(buffer);
      }
      void writeIncomingLog(std::string data, std::string ip, uint16_t port)
      {
         #ifdef DEBUG
            std::cout << "Saving Incoming Data From : "<< ip << ":" << port << ", data : " << data << std::endl;
         #endif
         int len = data.length();
         redisLog.logIP(ip, len);
         ifl << getCurrentTime() << ";" << ip << ":" << port << ";" << len << ";\"" << data << "\"" << std::endl;
      }
      void writeOutgoingLog(std::string data, std::string ip, uint16_t port)
      {
         #ifdef DEBUG
            std::cout << "Saving Outgoing Data For : "<< ip << ":" << port << ", data : " << data << std::endl;
         #endif
         ofl << getCurrentTime() << ";" << ip << ":" << port << ";" << data.length() << ";\"" << data << "\"" << std::endl;
      }
      void thdAutoSave(void)
      {
         #ifdef DEBUG
            std::cout << "Running thread for saving data..." << std::endl;
         #endif
         while (1)
         {
            ofl.flush();
            ifl.flush();
            std::this_thread::sleep_for(std::chrono::seconds(1));
         }
      }
      void initDataLogger(void)
      {
         #ifdef DEBUG
            std::cout << "Opening file for data logger ..." << std::endl;
         #endif

         ofl.open(OUTGOING_FNAME);
         ifl.open(INCOMING_FNAME);

         ThdAutoSave = std::thread(&DataLogger::thdAutoSave, this);                               // start thread for saving incoming & outgoing data

      }
      void addOutgoingLog(std::string data, std::string ip, uint16_t port)
      {
         // delete logOutgoing;
         logOutgoing = new std::thread(&DataLogger::writeOutgoingLog, this, data, ip, port);
         logOutgoing->join();
      }
      void addIncomingLog(std::string data, std::string ip, uint16_t port)
      {
         // delete logIncoming;
         logIncoming = new std::thread(&DataLogger::writeIncomingLog, this, data, ip, port);
         logIncoming->join();
      }
   private:
      std::ofstream ofl, ifl;
      std::thread ThdAutoSave;
      std::thread *logOutgoing, *logIncoming;
      RedisLogger redisLog;

};

namespace tcp_proxy
{
   namespace ip = boost::asio::ip;

   class bridge : public boost::enable_shared_from_this<bridge>
   {
   public:
      typedef ip::tcp::socket socket_type;
      typedef boost::shared_ptr<bridge> ptr_type;

      bridge(boost::asio::io_service& ios)
      : downstream_socket_(ios),
        upstream_socket_  (ios),
        dLogger(ios)
      {}

      socket_type& downstream_socket()
      {
         // Client socket
         return downstream_socket_;
      }

      socket_type& upstream_socket()
      {
         // Remote server socket
         return upstream_socket_;
      }

      void start(const std::string& upstream_host, unsigned short upstream_port)
      {
         // Attempt connection to remote server (upstream side)
         upstream_socket_.async_connect(
              ip::tcp::endpoint(
                   boost::asio::ip::address::from_string(upstream_host),
                   upstream_port),
               boost::bind(&bridge::handle_upstream_connect,
                    shared_from_this(),
                    boost::asio::placeholders::error));
      }

      void handle_upstream_connect(const boost::system::error_code& error)
      {
         if (!error)
         {
            // Setup async read from remote server (upstream)
            upstream_socket_.async_read_some(
                 boost::asio::buffer(upstream_data_,max_data_length),
                 boost::bind(&bridge::handle_upstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));

            // Setup async read from client (downstream)
            downstream_socket_.async_read_some(
                 boost::asio::buffer(downstream_data_,max_data_length),
                 boost::bind(&bridge::handle_downstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
         }
         else
            close();
      }

   private:

      /*
         Section A: Remote Server --> Proxy --> Client
         Process data recieved from remote sever then send to client.
      */

      // Read from remote server complete, now send data to client
      void handle_upstream_read(const boost::system::error_code& error,
                                const size_t& bytes_transferred)
      {
         if (!error)
         {
            // std::cout << "Data to write to client :" << upstream_data_ << ", len : " << bytes_transferred << std::endl;
            // std::cout << "downstream socket : " << downstream_socket_.remote_endpoint().address() << ", port : " << downstream_socket_.remote_endpoint().port() << std::endl;
            std::string outgoing = std::string((char*)upstream_data_, bytes_transferred);
            dLogger.addOutgoingLog(outgoing, downstream_socket().remote_endpoint().address().to_string(), downstream_socket().remote_endpoint().port());
            async_write(downstream_socket_,
                 boost::asio::buffer(upstream_data_,bytes_transferred),
                 boost::bind(&bridge::handle_downstream_write,
                      shared_from_this(),
                      boost::asio::placeholders::error));
         }
         else
            close();
      }

      // Write to client complete, Async read from remote server
      void handle_downstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            // std::cout << "handle_downstream_write : " << upstream_data_ << std::endl;
            upstream_socket_.async_read_some(
                 boost::asio::buffer(upstream_data_,max_data_length),
                 boost::bind(&bridge::handle_upstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
         }
         else
            close();
      }
      // *** End Of Section A ***


      /*
         Section B: Client --> Proxy --> Remove Server
         Process data recieved from client then write to remove server.
      */

      // Read from client complete, now send data to remote server
      void handle_downstream_read(const boost::system::error_code& error,
                                  const size_t& bytes_transferred)
      {
         if (!error)
         {
            // std::cout << "Data to write to server :" << downstream_data_ << ", len : " << bytes_transferred << std::endl;
            // std::cout << "upstream socket : " << upstream_socket_.remote_endpoint().address() << ", port : " << upstream_socket_.remote_endpoint().port() << std::endl;
            std::string incoming = std::string((char*)downstream_data_, bytes_transferred);
            dLogger.addIncomingLog(incoming, downstream_socket().remote_endpoint().address().to_string(), downstream_socket().remote_endpoint().port());
            async_write(upstream_socket_,
                  boost::asio::buffer(downstream_data_,bytes_transferred),
                  boost::bind(&bridge::handle_upstream_write,
                        shared_from_this(),
                        boost::asio::placeholders::error));
         }
         else
            close();
      }

      // Write to remote server complete, Async read from client
      void handle_upstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            // std::cout << "handle_upstream_write : " << downstream_data_ << std::endl;
            downstream_socket_.async_read_some(
                 boost::asio::buffer(downstream_data_,max_data_length),
                 boost::bind(&bridge::handle_downstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
         }
         else
            close();
      }
      // *** End Of Section B ***

      void close()
      {
         boost::mutex::scoped_lock lock(mutex_);

         if (downstream_socket_.is_open())
         {
            downstream_socket_.close();
         }

         if (upstream_socket_.is_open())
         {
            upstream_socket_.close();
         }
      }

      socket_type downstream_socket_;
      socket_type upstream_socket_;
      DataLogger dLogger;

      unsigned char downstream_data_[max_data_length];
      unsigned char upstream_data_  [max_data_length];

      boost::mutex mutex_;

   public:

      class acceptor
      {
      public:

         acceptor(boost::asio::io_service& io_service,
                  const std::string& local_host, unsigned short local_port,
                  const std::string& upstream_host, unsigned short upstream_port)
         : io_service_(io_service),
           localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
           acceptor_(io_service_,ip::tcp::endpoint(localhost_address,local_port)),
           upstream_port_(upstream_port),
           upstream_host_(upstream_host)
         {}

         bool accept_connections()
         {
            try
            {
               session_ = boost::shared_ptr<bridge>(new bridge(io_service_));

               acceptor_.async_accept(session_->downstream_socket(),
                    boost::bind(&acceptor::handle_accept,
                         this,
                         boost::asio::placeholders::error));
            }
            catch(std::exception& e)
            {
               std::cerr << "acceptor exception: " << e.what() << std::endl;
               return false;
            }

            return true;
         }

      private:

         void handle_accept(const boost::system::error_code& error)
         {
            if (!error)
            {
               // std::cout << "Host : " << upstream_host_ << ", Port : " << upstream_port_ << std::endl;
               session_->start(upstream_host_,upstream_port_);

               if (!accept_connections())
               {
                  std::cerr << "Failure during call to accept." << std::endl;
               }
            }
            else
            {
               std::cerr << "Error: " << error.message() << std::endl;
            }
         }

         boost::asio::io_service& io_service_;
         ip::address_v4 localhost_address;
         ip::tcp::acceptor acceptor_;
         ptr_type session_;
         unsigned short upstream_port_;
         std::string upstream_host_;
      };

   };
}

int main(int argc, char* argv[])
{
   if (argc != 5)
   {
      std::cerr << "usage: " << argv[0] << " <local host ip> <local port> <forward host ip> <forward port>" << std::endl;
      return 1;
   }

   const unsigned short local_port   = static_cast<unsigned short>(::atoi(argv[2]));
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
   const std::string local_host      = argv[1];
   const std::string forward_host    = argv[3];

   boost::asio::io_service ios;

   try
   {
      tcp_proxy::bridge::acceptor acceptor(ios,
                                           local_host, local_port,
                                           forward_host, forward_port);

      acceptor.accept_connections();
      ios.run();
   }
   catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}

/*
 * [Note] On posix systems the tcp proxy server build command is as follows:
 * c++ -pedantic -ansi -Wall -Werror -O3 -o tcpproxy_server tcpproxy_server.cpp -L/usr/lib -lstdc++ -lpthread -lboost_thread -lboost_system
 */

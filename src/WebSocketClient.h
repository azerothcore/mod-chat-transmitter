#ifndef _MOD_CHAT_TRANSMITTER_WEB_SOCKET_CLIENT_H_
#define _MOD_CHAT_TRANSMITTER_WEB_SOCKET_CLIENT_H_

#include <queue>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace ModChatTransmitter
{
    class WebSocketClient : public std::enable_shared_from_this<WebSocketClient>
    {
    public:
        explicit WebSocketClient(net::io_context& ioc);
        void Run(const std::string& host, int port, const std::string& path);
        void QueueMessage(const std::string& text);
        bool IsReady();
        void Close();

    private:
        void Resolve();
        void Write();
        void Read();
        void OnResolve(beast::error_code err, tcp::resolver::results_type results);
        void OnConnect(beast::error_code err, tcp::resolver::results_type::endpoint_type ep);
        void OnHandshake(beast::error_code err);
        void OnWrite(beast::error_code err, std::size_t bytes_transferred);
        void OnRead(beast::error_code err, std::size_t bytes_transferred);
        void OnClose(beast::error_code err);
        void OnError(beast::error_code err, const char* operation);

        tcp::resolver resolver;
        websocket::stream<beast::tcp_stream> ws;
        beast::flat_buffer buffer;
        std::string host;
        std::string path;
        int port;
        bool ready;
        bool close;
        int reconnectDelay;
        int reconnectAttempts;
        std::queue<std::string> workQueue;
    };
}

#endif // _MOD_CHAT_TRANSMITTER_WEB_SOCKET_CLIENT_H_

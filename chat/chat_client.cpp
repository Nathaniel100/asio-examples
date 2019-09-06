#include <deque>
#include <thread>

#include <stdio.h>
#include <string.h>

#include <asio.hpp>

#include "chat_message.h"

typedef std::deque<ChatMessage> ChatMessageQueue;
using asio::ip::tcp;

class ChatClient {
 public:
  ChatClient(asio::io_context &io_context,
             const tcp::resolver::results_type &endpoints)
      : io_context_(io_context), socket_(io_context) {
    DoConnect(endpoints);
  }

  void WriteMessage(const ChatMessage &msg) {
    asio::post(io_context_, [this, msg]() {
      bool write_in_progress = !write_msgs_.empty();
      write_msgs_.push_back(msg);
      if (!write_in_progress) {
        DoWrite();
      }
    });
  }

  void Close() {
    asio::post(io_context_, [this]() { socket_.close(); });
  }

 private:
  void DoConnect(const tcp::resolver::results_type &endpoints) {
    asio::async_connect(
        socket_, endpoints, [this](asio::error_code ec, tcp::endpoint) {
          if (!ec) {
            DoReadHeader();
          } else {
            printf("connect failed(%d): %s\n", ec.value(), ec.message().c_str());
          }
        });
  }

  void DoReadHeader() {
    asio::async_read(socket_,
                     asio::buffer(read_msg_.data(), ChatMessage::HEADER_LENGTH),
                     [this](asio::error_code ec, size_t /*length*/) {
                       if (!ec && read_msg_.decode_header()) {
                         DoReadBody();
                       } else {
                         socket_.close();
                       }
                     });
  }

  void DoReadBody() {
    asio::async_read(
        socket_, asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](asio::error_code ec, size_t /*length*/) {
          if (!ec) {
            printf("%.*s\n", static_cast<int>(read_msg_.body_length()),
                   read_msg_.body());
            DoReadHeader();
          } else {
            socket_.close();
          }
        });
  }

  void DoWrite() {
    asio::async_write(
        socket_,
        asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this](asio::error_code ec, size_t /*length*/) {
          if (!ec) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
              DoWrite();
            }
          } else {
            socket_.close();
          }
        });
  }

 private:
  asio::io_context &io_context_;
  tcp::socket socket_;
  tcp::resolver::results_type endpoints;
  ChatMessage read_msg_;
  ChatMessageQueue write_msgs_;
};

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: chat_client <host> <port>\n");
    return -1;
  }
  const char *host = argv[1];
  const char *port = argv[2];
  try {
    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(host, port);
    ChatClient c(io_context, endpoints);
    std::thread t{[&io_context]() { io_context.run(); }};
    char line[ChatMessage::MAX_MESSAGE_LENGTH + 1] = {0};

    while (fgets(line, sizeof(line), stdin)) {
      int line_length = strlen(line) - 1;  // 最后一位是\n, 替换为\0
      line[line_length] = '\0';
      ChatMessage chat_message;
      chat_message.body_length(line_length);
      memcpy(chat_message.body(), line, line_length);
      chat_message.encode_header();
      c.WriteMessage(chat_message);
    }

    c.Close();
    t.join();
  } catch (std::exception &e) {
    printf("chat client failed: %s\n", e.what());
  }
  return 0;
}

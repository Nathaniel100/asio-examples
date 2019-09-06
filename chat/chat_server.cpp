#include <stdio.h>
#include <string.h>

#include <deque>
#include <list>
#include <memory>
#include <set>

#include <asio.hpp>

#include "chat_message.h"

using asio::ip::tcp;

using ChatMessageQueue = std::deque<ChatMessage>;

struct ChatParticipant {
  virtual ~ChatParticipant() {}
  virtual void Deliver(const ChatMessage &msg) = 0;
};

using ChatParticipantPtr = std::shared_ptr<ChatParticipant>;

class ChatRoom {
 public:
  void Join(ChatParticipantPtr participant) {
    participants_.insert(participant);
    for (const auto &msg : recent_msgs_) {
      participant->Deliver(msg);
    }
  }
  void Leave(ChatParticipantPtr participant) {
    participants_.erase(participant);
  }

  void Deliver(const ChatMessage &msg) {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > MAX_RECENT_MSGS) {
      recent_msgs_.pop_front();
    }
    for (auto participant : participants_) {
      participant->Deliver(msg);
    }
  }

 private:
  std::set<ChatParticipantPtr> participants_;
  enum { MAX_RECENT_MSGS = 100 };
  ChatMessageQueue recent_msgs_;
};

class ChatSession : public ChatParticipant,
                    public std::enable_shared_from_this<ChatSession> {
 public:
  ChatSession(tcp::socket socket, ChatRoom &room)
      : socket_(std::move(socket)), room_(room) {}

  void Start() {
    room_.Join(shared_from_this());
    DoReadHeader();
  }

  virtual void Deliver(const ChatMessage &msg) {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
      DoWrite();
    }
  }

 private:
  void DoReadHeader() {
    auto self(shared_from_this());
    asio::async_read(socket_,
                     asio::buffer(read_msg_.data(), ChatMessage::HEADER_LENGTH),
                     [this, self](asio::error_code ec, size_t /*length*/) {
                       if (!ec && read_msg_.decode_header()) {
                         DoReadBody();
                       } else {
                         room_.Leave(self);
                       }
                     });
  }

  void DoReadBody() {
    auto self(shared_from_this());
    asio::async_read(socket_,
                     asio::buffer(read_msg_.body(), read_msg_.body_length()),
                     [this, self](asio::error_code ec, size_t /*length*/) {
                       if (!ec) {
                         room_.Deliver(read_msg_);
                         DoReadHeader();
                       } else {
                         room_.Leave(self);
                       }
                     });
  }

  void DoWrite() {
    auto self(shared_from_this());
    asio::async_write(
        socket_,
        asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this, self](asio::error_code ec, size_t /*length*/) {
          if (!ec) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
              DoWrite();
            }
          } else {
            room_.Leave(self);
          }
        });
  }

 private:
  tcp::socket socket_;
  ChatRoom &room_;
  ChatMessage read_msg_;
  ChatMessageQueue write_msgs_;
};

class ChatServer {
 public:
  ChatServer(asio::io_context &io_context, const tcp::endpoint &endpoint)
      : acceptor_(io_context, endpoint) {
    DoAccept();
  }

 private:
  void DoAccept() {
    acceptor_.async_accept([this](asio::error_code ec, tcp::socket socket) {
      if (!ec) {
        std::make_shared<ChatSession>(std::move(socket), room_)->Start();
      }
      DoAccept();
    });
  }

 private:
  tcp::acceptor acceptor_;
  ChatRoom room_;
};

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: chat_server <port> [<port> ...]\n");
    return -1;
  }
  try {
    asio::io_context io_context;
    std::list<ChatServer> servers;
    for (int i = 1; i < argc; i++) {
      tcp::endpoint endpoint(tcp::v4(), strtol(argv[i], nullptr, 0));
      servers.emplace_back(io_context, endpoint);
    }

    io_context.run();
  } catch (std::exception &e) {
    printf("Exception: %s\n", e.what());
  }
  return 0;
}


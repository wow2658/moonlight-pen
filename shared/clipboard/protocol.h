// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/** @brief Private, versioned Moonlight Pen clipboard protocol (not an upstream protocol). */
namespace pen_clipboard {
  /** @brief Compare Windows text content independently of LF/CRLF and clipboard format bookkeeping. */
  inline std::string text_identity(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '\r') {
        result.push_back('\n');
        if (i + 1 < text.size() && text[i + 1] == '\n') {
          ++i;
        }
      } else {
        result.push_back(text[i]);
      }
    }
    return result;
  }

  /** @brief Remember only the current text; sequence/format changes alone must not create network echoes. */
  class TextObservation {
  public:
    /** @brief Establish a baseline after negotiation or a remote write without transmitting it. */
    void remember(const std::optional<std::string> &text) {
      value_ = text ? std::optional<std::string>(text_identity(*text)) : std::nullopt;
    }

    /** @brief Return true only for a new text value, or a nontext event invalidating pending remote text. */
    bool changed(const std::optional<std::string> &text) {
      if (!text) {
        value_.reset();
        return true;
      }
      auto current = text_identity(*text);
      if (value_ && *value_ == current) {
        return false;
      }
      value_ = std::move(current);
      return true;
    }

  private:
    std::optional<std::string> value_;  ///< Current connection only; never written to logs or settings.
  };

  constexpr uint16_t control_type = 0x5c10;  ///< Only carried inside authenticated encrypted control packets.
  constexpr uint8_t channel = 6;  ///< Separate reliable ENet channel from keyboard and pen input.
  constexpr size_t header_size = 24;  ///< Little endian MPCT v1 header size.
  constexpr size_t chunk_size = 8192;  ///< Stop-and-wait keeps at most one text chunk in flight.
  constexpr size_t max_text = 16 * 1024 * 1024;  ///< Maximum UTF-8 text size; both peers need the same raised limit.
  enum class Role : uint8_t {
    client = 1,
    host = 2
  };  ///< Origin of a clipboard revision.
  enum class Kind : uint8_t {
    hello = 1,
    welcome,
    text,
    ack,
    invalidate,
    goodbye
  };  ///< Wire operation.
  enum class State {
    waiting,
    connected,
    stopped,
    failed
  };  ///< Sharing status, without clipboard contents.

  /** @brief Logical revision; simultaneous edits resolve consistently in favour of the host. */
  struct Revision {
    uint64_t counter = 0;  ///< Lamport counter, scoped to one streaming connection.
    uint8_t origin = 0;  ///< Client or host role.

    bool operator==(const Revision &b) const {
      return counter == b.counter && origin == b.origin;
    }

    bool operator<(const Revision &b) const {
      return counter < b.counter || (counter == b.counter && origin < b.origin);
    }
  };

  /** @brief Parsed packet, holding an owning copy only of this bounded chunk. */
  struct Packet {
    Kind kind;  ///< Message operation.
    Revision revision;  ///< Originating clipboard revision (ACK preserves the original origin).
    uint32_t total = 0;  ///< Complete UTF-8 length.
    uint32_t offset = 0;  ///< Chunk start, or next expected offset in an ACK.
    std::string data;  ///< At most chunk_size bytes.
  };

  /** @brief Append a little endian integer without unaligned loads. */
  inline void put(std::string &out, uint64_t n, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
      out.push_back(static_cast<char>(n >> (8 * i)));
    }
  }

  /** @brief Read a previously bounds-checked little endian integer. */
  inline uint64_t get(std::string_view in, size_t at, unsigned bytes) {
    uint64_t n = 0;
    for (unsigned i = 0; i < bytes; ++i) {
      n |= uint64_t(static_cast<uint8_t>(in[at + i])) << (8 * i);
    }
    return n;
  }

  /** @brief Serialize one protocol packet; transport encryption is mandatory outside this codec. */
  inline std::string encode(const Packet &p) {
    std::string out("MPCT", 4);
    out.push_back(1);
    out.push_back(static_cast<char>(p.kind));
    out.push_back(static_cast<char>(p.revision.origin));
    out.push_back(0);
    put(out, p.revision.counter, 8);
    put(out, p.total, 4);
    put(out, p.offset, 4);
    out += p.data;
    return out;
  }

  /** @brief Validate size, version and structural fields before accepting any allocation request. */
  inline std::optional<Packet> decode(std::string_view bytes) {
    if (bytes.size() < header_size || bytes.size() > header_size + chunk_size || bytes.substr(0, 4) != "MPCT" || bytes[4] != 1 || bytes[7] != 0) {
      return {};
    }
    auto kind = static_cast<Kind>(bytes[5]);
    const auto origin = static_cast<uint8_t>(bytes[6]);
    if (kind < Kind::hello || kind > Kind::goodbye || origin < 1 || origin > 2) {
      return {};
    }
    Packet p {kind, {get(bytes, 8, 8), origin}, static_cast<uint32_t>(get(bytes, 16, 4)), static_cast<uint32_t>(get(bytes, 20, 4)), {}};
    if (p.total > max_text || p.offset > p.total) {
      return {};
    }
    if (kind == Kind::text) {
      if (!p.revision.counter || bytes.size() - header_size > p.total - p.offset || (p.total && bytes.size() == header_size)) {
        return {};
      }
    } else if (bytes.size() != header_size) {
      return {};
    }
    if (kind != Kind::text && kind != Kind::ack && (p.total || p.offset)) {
      return {};
    }
    if ((kind == Kind::hello || kind == Kind::welcome || kind == Kind::goodbye) && p.revision.counter) {
      return {};
    }
    if ((kind == Kind::invalidate || kind == Kind::ack) && !p.revision.counter) {
      return {};
    }
    p.data.assign(bytes.substr(header_size));
    return p;
  }

  /** @brief Strict UTF-8 validation: reject embedded NUL, overlong encodings and surrogate code points. */
  inline bool valid_text(std::string_view s) {
    for (size_t i = 0; i < s.size();) {
      uint32_t cp = static_cast<uint8_t>(s[i++]);
      if (cp == 0) {
        return false;
      }
      if (cp < 0x80) {
        continue;
      }
      unsigned more;
      uint32_t minimum;
      if (cp >= 0xc2 && cp <= 0xdf) {
        more = 1;
        minimum = 0x80;
        cp &= 0x1f;
      } else if (cp >= 0xe0 && cp <= 0xef) {
        more = 2;
        minimum = 0x800;
        cp &= 0x0f;
      } else if (cp >= 0xf0 && cp <= 0xf4) {
        more = 3;
        minimum = 0x10000;
        cp &= 0x07;
      } else {
        return false;
      }
      if (s.size() - i < more) {
        return false;
      }
      for (unsigned j = 0; j < more; ++j) {
        const auto c = static_cast<uint8_t>(s[i++]);
        if ((c & 0xc0) != 0x80) {
          return false;
        }
        cp = (cp << 6) | (c & 0x3f);
      }
      if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        return false;
      }
    }
    return true;
  }

  /** @brief Single-threaded state machine; adapter owns the worker thread and OS clipboard. */
  class Endpoint {
  public:
    using Send = std::function<bool(const std::string &)>;  ///< Bounded transport enqueue operation.
    using Apply = std::function<void(Revision, std::optional<std::string>)>;  ///< Replace/cancel pending remote text.
    using Notify = std::function<void(State)>;  ///< Content-free state notification.

    /** @brief Construct a disconnected endpoint without reading or transmitting the clipboard. */
    Endpoint(Role role, Send send, Apply apply, Notify notify):
        role_(role),
        send_(std::move(send)),
        apply_(std::move(apply)),
        notify_(std::move(notify)) {}

    /** @brief Start client negotiation; the host waits for an explicit HELLO. */
    void begin(uint64_t now) {
      started_ = now_ = now;
      if (role_ == Role::client) {
        send_packet({Kind::hello, {0, static_cast<uint8_t>(role_)}});
      }
    }

    State state() const {
      return state_;
    }  ///< Query current sharing state.

    Revision revision() const {
      return current_;
    }  ///< Query latest accepted revision.

    /** @brief Fail closed, releasing buffered clipboard data and cancelling pending writes. */
    void fail() {
      if (state_ == State::failed || state_ == State::stopped) {
        return;
      }
      clear();
      state_ = State::failed;
      notify_(state_);
    }

    /** @brief Stop this connection; no buffered text survives the endpoint's lifetime. */
    void stop() {
      clear();
      state_ = State::stopped;
      notify_(state_);
    }

    /** @brief Time out an unsupported host or stalled transfer without affecting streaming. */
    void tick(uint64_t now) {
      now_ = now;
      if ((state_ == State::waiting && now - started_ >= 5000) || (waiting_ack_ && now - sent_at_ >= 10000)) {
        fail();
      }
    }

    /** @brief Publish a new OS clipboard revision; nullopt invalidates earlier text transfers. */
    void local(std::optional<std::string> text) {
      if (state_ != State::connected) {
        return;
      }
      if (current_.counter == std::numeric_limits<uint64_t>::max()) {
        fail();
        return;
      }
      current_ = {current_.counter + 1, static_cast<uint8_t>(role_)};
      clear();
      if (!text || text->size() > max_text || !valid_text(*text)) {
        send_packet({Kind::invalidate, current_});
        return;
      }
      outgoing_ = std::move(*text);
      sending_ = true;
      send_chunk();
    }

    /** @brief Receive one decrypted frame, rejecting stale and malformed revisions. */
    void receive(std::string_view bytes) {
      if (state_ == State::failed || state_ == State::stopped) {
        return;
      }
      auto decoded = decode(bytes);
      if (!decoded) {
        fail();
        return;
      }
      const auto &p = *decoded;
      const auto remote = static_cast<uint8_t>(role_ == Role::client ? Role::host : Role::client);
      if (p.kind == Kind::hello && role_ == Role::host && p.revision.origin == remote) {
        if (state_ == State::waiting) {
          connected();
        }
        send_packet({Kind::welcome, {0, static_cast<uint8_t>(role_)}});
        return;
      }
      if (p.kind == Kind::welcome && role_ == Role::client && p.revision.origin == remote) {
        if (state_ == State::waiting) {
          connected();
        }
        return;
      }
      if (state_ != State::connected) {
        return;
      }
      if (p.kind == Kind::ack) {
        if (!waiting_ack_ || !(p.revision == current_) || p.total != outgoing_.size() || p.offset != next_offset_) {
          return;
        }
        waiting_ack_ = false;
        offset_ = next_offset_;
        if (offset_ == outgoing_.size()) {
          outgoing_.clear();
          sending_ = false;
        } else {
          send_chunk();
        }
        return;
      }
      if (p.revision.origin != remote) {
        fail();
        return;
      }
      if (p.kind == Kind::goodbye) {
        stop();
        return;
      }
      if (p.kind != Kind::text && p.kind != Kind::invalidate) {
        fail();
        return;
      }
      if (p.revision < current_) {
        return;
      }
      if (current_ < p.revision) {
        current_ = p.revision;
        clear();
        incoming_total_ = p.total;
      }
      if (p.kind == Kind::invalidate) {
        clear();
        return;
      }
      if (received_complete_ && p.revision == current_) {
        // Reliable ENet is ordered, but duplicate application frames must not
        // cause another OS clipboard write or resurrect an old transfer.
        if (p.total == incoming_total_) {
          send_packet({Kind::ack, p.revision, p.total, p.total});
        }
        return;
      }
      if (p.total != incoming_total_ || p.offset != incoming_.size()) {
        fail();
        return;
      }
      incoming_ += p.data;
      send_packet({Kind::ack, p.revision, p.total, static_cast<uint32_t>(incoming_.size())});
      if (incoming_.size() == incoming_total_) {
        if (!valid_text(incoming_)) {
          fail();
          return;
        }
        received_complete_ = true;
        apply_(current_, std::move(incoming_));
        incoming_.clear();
      }
    }

  private:
    /** @brief Mark negotiation complete; the OS adapter takes its initial baseline here. */
    void connected() {
      state_ = State::connected;
      notify_(state_);
    }

    /** @brief Clear transfer state and invalidate any delayed clipboard write. */
    void clear() {
      incoming_.clear();
      outgoing_.clear();
      incoming_total_ = 0;
      offset_ = next_offset_ = 0;
      waiting_ack_ = sending_ = received_complete_ = false;
      apply_(current_, std::nullopt);
    }

    /** @brief Queue a bounded packet, disabling sharing on transport failure. */
    void send_packet(const Packet &p) {
      if (!send_(encode(p))) {
        fail();
      }
    }

    /** @brief Send exactly one chunk and wait for the receiver's application-level ACK. */
    void send_chunk() {
      const auto count = (std::min) (chunk_size, outgoing_.size() - offset_);
      next_offset_ = offset_ + static_cast<uint32_t>(count);
      waiting_ack_ = true;
      sent_at_ = now_;
      send_packet({Kind::text, current_, static_cast<uint32_t>(outgoing_.size()), offset_, outgoing_.substr(offset_, count)});
    }

    Role role_;  ///< Local origin.
    Send send_;  ///< Transport callback.
    Apply apply_;  ///< OS write callback.
    Notify notify_;  ///< Status callback.
    State state_ = State::waiting;  ///< Current connection status.
    Revision current_;  ///< Latest logical revision.
    std::string outgoing_, incoming_;  ///< Bounded transfer buffers.
    uint32_t incoming_total_ = 0, offset_ = 0, next_offset_ = 0;  ///< Transfer cursors.
    bool waiting_ack_ = false, sending_ = false;  ///< Stop-and-wait state.
    bool received_complete_ = false;  ///< Prevent reapplying a duplicate completed revision.
    uint64_t started_ = 0, now_ = 0, sent_at_ = 0;  ///< Monotonic timeout timestamps.
  };
}  // namespace pen_clipboard

// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/pen_clipboard/protocol.h"

#include <deque>
#include <gtest/gtest.h>
#include <iostream>
#include <random>

using namespace pen_clipboard;
#define CHECK(x) ASSERT_TRUE(x)

struct Pair {
  std::deque<std::string> to_host, to_client;
  std::string client_clip = "untouched client", host_clip = "untouched host";
  unsigned client_writes = 0, host_writes = 0;
  Endpoint client {Role::client, [&](const auto &b) {
                     to_host.push_back(b);
                     return true;
                   },
                   [&](auto, auto text) {
                     if (text) {
                       client_clip = *text;
                       ++client_writes;
                     }
                   },
                   [](auto) {
                   }};
  Endpoint host {Role::host, [&](const auto &b) {
                   to_client.push_back(b);
                   return true;
                 },
                 [&](auto, auto text) {
                   if (text) {
                     host_clip = *text;
                     ++host_writes;
                   }
                 },
                 [](auto) {
                 }};

  void pump() {
    size_t loops = 0;
    while (!to_host.empty() || !to_client.empty()) {
      CHECK(++loops < 1000);
      if (!to_host.empty()) {
        auto b = std::move(to_host.front());
        to_host.pop_front();
        host.receive(b);
      }
      if (!to_client.empty()) {
        auto b = std::move(to_client.front());
        to_client.pop_front();
        client.receive(b);
      }
    }
  }

  void connect() {
    host.begin(100);
    client.begin(100);
    pump();
    CHECK(client.state() == State::connected && host.state() == State::connected);
    CHECK(client_clip == "untouched client" && host_clip == "untouched host");
    CHECK(client_writes == 0 && host_writes == 0);
  }
};

TEST(PenClipboard, RoundTripsAndMalformedFrames) {
  TextObservation observation;
  observation.remember(std::string("existing\r\ntext"));
  CHECK(!observation.changed(std::string("existing\ntext")));
  for (int i = 0; i < 1000; ++i) {
    CHECK(!observation.changed(std::string("existing\r\ntext")));
  }
  CHECK(observation.changed(std::string("new copy")));
  observation.remember(std::string("remote text"));
  CHECK(!observation.changed(std::string("remote text")));
  CHECK(observation.changed(std::nullopt));  // New image/file cancels pending text, even after deduplication.
  CHECK(observation.changed(std::string("remote text")));  // Copying it again after an image is a new text copy.
  CHECK(observation.changed(std::string("")));
  CHECK(!observation.changed(std::string("")));
  Pair p;
  p.client.local("must not send before opt-in");
  CHECK(p.to_host.empty());
  p.connect();
  const std::string text = "한글 가나다\r\nEnglish 123\n😀🖊️\t끝";
  p.client.local(text);
  p.pump();
  CHECK(p.host_clip == text && p.host_writes == 1 && p.client_writes == 0);
  auto duplicate = encode({Kind::text, p.client.revision(), static_cast<uint32_t>(text.size()), 0, text});
  p.host.receive(duplicate);
  p.pump();
  CHECK(p.host_writes == 1);
  p.host.local("호스트 → 노트북\r\n둘째 줄");
  p.pump();
  CHECK(p.client_clip == "호스트 → 노트북\r\n둘째 줄" && p.client_writes == 1);
  p.client.local("");
  p.pump();
  CHECK(p.host_clip.empty());
  for (size_t size : {size_t(8191), size_t(8192), size_t(8193), max_text}) {
    std::string large(size, 'x');
    large.replace(size - 4, 4, "LAST");
    p.host.local(large);
    CHECK(p.to_client.size() == 1);  // Hard backpressure, not 128 queued chunks.
    p.pump();
    CHECK(p.client_clip == large);
  }
  auto saved = p.host_clip;
  p.client.local(std::string(max_text + 1, 'x'));
  p.pump();
  CHECK(p.host_clip == saved);
  p.client.local(std::string("bad\0text", 8));
  p.pump();
  CHECK(p.host_clip == saved);
  p.client.local(std::string("\xc0\xaf", 2));
  p.pump();
  CHECK(p.host_clip == saved);
  CHECK(!valid_text("\xed\xa0\x80") && !valid_text("\xf4\x90\x80\x80"));
  CHECK(!valid_text("\xe3\x81") && !valid_text("\x80"));
  CHECK(valid_text("字😀") && valid_text(""));
  // A local image/file/clear invalidates in-flight text without clearing the remote clipboard.
  p.client.local(std::string(20000, 'a'));
  p.client.local(std::nullopt);
  p.pump();
  CHECK(p.host_clip == saved);
  // A newer local copy cancels an unfinished old transfer.
  p.client.local(std::string(25000, 'b'));
  p.client.local("newest");
  p.pump();
  CHECK(p.host_clip == "newest");
  // Simultaneous opposite-direction edits converge deterministically and never echo forever.
  p.client.local("client simultaneous");
  p.host.local("host simultaneous");
  p.pump();
  CHECK(p.client_clip == "host simultaneous");
  CHECK(p.client.state() == State::connected && p.host.state() == State::connected);
  for (int i = 0; i < 200; ++i) {
    auto s = "alternate " + std::to_string(i);
    if (i % 2) {
      p.host.local(s);
      p.pump();
      CHECK(p.client_clip == s);
    } else {
      p.client.local(s);
      p.pump();
      CHECK(p.host_clip == s);
    }
  }
  // A stopped/disconnected session must not apply late packets or carry them into a new session.
  auto late = encode({Kind::text, {999, 2}, 4, 0, "late"});
  auto before = p.client_clip;
  p.client.stop();
  p.client.receive(late);
  CHECK(p.client_clip == before);
  Pair reconnect;
  reconnect.connect();
  CHECK(reconnect.client_writes == 0);
  Pair unsupported;
  unsupported.client.begin(10);
  unsupported.client.tick(5010);
  CHECK(unsupported.client.state() == State::failed);
  Pair timeout;
  timeout.connect();
  timeout.client.local("no acknowledgement");
  timeout.client.tick(10100);
  CHECK(timeout.client.state() == State::failed);
  // Malformed/oversize headers are rejected before allocation or clipboard mutation.
  Pair malformed;
  malformed.connect();
  malformed.client.receive(encode({Kind::text, {1, 2}, static_cast<uint32_t>(max_text + 1), 0, "x"}));
  CHECK(malformed.client.state() == State::failed && malformed.client_writes == 0);
  CHECK(!decode(""));
  CHECK(!decode(std::string(header_size + chunk_size + 1, 'x')));
  std::mt19937 rng(0x434c4950);
  for (int i = 0; i < 10000; ++i) {
    std::string bytes(rng() % 100, '\0');
    for (auto &c : bytes) {
      c = static_cast<char>(rng());
    }
    auto decoded = decode(bytes);
    if (decoded) {
      CHECK(decoded->data.size() <= chunk_size && decoded->total <= max_text);
    }
  }
  std::cout << "PASS: negotiation, Unicode, both directions, 1 MiB, flow control, invalidation, races, reconnect, limits and malformed frames\n";
}

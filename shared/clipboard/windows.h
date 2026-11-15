// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include "protocol.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <windows.h>

namespace pen_clipboard {
  /** @brief A clipboard snapshot; null text means an unsupported/empty clipboard revision. */
  struct Snapshot {
    DWORD sequence = 0;  ///< Sequence after delayed rendering completes.
    std::optional<std::string> text;  ///< Valid, bounded UTF-8 plain text only.
  };

  /** @brief Read Unicode content while the calling thread already holds the Windows clipboard lock. */
  inline Snapshot read_open_clipboard() {
    Snapshot result;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && !IsClipboardFormatAvailable(CF_HDROP)) {
      HANDLE data = GetClipboardData(CF_UNICODETEXT);
      const auto bytes = data ? GlobalSize(data) : 0;
      if (bytes >= sizeof(wchar_t) && bytes <= (max_text + 1) * sizeof(wchar_t)) {
        const auto *p = static_cast<const wchar_t *>(GlobalLock(data));
        if (p) {
          const size_t capacity = bytes / sizeof(wchar_t);
          const auto end = std::find(p, p + capacity, L'\0');
          if (end != p + capacity) {
            const int length = static_cast<int>(end - p);
            const int required = length ? WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, p, length, nullptr, 0, nullptr, nullptr) : 0;
            if ((!length || required > 0) && required <= static_cast<int>(max_text)) {
              std::string text(static_cast<size_t>(required), '\0');
              if (!length || WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, p, length, text.data(), required, nullptr, nullptr) == required) {
                result.text = std::move(text);
              }
            }
          }
          GlobalUnlock(data);
        }
      }
    }
    result.sequence = GetClipboardSequenceNumber();
    return result;
  }

  /** @brief Read one stable Unicode snapshot, returning nullopt if another app holds the clipboard. */
  inline std::optional<Snapshot> read_clipboard(HWND owner) {
    if (!OpenClipboard(owner)) {
      return {};
    }
    auto result = read_open_clipboard();
    CloseClipboard();
    return result;
  }

  enum class WriteResult {
    written,
    unchanged,
    busy,
    changed,
    invalid
  };  ///< Clipboard write outcome.

  /** @brief Replace clipboard text only if no local copy occurred since the adapter's last snapshot. */
  inline WriteResult write_clipboard(HWND owner, std::string_view text, DWORD expected, DWORD &written_sequence) {
    if (text.size() > max_text || !valid_text(text)) {
      return WriteResult::invalid;
    }
    const int count = text.empty() ? 0 : MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!text.empty() && count <= 0) {
      return WriteResult::invalid;
    }
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(count) + 1) * sizeof(wchar_t));
    if (!data) {
      return WriteResult::busy;
    }
    auto *destination = static_cast<wchar_t *>(GlobalLock(data));
    if (!destination) {
      GlobalFree(data);
      return WriteResult::busy;
    }
    if (count) {
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), destination, count);
    }
    destination[count] = L'\0';
    GlobalUnlock(data);
    if (!OpenClipboard(owner)) {
      GlobalFree(data);
      return WriteResult::busy;
    }
    if (GetClipboardSequenceNumber() != expected) {
      CloseClipboard();
      GlobalFree(data);
      return WriteResult::changed;
    }
    // Do not empty/repopulate an identical clipboard. Windows history and other
    // format providers can alter its sequence without changing the copied text.
    const auto current = read_open_clipboard();
    if (current.text && text_identity(*current.text) == text_identity(text)) {
      written_sequence = current.sequence;
      CloseClipboard();
      GlobalFree(data);
      return WriteResult::unchanged;
    }
    const bool ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
    written_sequence = GetClipboardSequenceNumber();
    CloseClipboard();
    if (!ok) {
      GlobalFree(data);
      return WriteResult::busy;
    }
    return WriteResult::written;
  }

  /** @brief Check that this worker is on the user's input desktop, excluding lock/UAC desktops. */
  inline bool on_input_desktop() {
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) {
      return false;
    }
    wchar_t current_name[256] = {}, input_name[256] = {};
    DWORD needed = 0;
    const bool match = GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, current_name, sizeof(current_name), &needed) &&
                       GetUserObjectInformationW(input, UOI_NAME, input_name, sizeof(input_name), &needed) &&
                       wcscmp(current_name, input_name) == 0;
    CloseDesktop(input);
    return match;
  }

  /** @brief Bounded worker owning all Windows clipboard calls; transport callbacks never touch the GUI thread. */
  class WindowsBridge {
  public:
    using Sender = Endpoint::Send;  ///< Client sends directly; host drains its queue on the ENet thread.
    using Notifier = Endpoint::Notify;  ///< Optional content-free status callback.
    using DesktopCheck = std::function<bool()>;  ///< Injectable desktop check for isolated window-station tests.

    /** @brief Construct an idle bridge. No clipboard access occurs before explicit start and negotiation. */
    explicit WindowsBridge(Role role, Sender sender = {}, Notifier notifier = {}, DesktopCheck desktop_check = on_input_desktop):
        role_(role),
        sender_(std::move(sender)),
        notifier_(std::move(notifier)),
        desktop_check_(std::move(desktop_check)) {}

    ~WindowsBridge() {
      stop();
    }  ///< Join before transport destruction.

    WindowsBridge(const WindowsBridge &) = delete;
    WindowsBridge &operator=(const WindowsBridge &) = delete;

    /** @brief Start the clipboard worker once, before accepting transport frames. */
    void start() {
      if (thread_.joinable()) {
        return;
      }
      cancelled_ = false;
      accepting_ = true;
      thread_ = std::thread([this] {
        run();
      });
    }

    /** @brief Stop accepting work immediately; safe from a control-stream termination callback. */
    void cancel() {
      accepting_ = false;
      cancelled_ = true;
    }

    /** @brief Stop and join before freeing session/cipher/network objects. */
    void stop() {
      cancel();
      if (thread_.joinable()) {
        thread_.join();
      }
      std::lock_guard<std::mutex> lock(mutex_);
      incoming_.clear();
      outgoing_.clear();
    }

    State state() const {
      return state_;
    }  ///< Get current status without clipboard contents.

    /** @brief Queue only a bounded, structurally valid frame from the authenticated control channel. */
    bool receive(std::string_view bytes) {
      if (!accepting_) {
        return false;
      }
      if (!decode(bytes)) {
        overflow_ = true;
        return false;
      }
      std::lock_guard<std::mutex> lock(mutex_);
      if (!accepting_) {
        return false;
      }
      if (incoming_.size() >= 32) {
        overflow_ = true;
        return false;
      }
      incoming_.emplace_back(bytes);
      return true;
    }

    /** @brief Pop one host response; only the host's existing control thread performs encryption/send. */
    std::optional<std::string> take_outgoing() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (outgoing_.empty() || cancelled_) {
        return {};
      }
      auto bytes = std::move(outgoing_.front());
      outgoing_.pop_front();
      return bytes;
    }

  private:
    /** @brief Register a message-only owner for clipboard ownership and format notifications. */
    static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
      return DefWindowProcW(window, msg, wp, lp);
    }

    /** @brief Deliver one small response, with fixed memory bounds on the host queue. */
    bool send(const std::string &bytes) {
      if (cancelled_) {
        return false;
      }
      if (sender_) {
        return sender_(bytes);
      }
      std::lock_guard<std::mutex> lock(mutex_);
      if (outgoing_.size() >= 32) {
        return false;
      }
      outgoing_.push_back(bytes);
      return true;
    }

    /** @brief Pump clipboard/transport changes on the dedicated worker thread. */
    void run() {
      const wchar_t *class_name = L"MoonlightPenClipboardV1";
      WNDCLASSW wc {};
      wc.lpfnWndProc = window_proc;
      wc.hInstance = GetModuleHandleW(nullptr);
      wc.lpszClassName = class_name;
      RegisterClassW(&wc);
      HWND window = CreateWindowExW(0, class_name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
      if (!window || !AddClipboardFormatListener(window)) {
        if (window) {
          DestroyWindow(window);
        }
        state_ = State::failed;
        accepting_ = false;
        if (notifier_) {
          notifier_(State::failed);
        }
        return;
      }
      DWORD sequence = GetClipboardSequenceNumber();
      TextObservation observation;
      std::optional<std::string> pending;
      Revision pending_revision;
      uint64_t pending_since = 0;
      bool desktop_available = false;
      Endpoint endpoint(role_, [this](const std::string &b) {
        return send(b);
      },
                        [&](Revision r, std::optional<std::string> text) {
                          pending_revision = r;
                          pending = std::move(text);
                          pending_since = GetTickCount64();
                        },
                        [&](State status) {
                          state_ = status;
                          if (status == State::connected) {
                            // Never transmit the clipboard that existed before negotiation completed.
                            sequence = GetClipboardSequenceNumber();
                            if (auto initial = read_clipboard(window)) {
                              sequence = initial->sequence;
                              observation.remember(initial->text);
                            }
                            desktop_available = desktop_check_();
                          }
                          if (notifier_) {
                            notifier_(status);
                          }
                        });
      endpoint.begin(GetTickCount64());
      while (!cancelled_ && endpoint.state() != State::failed && endpoint.state() != State::stopped) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          DispatchMessageW(&msg);
        }
        const auto now = GetTickCount64();
        endpoint.tick(now);
        if (overflow_) {
          endpoint.fail();
          break;
        }
        const bool available = desktop_check_();
        if (available != desktop_available) {
          desktop_available = available;
          sequence = GetClipboardSequenceNumber();
          pending.reset();
          observation.remember(std::nullopt);
          // Cancel data that was copied before switching desktop; resume from new copies.
          if (endpoint.state() == State::connected) {
            endpoint.local(std::nullopt);
          }
        }
        // Observe local changes BEFORE remote packets, so a late network write cannot erase a newer local copy.
        bool local_busy = false;
        if (endpoint.state() == State::connected && available && GetClipboardSequenceNumber() != sequence) {
          auto snapshot = read_clipboard(window);
          if (snapshot) {
            sequence = snapshot->sequence;
            if (observation.changed(snapshot->text)) {
              endpoint.local(std::move(snapshot->text));
            }
          } else {
            local_busy = true;
          }
        }
        std::deque<std::string> frames;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          // Defer network changes while a newer local copy is still being rendered/locked.
          if (!local_busy) {
            frames.swap(incoming_);
          }
        }
        for (const auto &frame : frames) {
          if (cancelled_) {
            break;
          }
          endpoint.receive(frame);
        }
        if (!available) {
          pending.reset();
        }
        if (pending && !local_busy && available && !cancelled_ && endpoint.state() == State::connected && pending_revision == endpoint.revision()) {
          const auto result = write_clipboard(window, *pending, sequence, sequence);
          if (result == WriteResult::written || result == WriteResult::unchanged) {
            observation.remember(pending);
          }
          if (result == WriteResult::written || result == WriteResult::unchanged || result == WriteResult::invalid || result == WriteResult::changed || (now >= pending_since && now - pending_since > 2000)) {
            pending.reset();
          }
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
      }
      if (endpoint.state() != State::failed) {
        endpoint.stop();
      }
      accepting_ = false;
      RemoveClipboardFormatListener(window);
      DestroyWindow(window);
    }

    Role role_;  ///< Endpoint role.
    Sender sender_;  ///< Optional direct client transport.
    Notifier notifier_;  ///< Status callback.
    DesktopCheck desktop_check_;  ///< Production uses the real Windows input desktop check.
    std::thread thread_;  ///< Clipboard owner thread.
    std::atomic<bool> cancelled_ {true}, accepting_ {false}, overflow_ {false};  ///< Cross-thread lifecycle flags.
    std::atomic<State> state_ {State::waiting};  ///< Last content-free state.
    std::mutex mutex_;  ///< Protects frame queues only, never OS clipboard or transport operations.
    std::deque<std::string> incoming_, outgoing_;  ///< At most 32 bounded frames per direction.
  };
}  // namespace pen_clipboard

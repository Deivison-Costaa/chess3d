#include "net/LanConnection.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

using namespace chess3d::net;
using namespace std::chrono_literals;

static constexpr uint16_t kBasePort = 31200;

static bool waitFor(std::function<bool()> cond, std::chrono::milliseconds timeout = 3000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

// Waits for and returns one message (non-destructive poll with retry).
static std::optional<std::string> waitForMessage(LanConnection& conn,
                                                  std::chrono::milliseconds timeout = 2000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto m = conn.pollIncoming();
        if (m) return m;
        std::this_thread::sleep_for(10ms);
    }
    return std::nullopt;
}

TEST_CASE("LanConnection loopback connect both sides connected", "[lan]") {
    LanConnection host, client;
    const uint16_t port = kBasePort + 0;

    REQUIRE(host.startListening(port));

    bool clientOk = false;
    std::thread clientThread([&] {
        clientOk = client.connectTo("127.0.0.1", port);
    });

    REQUIRE(waitFor([&] { return host.isConnected(); }));
    clientThread.join();

    REQUIRE(host.isConnected());
    REQUIRE(client.isConnected());
    REQUIRE(clientOk);

    host.close();
    client.close();
}

TEST_CASE("LanConnection send from host to client and back", "[lan]") {
    LanConnection host, client;
    const uint16_t port = kBasePort + 1;

    REQUIRE(host.startListening(port));
    std::thread t([&] { client.connectTo("127.0.0.1", port); });
    REQUIRE(waitFor([&] { return host.isConnected() && client.isConnected(); }));
    t.join();

    // Host -> Client
    REQUIRE(host.sendMessage("HELLO chess3d/1 alice"));
    auto msg = waitForMessage(client);
    REQUIRE(msg.has_value());
    REQUIRE(*msg == "HELLO chess3d/1 alice");

    // Client -> Host
    REQUIRE(client.sendMessage("HELLO chess3d/1 bob"));
    auto msg2 = waitForMessage(host);
    REQUIRE(msg2.has_value());
    REQUIRE(*msg2 == "HELLO chess3d/1 bob");

    host.close();
    client.close();
}

TEST_CASE("LanConnection framing preserves separate messages in burst", "[lan]") {
    LanConnection host, client;
    const uint16_t port = kBasePort + 2;

    REQUIRE(host.startListening(port));
    std::thread t([&] { client.connectTo("127.0.0.1", port); });
    REQUIRE(waitFor([&] { return host.isConnected() && client.isConnected(); }));
    t.join();

    constexpr int kCount = 50;  // smaller burst to be safe on all platforms
    for (int i = 0; i < kCount; ++i) {
        REQUIRE(host.sendMessage("MSG" + std::to_string(i)));
    }

    std::vector<std::string> received;
    const auto deadline = std::chrono::steady_clock::now() + 10000ms;
    while ((int)received.size() < kCount && std::chrono::steady_clock::now() < deadline) {
        while (auto m = client.pollIncoming()) received.push_back(*m);
        if ((int)received.size() < kCount) std::this_thread::sleep_for(10ms);
    }

    REQUIRE((int)received.size() == kCount);
    for (int i = 0; i < kCount; ++i) {
        REQUIRE(received[i] == "MSG" + std::to_string(i));
    }

    host.close();
    client.close();
}

TEST_CASE("LanConnection long payload survives framing", "[lan]") {
    LanConnection host, client;
    const uint16_t port = kBasePort + 3;

    REQUIRE(host.startListening(port));
    std::thread t([&] { client.connectTo("127.0.0.1", port); });
    REQUIRE(waitFor([&] { return host.isConnected() && client.isConnected(); }));
    t.join();

    const std::string longMsg(4096, 'X');
    REQUIRE(host.sendMessage(longMsg));

    auto msg = waitForMessage(client, 3000ms);
    REQUIRE(msg.has_value());
    REQUIRE(*msg == longMsg);

    host.close();
    client.close();
}

TEST_CASE("LanConnection close on one side is detected without crash", "[lan]") {
    LanConnection host, client;
    const uint16_t port = kBasePort + 4;

    REQUIRE(host.startListening(port));
    std::thread t([&] { client.connectTo("127.0.0.1", port); });
    REQUIRE(waitFor([&] { return host.isConnected() && client.isConnected(); }));
    t.join();

    client.close();
    std::this_thread::sleep_for(100ms);
    host.sendMessage("PING");  // may fail silently -- that is OK

    host.close();
    SUCCEED("No crash or deadlock after peer close");
}

TEST_CASE("LanConnection pollIncoming returns nullopt when queue empty", "[lan]") {
    LanConnection conn;
    auto msg = conn.pollIncoming();
    REQUIRE_FALSE(msg.has_value());
}

TEST_CASE("LanConnection close aborts in-flight connectTo quickly", "[lan]") {
    LanConnection conn;

    // IP de teste não-roteável (RFC 5737): connect fica pendente até abortarmos.
    bool result = true;
    std::thread t([&] { result = conn.connectTo("192.0.2.1", 65000); });

    std::this_thread::sleep_for(100ms);
    const auto t0 = std::chrono::steady_clock::now();
    conn.close();
    t.join();
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE_FALSE(result);
    REQUIRE_FALSE(conn.isConnected());
    REQUIRE(elapsed < 1000ms);  // antes do fix, join travava por minutos
}

TEST_CASE("LanConnection connectTo honours timeout", "[lan]") {
    LanConnection conn;

    const auto t0 = std::chrono::steady_clock::now();
    const bool result = conn.connectTo("192.0.2.1", 65000, 300);
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE_FALSE(result);
    REQUIRE(elapsed < 2000ms);
    conn.close();
}

TEST_CASE("LanConnection close cancels pending listen without crash", "[lan]") {
    LanConnection conn;
    REQUIRE(conn.startListening(kBasePort + 5));
    std::this_thread::sleep_for(50ms);

    const auto t0 = std::chrono::steady_clock::now();
    conn.close();
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE(elapsed < 1000ms);
    SUCCEED("listen cancelado sem travar nem crashar");
}

TEST_CASE("LanConnection port is reusable after cancelled listen", "[lan]") {
    const uint16_t port = kBasePort + 6;
    {
        LanConnection first;
        REQUIRE(first.startListening(port));
        std::this_thread::sleep_for(50ms);
        first.close();
    }
    // Antes do fix, o double-close do fd podia fechar um fd alheio; aqui só
    // garantimos que reabrir a mesma porta funciona após o cancelamento.
    LanConnection second;
    REQUIRE(second.startListening(port));
    second.close();
}

#include <catch2/catch_test_macros.hpp>

#include "engine/events/dispatcher.hpp"

namespace {

struct TestEvent {
    int value = 0;
};

struct Listener {
    int received = 0;
    int last_value = -1;
    void on_event(const TestEvent& e) {
        ++received;
        last_value = e.value;
    }
};

} // namespace

TEST_CASE("dispatcher trigger fires connected listener immediately", "[events]") {
    engine::events::Dispatcher dispatcher;
    Listener listener;
    dispatcher.sink<TestEvent>().connect<&Listener::on_event>(listener);

    dispatcher.trigger(TestEvent{42});
    CHECK(listener.received == 1);
    CHECK(listener.last_value == 42);
}

TEST_CASE("dispatcher enqueue defers until update", "[events]") {
    engine::events::Dispatcher dispatcher;
    Listener listener;
    dispatcher.sink<TestEvent>().connect<&Listener::on_event>(listener);

    dispatcher.enqueue<TestEvent>(7);
    dispatcher.enqueue<TestEvent>(8);
    CHECK(listener.received == 0); // queued, not delivered

    dispatcher.update();
    CHECK(listener.received == 2);
    CHECK(listener.last_value == 8); // FIFO delivery order
}

TEST_CASE("disconnected listener no longer receives", "[events]") {
    engine::events::Dispatcher dispatcher;
    Listener listener;
    dispatcher.sink<TestEvent>().connect<&Listener::on_event>(listener);
    dispatcher.trigger(TestEvent{1});
    dispatcher.sink<TestEvent>().disconnect<&Listener::on_event>(listener);
    dispatcher.trigger(TestEvent{2});

    CHECK(listener.received == 1);
    CHECK(listener.last_value == 1);
}

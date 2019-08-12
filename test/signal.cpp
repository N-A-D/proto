#include <gtest/gtest.h>
#include "../include/proto/proto.hpp"

struct DummyReceiver0 : proto::receiver {
	void function0(bool x) { ASSERT_TRUE(x); }
	void function1(bool x) const { ASSERT_TRUE(x); }
};

struct DummyReceiver1 : proto::receiver {
	void function0(bool x) { ASSERT_FALSE(x); }
	void function1(bool x) const { ASSERT_FALSE(x); }
};

TEST(SignalTests, ConstructorTests) {

	// Test empty constructor
	proto::signal<void()> signal0;
	proto::signal<void(int)> signal1;
	proto::signal<void(int, int)> signal2;
	proto::signal<int()> signal3;
	proto::signal<int(int)> signal4;
	proto::signal<int(int, int)> signal5;

}

TEST(SignalTests, SignalConnectionTests) {
	DummyReceiver0 dummy0;
	DummyReceiver1 dummy1;

	proto::signal<void(bool)> signal;

	// Test copy connection copy construction
	proto::connection conn0 = signal.connect(&dummy0, &DummyReceiver0::function0);
	proto::connection conn_copy(conn0);
	
	ASSERT_TRUE(conn0); // Test connectedness of the connection
	ASSERT_TRUE(conn_copy); // Test the connectedness of the the copy connection
	ASSERT_EQ(conn0, conn_copy); // Test connection equivalence
	proto::connection conn_move(std::move(conn_copy)); // Test move construction
	ASSERT_TRUE(conn_move); // Validate connectedness of the thief
	ASSERT_FALSE(conn_copy); // Validated the unconnectedness of the victim
	ASSERT_EQ(conn0, conn_move); // Validate the equivalence between connections

	proto::connection conn1 = signal.connect(&dummy1, &DummyReceiver1::function0);
	ASSERT_TRUE(conn1);
	ASSERT_NE(conn1, conn0); // Test inequality between two difference connections

	// Tests disconnecting a connection
	ASSERT_EQ(signal.size(), 2);
	conn0.disconnect();
	ASSERT_FALSE(conn0); // Tests the disconnectedness of the connection
	ASSERT_EQ(signal.size(), 1); // Test the signal's state after a disconnection

	// Tests the connectedness of a scoped connection, equivalence, inequality
	{
		proto::scoped_connection scoped_conn = signal.connect(&dummy1, &DummyReceiver1::function1);
		proto::scoped_connection scoped_conn_copy(scoped_conn);
		ASSERT_TRUE(scoped_conn); // Test connectedness of a scoped connection
		ASSERT_TRUE(scoped_conn_copy); // Test connectedness of a scoped connection copy
		ASSERT_EQ(scoped_conn, scoped_conn_copy); // Test equivalence between two scoped connections
		proto::scoped_connection scoped_conn_move(std::move(scoped_conn_copy));
		ASSERT_TRUE(scoped_conn_move); // Test connectedness of thief scoped connection
		ASSERT_FALSE(scoped_conn_copy); // Test unconnectedness of of the victim
		ASSERT_EQ(scoped_conn, scoped_conn_move); // Test equivalence between two scoped connections
		ASSERT_EQ(signal.size(), 2);

		proto::scoped_connection scoped_conn1 = signal.connect(&dummy0, &DummyReceiver0::function1);
		ASSERT_TRUE(scoped_conn1); // Test connectedness of a scoped connection
		ASSERT_NE(scoped_conn1, scoped_conn); // Test inequality between two scoped connections
		ASSERT_EQ(signal.size(), 3);
	}
	ASSERT_EQ(signal.size(), 1);
}

TEST(SignalTests, SignalClearTest) {
	DummyReceiver0 dummy0;
	DummyReceiver1 dummy1;
	proto::signal<void(bool)> signal;
	ASSERT_TRUE(signal.empty());

	auto conn0 = signal.connect(&dummy0, &DummyReceiver0::function0);
	auto conn1 = signal.connect(&dummy0, &DummyReceiver0::function1);
	auto conn2 = signal.connect(&dummy1, &DummyReceiver1::function0);
	auto conn3 = signal.connect(&dummy1, &DummyReceiver1::function1);
	auto conn4 = signal.connect([](bool x) { ASSERT_TRUE(x); });

	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);
	ASSERT_TRUE(conn2);
	ASSERT_TRUE(conn3);
	ASSERT_TRUE(conn4);

	ASSERT_FALSE(signal.empty());
	ASSERT_EQ(signal.size(), 5);

	signal.clear();

	ASSERT_TRUE(signal.empty());

	ASSERT_FALSE(conn0);
	ASSERT_FALSE(conn1);
	ASSERT_FALSE(conn2);
	ASSERT_FALSE(conn3);
	ASSERT_FALSE(conn4);
}

TEST(SignalTests, SignalEmissionTests) {

	DummyReceiver0 dummy_receiver0;

	proto::signal<void(bool)> signal0;

	signal0.connect(&dummy_receiver0, &DummyReceiver0::function0);
	signal0.connect(&dummy_receiver0, &DummyReceiver0::function1);
	signal0.connect([](bool x) { ASSERT_TRUE(x); });

	ASSERT_EQ(dummy_receiver0.num_connections(), 2);
	ASSERT_EQ(signal0.size(), 3);

	signal0(true);
	
	DummyReceiver1 dummy_receiver1;

	proto::signal<void(bool)> signal1;

	signal1.connect(&dummy_receiver1, &DummyReceiver1::function0);
	signal1.connect(&dummy_receiver1, &DummyReceiver1::function1);
	signal1.connect([](bool x) { ASSERT_FALSE(x); });

	ASSERT_EQ(dummy_receiver1.num_connections(), 2);
	ASSERT_EQ(signal1.size(), 3);

	signal1(false);

}
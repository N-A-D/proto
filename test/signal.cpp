#include <gtest/gtest.h>
#include <proto/proto.hpp>
#include <numeric>

struct DummyReceiver0 : proto::receiver {
	void function0(bool x) { ASSERT_TRUE(x); }
	void function1(bool x) const { ASSERT_TRUE(x); }
};

struct DummyReceiver1 : proto::receiver {
	void function0(bool x) { ASSERT_FALSE(x); }
	void function1(bool x) const { ASSERT_FALSE(x); }
};

TEST(SignalTests, DefaultConstructorTests) {
	proto::signal<void()> signal0;
	proto::signal<void(int)> signal1;
	proto::signal<void(int, int)> signal2;
	proto::signal<int()> signal3;
	proto::signal<int(int)> signal4;
	proto::signal<int(int, int)> signal5;
}

TEST(SignalTests, MoveConstructorTests) {
	proto::signal<void()> signal;
	proto::connection conn0 = signal.connect([]() {});
	proto::connection conn1 = signal.connect([]() {});
	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);
	ASSERT_EQ(signal.size(), 2);
	{
		proto::signal<void()> scoped_signal(std::move(signal));
		ASSERT_TRUE(signal.empty());
		ASSERT_EQ(scoped_signal.size(), 2);

		// Existing connections to signal now belong to scoped_signal
		ASSERT_TRUE(conn0);
		ASSERT_TRUE(conn1);
	}
	// Scoped signal has been destroyed, the existing conns are now invalid
	ASSERT_FALSE(conn0);
	ASSERT_FALSE(conn1);

	proto::signal<void()> signal2;
	conn0 = signal2.connect([]() {});
	conn1 = signal2.connect([]() {});
	ASSERT_EQ(signal2.size(), 2);
	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);
	
	proto::signal<void()> signal3(std::move(signal2));
	ASSERT_TRUE(signal2.empty());
	ASSERT_EQ(signal3.size(), 2);
	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);
}

TEST(SignalTests, MoveAssignmentTests) {
	proto::signal<void()> signal;
	proto::connection conn0;
	proto::connection conn1;
	ASSERT_FALSE(conn0);
	ASSERT_FALSE(conn1);
	{
		proto::signal<void()> scoped_signal;
		conn0 = scoped_signal.connect([]() {});
		conn1 = scoped_signal.connect([]() {});
		ASSERT_TRUE(conn0);
		ASSERT_TRUE(conn1);

		signal = std::move(scoped_signal);
		ASSERT_TRUE(scoped_signal.empty());
		ASSERT_EQ(signal.size(), 2);
		ASSERT_TRUE(conn0);
		ASSERT_TRUE(conn1);
	}
	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);

	proto::signal<void()> signal2;
	signal2 = std::move(signal);
	ASSERT_TRUE(signal.empty());
	ASSERT_EQ(signal2.size(), 2);
	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);
}


TEST(SignalTests, SignalClearTest) {
	proto::signal<void(bool)> signal;
	ASSERT_TRUE(signal.empty());

	proto::connection conn0 = signal.connect([](bool) {});
	proto::connection conn1 = signal.connect([](bool) {});
	proto::connection conn2 = signal.connect([](bool) {});
	proto::connection conn3 = signal.connect([](bool) {});
	proto::scoped_connection conn4 = signal.connect([](bool) {});
	proto::scoped_connection conn5 = signal.connect([](bool) {});
	proto::scoped_connection conn6 = signal.connect([](bool) {});
	proto::scoped_connection conn7 = signal.connect([](bool) {});

	ASSERT_TRUE(conn0);
	ASSERT_TRUE(conn1);
	ASSERT_TRUE(conn2);
	ASSERT_TRUE(conn3);
	ASSERT_TRUE(conn4);
	ASSERT_TRUE(conn5);
	ASSERT_TRUE(conn6);
	ASSERT_TRUE(conn7);

	ASSERT_FALSE(signal.empty());
	ASSERT_EQ(signal.size(), 8);

	signal.clear();

	ASSERT_TRUE(signal.empty());

	// By clearing the signal, all attached connections are made invalid
	ASSERT_FALSE(conn0);
	ASSERT_FALSE(conn1);
	ASSERT_FALSE(conn2);
	ASSERT_FALSE(conn3);
	ASSERT_FALSE(conn4);
	ASSERT_FALSE(conn5);
	ASSERT_FALSE(conn6);
	ASSERT_FALSE(conn7);
}

TEST(SignalTests, SignalSwapTests) {
	proto::signal<void()> signal0;
	proto::signal<void()> signal1;

	proto::connection conn0x0 = signal0.connect([]() {});
	proto::connection conn0x1 = signal0.connect([]() {});

	proto::connection conn1x0 = signal1.connect([]() {});
	proto::connection conn1x1 = signal1.connect([]() {});

	ASSERT_EQ(signal0.size(), 2);
	ASSERT_TRUE(conn0x0);
	ASSERT_TRUE(conn0x1);

	ASSERT_EQ(signal1.size(), 2);
	ASSERT_TRUE(conn1x0);
	ASSERT_TRUE(conn1x1);

	signal0.swap(signal1);

	ASSERT_EQ(signal0.size(), 2);
	ASSERT_TRUE(conn0x0);
	ASSERT_TRUE(conn0x1);

	ASSERT_EQ(signal1.size(), 2);
	ASSERT_TRUE(conn1x0);
	ASSERT_TRUE(conn1x1);

	conn1x0.close();
	ASSERT_EQ(signal0.size(), 1);
	conn1x1.close();
	ASSERT_TRUE(signal0.empty());

	conn0x0.close();
	ASSERT_EQ(signal1.size(), 1);
	conn0x1.close();
	ASSERT_TRUE(signal1.empty());
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

TEST(SignalTests, SignalOutputCollectionTests) {

	struct inner_receiver : public proto::receiver {
		int one() {
			return 1;
		}
		int two() {
			return 2;
		}
		int three() {
			return 3;
		}
	};

	struct inner_receiver2 : public proto::receiver {
		int func0(int x) {
			return x;
		}
		int func1(int x) {
			return x;
		}
		int func2(int x) {
			return x;
		}
	};

	proto::signal<int()> signal;
	inner_receiver receiver;

	signal.connect(&receiver, &inner_receiver::one);
	signal.connect(&receiver, &inner_receiver::two);
	signal.connect(&receiver, &inner_receiver::three);

	std::vector<int> values;
	signal.collect(std::back_inserter(values));

	ASSERT_EQ(std::accumulate(values.begin(), values.end(), 0), 6);

	proto::signal<int(int)> signal2;
	inner_receiver2 receiver2;

	signal2.connect(&receiver2, &inner_receiver2::func0);
	signal2.connect(&receiver2, &inner_receiver2::func1);
	signal2.connect(&receiver2, &inner_receiver2::func2);

	values.clear();
	signal2.collect(std::back_inserter(values), 1);
	ASSERT_EQ(std::accumulate(values.begin(), values.end(), 0), 3);
}

TEST(SignalTests, ConnectionConstructionTests) {
	proto::signal<void()> signal;
	proto::connection conn = signal.connect([]() {});
	ASSERT_TRUE(conn);
	ASSERT_EQ(signal.size(), 1);
}

TEST(SignalTests, ConnectionDefaultConstructorTests) {
	proto::connection conn;
	ASSERT_FALSE(conn);
}

TEST(SignalTests, ConnectionMoveConstructorTests) {
	proto::signal<void()> signal;
	proto::connection conn = signal.connect([]() {});
	proto::connection move(std::move(conn));
	ASSERT_EQ(signal.size(), 1);
	ASSERT_FALSE(conn);
	ASSERT_TRUE(move);
	ASSERT_EQ(signal.size(), 1);
}

TEST(SignalTests, ConnectionClosureTests) {
	proto::signal<void()> signal;
	proto::connection conn = signal.connect([]() {});
	ASSERT_FALSE(signal.empty());
	ASSERT_TRUE(conn);
	conn.close();
	ASSERT_FALSE(conn);
	ASSERT_TRUE(signal.empty());
}

TEST(SignalTests, ScopedConnectionDefaultConstructorTests) {
	proto::scoped_connection scoped_conn;
	ASSERT_FALSE(scoped_conn);
}

TEST(SignalTests, ScopedConnectionMoveConstructorTests) {
	proto::signal<void()> signal;
	proto::scoped_connection scoped_conn = signal.connect([]() {});
	ASSERT_EQ(signal.size(), 1);
	ASSERT_TRUE(scoped_conn);
	proto::scoped_connection move(std::move(scoped_conn));
	ASSERT_EQ(signal.size(), 1);
	ASSERT_FALSE(scoped_conn);
	ASSERT_TRUE(move);
}

TEST(SignalTests, ScopedConnectionClosureTests) {
	proto::signal<void()> signal;
	proto::scoped_connection scoped_conn = signal.connect([]() {});
	ASSERT_EQ(signal.size(), 1);
	ASSERT_TRUE(scoped_conn);
	scoped_conn.close();
	ASSERT_TRUE(signal.empty());
	ASSERT_FALSE(scoped_conn);
	{
		proto::scoped_connection conn = signal.connect([]() {});
		ASSERT_EQ(signal.size(), 1);
	}
	ASSERT_TRUE(signal.empty());
}
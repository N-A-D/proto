#pragma once

#include <map>
#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>
#include <functional>
#include <type_traits>

namespace proto {

	template <class Callable>
	class signal;

	namespace detail {
		struct socket_base {
			virtual ~socket_base() = default;
			virtual bool valid() const = 0;
			virtual void close() const = 0;
		};

		template <class Callable>
		class socket final : public socket_base {
		public:
			socket(uint64_t slot_id, signal<Callable>* signal)
				: m_slot_id(slot_id)
				, m_signal(signal) {}

			bool valid() const override {
				return m_signal && m_signal->contains(m_slot_id);
			}

			void close() const override {
				assert(valid());
				m_signal->remove(m_slot_id);
			}

		private:
			uint64_t m_slot_id;
			signal<Callable>* m_signal;
		};
	}

	class connection final {
	public:
		connection() = default;

		explicit connection(const std::weak_ptr<detail::socket_base>& socket)
			: m_socket(socket) {}

		connection(const connection&) = default;
		connection(connection&&) = default;

		connection& operator=(const connection&) = default;
		connection& operator=(connection&&) = default;

		operator bool() const {
			return valid();
		}

		bool valid() const {
			std::shared_ptr<detail::socket_base> socket(m_socket.lock());
			return socket && socket->valid();
		}

		void close() const {
			std::shared_ptr<detail::socket_base> socket(m_socket.lock());
			if (socket)
				socket->close();
		}

		bool operator==(const connection& other) const {
			std::shared_ptr<detail::socket_base> socket(m_socket.lock());
			std::shared_ptr<detail::socket_base> other_socket(other.m_socket.lock());
			return socket == other_socket;
		}

		bool operator!=(const connection& other) const {
			return !(*this == other);
		}

	private:
		std::weak_ptr<detail::socket_base> m_socket;
	};

	class scoped_connection final {
	public:
		scoped_connection() = default;

		scoped_connection(const connection& conn)
			: m_conn(conn) {}

		scoped_connection(connection&& conn)
			: m_conn(std::move(conn)) {}

		scoped_connection(const scoped_connection&) = default;
		scoped_connection(scoped_connection&&) = default;

		scoped_connection& operator=(const scoped_connection&) = default;
		scoped_connection& operator=(scoped_connection&&) = default;

		~scoped_connection() {
			close();
		}

		operator bool() const {
			return valid();
		}

		bool valid() const {
			return m_conn.valid();
		}

		void close() const {
			m_conn.close();
		}

		bool operator==(const scoped_connection& other) const {
			return m_conn == other.m_conn;
		}

		bool operator!=(const scoped_connection& other) const {
			return !(*this == other);
		}

	private:
		connection m_conn;
	};

	class receiver {
	public:
		virtual ~receiver() {
			for (connection conn : m_connections)
				if (conn)
					conn.close();
		}

		size_t num_connections() const {
			size_t count = 0;
			for (connection conn : m_connections)
				if (conn)
					++count;
			return count;
		}

	private:

		template <typename>
		friend class signal;

		void append(const connection& conn) {
			m_connections.emplace_back(conn);
		}

		std::vector<connection> m_connections;
	};

	namespace detail {
		template <class Callable>
		struct slot;

		template <class Ret, class... Args>
		struct slot<Ret(Args...)> final {
			Ret operator()(Args... args) const {
				return m_callback(std::forward<Args>(args)...);
			}
			std::function<Ret(Args...)> m_callback;
			std::shared_ptr<socket_base> m_socket;
		};

		class noncopyable {
		public:
			noncopyable(const noncopyable&) = delete;
			noncopyable& operator=(const noncopyable&) = delete;
		protected:
			noncopyable() = default;
			~noncopyable() = default;
		};
	}

	template <class Ret, class... Args>
	class signal<Ret(Args...)> final : public detail::noncopyable {
	public:
		signal() noexcept
			: m_curr_slot_id(0)
			, m_slots() {}

		// connects a generic free function to the signal
		connection connect(std::function<Ret(Args...)> callback) {
			using std::make_shared;
			uint64_t slot_id = m_curr_slot_id++;
			auto socket(make_shared<detail::socket<Ret(Args...)>>(slot_id, this));
			m_slots.emplace(slot_id, detail::slot<Ret(Args...)>{ callback, socket });
			return connection(socket);
		}

		// connects a non-const member function to the signal
		template <class T>
		connection connect(T* obj, Ret(T::*func)(Args...)) {
			static_assert(std::is_base_of_v<receiver, T>);

			// construct the slot connection
			connection conn = connect([obj, func](Args... args) {
				return (obj->*func)(args...);
			});

			// append it to the receiver's list of slots
			static_cast<receiver*>(obj)->append(conn);
			return conn;
		}

		// connects a const member function to the signal
		template <class T>
		connection connect(T* obj, Ret(T::*func)(Args...) const) {
			static_assert(std::is_base_of_v<receiver, T>);

			// construct the slot connection
			connection conn = connect([obj, func](Args... args) {
				return (obj->*func)(args...);
			});

			// append it to the receiver's list of slots
			static_cast<receiver*>(obj)->append(conn);
			return conn;
		}

		// invokes each slot attached to *this
		void operator()(Args&&... args) {
			emit(std::forward<Args>(args)...);
		}

		// invokes each slot attached to *this
		void emit(Args&&... args) {
			for (auto&[_, slot] : m_slots)
				slot(std::forward<Args>(args)...);
		}

		// returns the number of slots attached to *this
		size_t size() const noexcept {
			return m_slots.size();
		}

		// checks if *this contains any slots
		bool empty() const noexcept {
			return m_slots.empty();
		}

		// disconnects all  slots
		void clear() noexcept {
			m_curr_slot_id = 0;
			m_slots.clear();
		}

	private:
		friend class detail::socket<Ret(Args...)>;

		bool contains(uint64_t slot_id) const {
			return m_slots.find(slot_id) != m_slots.end();
		}

		void remove(uint64_t slot_id) {
			auto it = m_slots.find(slot_id);
			assert(it != m_slots.end());
			m_slots.erase(it);
		}

		uint64_t m_curr_slot_id;
		std::map<uint64_t, detail::slot<Ret(Args...)>> m_slots;
	};

}
/*
 _______                        __
|       \                      |  \
| $$$$$$$\  ______    ______  _| $$_     ______
| $$__/ $$ /      \  /      \|   $$ \   /      \
| $$    $$|  $$$$$$\|  $$$$$$\\$$$$$$  |  $$$$$$\
| $$$$$$$ | $$   \$$| $$  | $$ | $$ __ | $$  | $$
| $$      | $$      | $$__/ $$ | $$|  \| $$__/ $$
| $$      | $$       \$$    $$  \$$  $$ \$$    $$
 \$$       \$$        \$$$$$$    \$$$$   \$$$$$$

Licensed under the MIT License https://opensource.org/licenses/MIT

Copyright (c) 2019 Ned Datiles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include <map>
#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <functional>
#include <type_traits>

namespace proto {

	template <class Callable>
	class signal;

	namespace detail {
		
		struct signal_proxy_base {
			virtual ~signal_proxy_base() = default;
			virtual bool connected(uint64_t) const = 0;
			virtual void disconnect(uint64_t) const = 0;
		};


		template <class Callable>
		class signal_proxy final : public signal_proxy_base {
		public:
			signal_proxy(signal<Callable>* signal)
				: m_signal(signal) {}

			bool connected(uint64_t slot_id) const override {
				return m_signal && m_signal->connected(slot_id);
			}

			void disconnect(uint64_t slot_id) const override {
				assert(connected(slot_id));
				m_signal->disconnect(slot_id);
			}

		private:
			friend class signal<Callable>;

			signal<Callable>* m_signal;
		};
	}

	class connection final {
	public:

		connection() noexcept
			: m_slot_id(0)
			, m_signal_proxy() {}

		connection(uint64_t slot_id, const std::weak_ptr<detail::signal_proxy_base>& signal_proxy) noexcept
			: m_slot_id(slot_id)
			, m_signal_proxy(signal_proxy) {}

		// connections are not copy constructible or copy assignable
		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;

		connection(connection&&) = default;
		connection& operator=(connection&&) = default;

		operator bool() const {
			return valid();
		}

		bool valid() const {
			std::shared_ptr<detail::signal_proxy_base> signal_proxy(m_signal_proxy.lock());
			return signal_proxy && signal_proxy->connected(m_slot_id);
		}

		void close() {
			std::shared_ptr<detail::signal_proxy_base> signal_proxy(m_signal_proxy.lock());
			if (signal_proxy && signal_proxy->connected(m_slot_id))
				signal_proxy->disconnect(m_slot_id);
			m_signal_proxy.reset();
		}

	private:
		uint64_t m_slot_id;
		std::weak_ptr<detail::signal_proxy_base> m_signal_proxy;
	};

	class scoped_connection final {
	public:

		scoped_connection() = default;

		scoped_connection(connection&& conn)
			: m_conn(std::move(conn)) {}

		// scoped onnections are not copy constructible or copyy assignable
		scoped_connection(const scoped_connection&) = delete;
		scoped_connection& operator=(const scoped_connection&) = delete;

		scoped_connection(scoped_connection&&) = default;
		scoped_connection& operator=(scoped_connection&&) = default;

		~scoped_connection() { close(); }

		operator bool() const {
			return valid();
		}

		bool valid() const {
			return m_conn.valid();
		}

		void close() {
			m_conn.close();
		}

	private:
		connection m_conn;
	};

	class receiver {
	public:
		receiver() = default;

		receiver(const receiver&) = delete;
		receiver& operator=(const receiver&) = delete;

		receiver(receiver&&) = delete;
		receiver& operator=(receiver&&) = delete;

		virtual ~receiver() {
			for (connection& conn : m_conns)
				if (conn)
					conn.close();
		}

		size_t num_connections() const {
			return std::count_if(m_conns.begin(), m_conns.end(), 
				[](const connection& conn) { return conn.valid(); });
		}

	private:
		template <typename>
		friend class signal;

		void append(connection&& conn) {
			m_conns.emplace_back(std::move(conn));
		}

		std::vector<connection> m_conns;
	};

	namespace detail {

		template <class It, class = std::void_t<>>
		struct is_iterator : std::false_type {};

		template <class It>
		struct is_iterator<It, std::void_t<typename std::iterator_traits<It>::iterator_category>>
			: std::true_type {};

		template <class It>
		constexpr bool is_iterator_v = is_iterator<It>::value;

	}

	template <class Ret, class... Args>
	class signal<Ret(Args...)> final {
		using signal_proxy_type = detail::signal_proxy<Ret(Args...)>;
		friend class signal_proxy_type;
	public:

		using slot_type = std::function<Ret(Args...)>;

		signal()
			: m_next_slot_id(0)
			, m_slots()
			, m_signal_proxy(std::make_shared<signal_proxy_type>(this)) 
		{}
		
		signal(signal&& other)
			: m_next_slot_id(other.m_next_slot_id)
			, m_slots(std::move(other.m_slots))
			, m_signal_proxy(std::move(other.m_signal_proxy))
		{
			rebind_signal_proxy();
		}

		signal& operator=(signal&& other) {
			if (this != std::addressof(other)) {
				m_next_slot_id = other.m_next_slot_id;
				m_slots = std::move(other.m_slots);
				m_signal_proxy = std::move(other.m_signal_proxy);
				rebind_signal_proxy();
			}
			return *this;
		}

		// connects a free-function or lambda function
		connection connect(slot_type slot) {
			uint64_t slot_id = m_next_slot_id++;
			m_slots.emplace(slot_id, slot);
			return connection(slot_id, m_signal_proxy);
		}

		// connects a non-const member function to the signal
		template <class T>
		void connect(T* obj, Ret(T::*func)(Args...)) {
			static_assert(std::is_base_of_v<receiver, T>);

			// construct the slot connection
			connection conn = connect([obj, func](Args... args) {
				return (obj->*func)(args...);
			});

			// append it to the receiver's list of slots
			static_cast<receiver*>(obj)->append(std::move(conn));
		}

		// connects a const member function
		template <class T>
		void connect(T* obj, Ret(T::*func)(Args...) const) {
			static_assert(std::is_base_of_v<receiver, T>);

			// construct the slot connection
			connection conn = connect([obj, func](Args... args) {
				return (obj->*func)(args...);
			});

			// append it to the receiver's list of slots
			static_cast<receiver*>(obj)->append(std::move(conn));
		}

		// invokes each connected slot and outputs its return value
		// into the collection given by dest
		template <class OutIt>
		std::enable_if_t<detail::is_iterator_v<OutIt>> 
		collect(OutIt dest, Args... args) {
			static_assert(!std::is_same_v<Ret, void>, 
				"Cannot collect from void returning callbacks.");

			for (auto&[_, slot] : m_slots)
				*dest++ = slot(args...);
		}

		// invokes each slot attached to *this
		void operator()(Args... args) {
			emit(args...);
		}

		// invokes each slot attached to *this
		void emit(Args... args) {
			for (auto&[_, slot] : m_slots)
				slot(args...);
		}

		// checks if *this contains any slot
		bool empty() const noexcept {
			return m_slots.empty();
		}

		// returns the number of slots attached to *this
		size_t size() const noexcept {
			return m_slots.size();
		}

		// disconnects all slots
		void clear() noexcept {
			m_next_slot_id = 0;
			m_slots.clear();
		}

		void swap(signal& other) {
			if (this != std::addressof(other)) {
				using std::swap;
				swap(m_next_slot_id, other.m_next_slot_id);
				swap(m_slots, other.m_slots);
				swap(m_signal_proxy, other.m_signal_proxy);
				rebind_signal_proxy();
				other.rebind_signal_proxy();
			}
		}

	private:
		
		signal(const signal&) = delete;
		signal& operator=(const signal&) = delete;
		
		void rebind_signal_proxy() {
			using std::static_pointer_cast;
			static_pointer_cast<signal_proxy_type>(m_signal_proxy)->m_signal = this;
		}

		bool connected(uint64_t slot_id) const {
			return m_slots.find(slot_id) != m_slots.end();
		}

		void disconnect(uint64_t slot_id) {
			auto it = m_slots.find(slot_id);
			assert(it != m_slots.end());
			m_slots.erase(it);
		}

		uint64_t m_next_slot_id;
		std::map<uint64_t, slot_type> m_slots;
		std::shared_ptr<detail::signal_proxy_base> m_signal_proxy;
	};

}
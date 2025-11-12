#ifndef ASYNCLIB_SIMPLEASYNC_H
#define ASYNCLIB_SIMPLEASYNC_H
#pragma once
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/when_all.hpp>
#include <iostream>
#include <thread>
#include <type_traits>

namespace simpleasync {

    inline cppcoro::static_thread_pool g_thread_pool{ std::thread::hardware_concurrency() };

    template<typename T> class Future;

    template<typename T>
    struct is_future : std::false_type {};

    template<typename T>
    struct is_future<Future<T>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_future_v = is_future<std::decay_t<T>>::value;

    template<typename T>
    class Future {
    private:
        cppcoro::task<T> m_task;

        explicit Future(cppcoro::task<T>&& task) : m_task(std::move(task)) {}

        template<typename U> friend class Future;
        template<typename Callable> friend auto supplyAsync(Callable&& c);
        template<typename Callable> friend void runAsync(Callable&& job);
        template<typename Type> friend Future<std::vector<Type>> allOf(std::vector<Future<Type>>&& futures);
        friend Future<void> allOf(std::vector<Future<void>>&& futures);
        template<typename... Ts> friend Future<std::tuple<Ts...>> allOf(Future<Ts>&&... futures);

    public:
        using value_type = T;

        template<typename Func>
        auto then(Func&& continuation) {
            using Return = std::invoke_result_t<Func, T>;

            if constexpr (is_future_v<Return>) {
                using U = typename Return::value_type;

                auto new_task = [](cppcoro::task<T> prev, Func func) -> cppcoro::task<U> {
                    T val = co_await prev;
                    Return fut = func(std::move(val));
                    co_return co_await std::move(fut.m_task);
                } (std::move(m_task), std::forward<Func>(continuation));

                return Future<U>(std::move(new_task));
            }
            else {
                using ResultType = Return;

                auto new_task = [](cppcoro::task<T> prev, Func func) -> cppcoro::task<ResultType> {
                    T val = co_await prev;
                    if constexpr (std::is_void_v<ResultType>) {
                        func(val);
                        co_return;
                    } else {
                        co_return func(val);
                    }
                } (std::move(m_task), std::forward<Func>(continuation));

                return Future<ResultType>(std::move(new_task));
            }
        }

        template<typename Func>
        auto map(Func&& func) {
            return then(std::forward<Func>(func));
        }

        T get() {
            return cppcoro::sync_wait(m_task);
        }
    };

    template<>
    class Future<void> {
    private:
        cppcoro::task<void> m_task;

        explicit Future(cppcoro::task<void>&& task) : m_task(std::move(task)) {}

        template<typename U> friend class Future;
        template<typename Callable> friend auto supplyAsync(Callable&& c);
        template<typename Callable> friend void runAsync(Callable&& job);
        template<typename T> friend Future<std::vector<T>> allOf(std::vector<Future<T>>&& futures);
        friend Future<void> allOf(std::vector<Future<void>>&& futures);
        template<typename... Ts> friend Future<std::tuple<Ts...>> allOf(Future<Ts>&&... futures);

    public:
        using value_type = void;

        template<typename Func>
        auto then(Func&& continuation) {
            using Return = std::invoke_result_t<Func>;

            if constexpr (is_future_v<Return>) {
                using U = typename Return::value_type;

                auto new_task = [](cppcoro::task<void> prev, Func func) -> cppcoro::task<U> {
                    co_await prev;
                    Return fut = func();
                    co_return co_await std::move(fut.m_task);

                } (std::move(m_task), std::forward<Func>(continuation));

                return Future<U>(std::move(new_task));
            } else {
                using ResultType = Return;

                auto new_task = [](cppcoro::task<void> prev, Func func) -> cppcoro::task<ResultType> {
                    co_await prev;

                    if constexpr (std::is_void_v<ResultType>) {
                        func();
                        co_return;

                    } else {
                        co_return func();

                    }
                } (std::move(m_task), std::forward<Func>(continuation));

                return Future<ResultType>(std::move(new_task));
            }
        }

        template<typename Func>
        auto map(Func&& func) {
            return then(std::forward<Func>(func));
        }

        void get() {
            cppcoro::sync_wait(m_task);
        }
    };

    template<typename Callable>
    auto supplyAsync(Callable&& work) {
        using T = std::invoke_result_t<Callable>;

        auto task = [](Callable w) -> cppcoro::task<T> {
            co_await g_thread_pool.schedule();

            if constexpr (std::is_void_v<T>) {
                w();
                co_return;

            } else {
                co_return w();

            }
        } (std::forward<Callable>(work));

        return Future<T>(std::move(task));
    }

    template<typename Callable>
    void runAsync(Callable&& job) {
        using T = std::invoke_result_t<Callable>;

        auto detached_task = [](Callable w) -> cppcoro::task<void> {
            co_await g_thread_pool.schedule();

            try {
                if constexpr (std::is_void_v<T>) w();

            } catch (const std::exception& e) {
                std::cerr << "[Thread " << std::this_thread::get_id()
                          << "] Erro: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[Thread " << std::this_thread::get_id()
                          << "] Erro desconhecido\n";
            }

            co_return;

        } (std::forward<Callable>(job));

        std::thread([task = std::move(detached_task)]() mutable {
            cppcoro::sync_wait(std::move(task));
        }).detach();
    }

    template<typename T>
    Future<std::vector<T>> allOf(std::vector<Future<T>>&& futures) {
        auto task = [](std::vector<Future<T>> futs) -> cppcoro::task<std::vector<T>> {
            std::vector<cppcoro::task<T>> tasks;
            tasks.reserve(futs.size());

            for (auto& f : futs)
                tasks.push_back(std::move(f.m_task));

            co_return co_await cppcoro::when_all(std::move(tasks));
        } (std::move(futures));

        return Future<std::vector<T>>(std::move(task));
    }

    inline Future<void> allOf(std::vector<Future<void>>&& futures) {
        auto task = [](std::vector<Future<void>> futs) -> cppcoro::task<void> {
            std::vector<cppcoro::task<void>> tasks;
            tasks.reserve(futs.size());

            for (auto& f : futs)
                tasks.push_back(std::move(f.m_task));

            co_await cppcoro::when_all(std::move(tasks));
        } (std::move(futures));

        return Future<void>(std::move(task));
    }

    template<typename... Ts>
    Future<std::tuple<Ts...>> allOf(Future<Ts>&&... futures) {
        auto task = [](Future<Ts>... futs) -> cppcoro::task<std::tuple<Ts...>> {
            co_return co_await cppcoro::when_all(std::move(futs.m_task)...);
        } (std::move(futures)...);

        return Future<std::tuple<Ts...>>(std::move(task));
    }

}

#endif

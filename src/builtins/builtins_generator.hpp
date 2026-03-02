#pragma once

// =============================================================================
// Generator builtins — next, is_exhausted, gen_collect
// =============================================================================

#include "builtin_registry.hpp"
#include <thread>

namespace xell
{

    inline void registerGeneratorBuiltins(BuiltinTable &t)
    {
        // next(generator) — resume generator, return next yielded value
        t["next"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("next", 1, (int)args.size(), line);
            if (!args[0].isGenerator())
                throw TypeError("next() expects a generator, got " +
                                    std::string(xtype_name(args[0].type())),
                                line);

            auto &gen = args[0].asGeneratorMut();
            auto &state = gen.state;
            std::unique_lock<std::mutex> lock(state->mtx);

            // If generator is done, error
            if (state->phase == GeneratorState::Phase::DONE)
                throw RuntimeError("next() called on exhausted generator", line);

            // If not started yet, start the worker
            if (!state->started)
            {
                state->started = true;
                state->phase = GeneratorState::Phase::RUNNING;
                lock.unlock();
                // The worker thread should already be launched by createGenerator.
                // We just need to wait for it to yield or finish.
                lock.lock();
            }
            else
            {
                // Resume: set to RUNNING and notify worker
                state->phase = GeneratorState::Phase::RUNNING;
                lock.unlock();
                state->cv.notify_all();
                lock.lock();
            }

            // Wait for yield or done
            state->cv.wait(lock, [&]
                           { return state->phase == GeneratorState::Phase::YIELDED ||
                                    state->phase == GeneratorState::Phase::DONE; });

            // If an error occurred in the generator
            if (state->error)
                std::rethrow_exception(state->error);

            if (state->phase == GeneratorState::Phase::DONE)
                throw RuntimeError("generator exhausted", line);

            return state->yieldedValue ? state->yieldedValue->clone() : XObject::makeNone();
        };

        // is_exhausted(generator) — check if generator is done
        t["is_exhausted"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_exhausted", 1, (int)args.size(), line);
            if (!args[0].isGenerator())
                throw TypeError("is_exhausted() expects a generator, got " +
                                    std::string(xtype_name(args[0].type())),
                                line);

            auto &gen = args[0].asGenerator();
            std::lock_guard<std::mutex> lock(gen.state->mtx);
            return XObject::makeBool(gen.state->phase == GeneratorState::Phase::DONE);
        };

        // gen_collect(generator) — exhaust generator, return list of all yielded values
        t["gen_collect"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("gen_collect", 1, (int)args.size(), line);
            if (!args[0].isGenerator())
                throw TypeError("gen_collect() expects a generator, got " +
                                    std::string(xtype_name(args[0].type())),
                                line);

            auto &gen = args[0].asGeneratorMut();
            auto &state = gen.state;
            std::vector<XObject> results;

            while (true)
            {
                std::unique_lock<std::mutex> lock(state->mtx);

                if (state->phase == GeneratorState::Phase::DONE)
                    break;

                if (!state->started)
                {
                    state->started = true;
                    state->phase = GeneratorState::Phase::RUNNING;
                    lock.unlock();
                    lock.lock();
                }
                else
                {
                    state->phase = GeneratorState::Phase::RUNNING;
                    lock.unlock();
                    state->cv.notify_all();
                    lock.lock();
                }

                state->cv.wait(lock, [&]
                               { return state->phase == GeneratorState::Phase::YIELDED ||
                                        state->phase == GeneratorState::Phase::DONE; });

                if (state->error)
                    std::rethrow_exception(state->error);

                if (state->phase == GeneratorState::Phase::YIELDED)
                    results.push_back(state->yieldedValue ? state->yieldedValue->clone() : XObject::makeNone());
                else
                    break; // DONE
            }

            return XObject::makeList(std::move(results));
        };
    }

} // namespace xell

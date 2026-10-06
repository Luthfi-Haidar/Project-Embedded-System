#include "scheduler.h"

bool Scheduler::schedule(size_t id, TaskCallback cb, void *ctx,
                         unsigned long delay) {
    if (id >= MAX_TASKS)
        return false;

    if (tasks[id].active) {
        return false;
    }

    tasks[id] = {cb, ctx, millis(), delay, true};

    return true;
}

bool Scheduler::active(size_t id) {
    if (id >= MAX_TASKS)
        return false;

    return tasks[id].active;
}

void Scheduler::kill(size_t id) {
    if (id >= MAX_TASKS)
        return;

    if (tasks[id].active) {
        tasks[id].active = false;
    }
}

void Scheduler::execute(size_t id) {
    if (id >= MAX_TASKS)
        return;

    if (tasks[id].active) {
        TaskCallback cb = tasks[id].callback;
        void *ctx = tasks[id].context;

        tasks[id].active = false;

        if (cb) {
            cb(ctx);
        }
    }
}

void Scheduler::step() {
    unsigned long now = millis();

    for (size_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].active && (now - tasks[i].start) >= tasks[i].delay) {
            execute(i);
        }
    }
}

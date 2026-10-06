#pragma once
#include <Arduino.h>

using TaskCallback = void (*)(void *);

struct Task {
    TaskCallback callback;
    void *context;
    unsigned long start;
    unsigned long delay;
    bool active;
};

class Scheduler {
  private:
    static constexpr size_t MAX_TASKS = 15;
    Task tasks[MAX_TASKS];

  public:
    Scheduler() {
        for (size_t i = 0; i < MAX_TASKS; ++i)
            tasks[i].active = false;
    }

    bool schedule(size_t id, TaskCallback cb, void *ctx, unsigned long delay);
    bool active(size_t id);
    void kill(size_t id);
    void execute(size_t id);
    void step();
};

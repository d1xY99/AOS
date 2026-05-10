#pragma once

#include "Thread.h"

class SwappingThread : public Thread {
public:
  SwappingThread();
  bool schedulable() override;

  virtual void Run();
};

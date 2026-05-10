#include "ArchInterrupts.h"
#include "Scheduler.h"
#include "kprintf.h"
#include "KernelHeap.h"
#include "Thread.h"
#include "types.h"

namespace aostl
{
  void checkKMMDeadlock()
  {
    if (unlikely (ArchInterrupts::testIFSet() == false || Scheduler::instance()->isSchedulingEnabled() == false))
    {
      if (unlikely (KernelHeap::instance()->KMMLockHeldBy() != 0))
      {
        system_state = KPANIC;
        kprintfd("(ERROR) checkKMMDeadlock: Using a not resize-safe aostl container method with IF=%d and SchedulingEnabled=%d ! This will fail!!!\n",
                 ArchInterrupts::testIFSet(), Scheduler::instance()->isSchedulingEnabled());
        currentThread->printBacktrace(true);
        assert(false);
      }
    }
  }
}

#include "ArchCommon.h"
#include "ArchInterrupts.h"
#include "ArchThreads.h"
#include "BlockDevTable.h"
#include "BlockDev.h"
#include "Dentry.h"
#include "DevFsReg.h"
#include "FileDescriptor.h"
#include "KeyboardManager.h"
#include "MinixFsReg.h"
#include "FrameAlloc.h"
#include "RamFsReg.h"
#include "Scheduler.h"
#include "Terminal.h"
#include "FsOps.h"
#include "Vfs.h"
#include "assert.h"
#include "debug_bochs.h"
#include "kprintf.h"
#include "outerrstream.h"
#include "user_progs.h"
#include <AppRegistry.h>
#include <types.h>

extern void *kernel_end_address;

uint8 boot_stack[0x4000] __attribute__((aligned(0x4000)));
SystemState system_state;
FsContext *default_working_dir;

extern "C" void initialiseBootTimePaging();
extern "C" void removeBootTimeIdentMapping();

extern "C" void startup() {
  writeLineDebug("Tearing down boot identity mapping...\n");
  removeBootTimeIdentMapping();
  system_state = BOOTING;

  FrameAlloc::instance();
  writeLineDebug("FrameAlloc and KernelHeap created \n");

  main_console = ArchCommon::createConsole(1);
  writeLineDebug("Console created \n");

  Terminal *term_0 =
      main_console->getTerminal(0); // add more if you need more...
  term_0->initTerminalColors(Console::WHITE, Console::BLACK);
  kprintfd("Init debug printf\n");
  term_0->writeString("This is on term 0, you should see me now\n");

  main_console->setActiveTerminal(0);

  kprintf("Kernel end address is %p\n", &kernel_end_address);

  Scheduler::instance();

  // needs to be done after scheduler and terminal, but prior to
  // enableInterrupts
  kprintf_init();

  debug(MAIN, "Threads init\n");
  ArchThreads::initialise();
  debug(MAIN, "Interrupts init\n");
  ArchInterrupts::initialise();

  ArchInterrupts::setTimerFrequency(IRQ0_TIMER_FREQUENCY);

  ArchCommon::initDebug();

  vfs.initialize();
  debug(MAIN, "Mounting root file system\n");
  vfs.registerFileSystem(DevFsReg::getInstance());
  vfs.registerFileSystem(new RamFsReg());
  vfs.registerFileSystem(new MinixFsReg());
  default_working_dir = vfs.rootMount("ramfs", 0);
  assert(default_working_dir);

  // Important: Initialise global and static objects
  new (&global_fd_list) FileDescriptorList();

  debug(MAIN, "Block Device creation\n");
  BlockDevTable::getInstance()->doDeviceDetection();
  debug(MAIN, "Block Device done\n");

  for (BlockDev *bdvd : BlockDevTable::getInstance()->device_list_) {
    debug(MAIN, "Detected Device: %s :: %d\n", bdvd->getName(),
          bdvd->getDeviceNumber());
  }

  debug(MAIN, "make a deep copy of FsWorkingDir\n");
  main_console->setWorkingDirInfo(new FsContext(*default_working_dir));
  debug(MAIN, "main_console->setWorkingDirInfo done\n");

  aostl::coutclass::init();
  debug(MAIN, "default_working_dir root name: %s\t pwd name: %s\n",
        default_working_dir->getRoot().dentry_->getName(),
        default_working_dir->getPwd().dentry_->getName());
  if (main_console->getWorkingDirInfo()) {
    delete main_console->getWorkingDirInfo();
  }
  main_console->setWorkingDirInfo(default_working_dir);

  debug(MAIN, "Timer enable\n");
  ArchInterrupts::enableTimer();

  KeyboardManager::instance();
  ArchInterrupts::enableKBD();

  debug(MAIN, "Adding Kernel threads\n");
  Scheduler::instance()->addNewThread(main_console);
  Scheduler::instance()->addNewThread(
      new AppRegistry(new FsContext(*default_working_dir),
                          user_progs /*see user_progs.h*/));
  Scheduler::instance()->printThreadList();

  kprintf("Now enabling Interrupts...\n");
  system_state = RUNNING;

  ArchInterrupts::enableInterrupts();

  Scheduler::instance()->yield();
  // not reached
  assert(false);
}

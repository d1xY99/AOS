#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "umap.h"
#include "uvector.h"

class UserProcess;

class AppRegistry : public Thread
{
  public:
    /**
     * Constructor
     * @param root_fs_info the FsContext
     * @param progs a string-array of the userprograms which should be executed
     */
    AppRegistry ( FsContext *root_fs_info, char const *progs[] );
    ~AppRegistry();

    /**
     * Mounts the Minix-Partition with user-programs and creates processes
     */
    virtual void Run();

    /**
     * Tells us that a userprocess is being destroyed
     */
    void processExit();

    /**
     * Tells us that a userprocess is being created due to a fork or something similar
     */
    void processStart();

    /**
     * Tells us how many processes are running
     */
    size_t processCount();

    static AppRegistry* instance();
    void createProcess(const char* path);

    void registerProcess(UserProcess *process);
    void notifyProcessExit(UserProcess *process, int exit_status);
    size_t waitForProcess(size_t pid, int *status_out);

  private:

    char const **progs_;
    uint32 progs_running_;
    Mutex counter_lock_;
    Condition all_processes_killed_;
    Mutex exit_status_lock_;
    Condition exit_status_cond_;
    aostl::map<size_t, int> exit_status_map_;
    aostl::vector<size_t> live_processes_;
    static AppRegistry* instance_;
};

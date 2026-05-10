#pragma once

#include "ustring.h"
#include "umap.h"
#include "KernelDebugInfo.h"


// The limit for function names, after that, they will get capped
#define CALL_FUNC_NAME_LIMIT 256
#define CALL_FUNC_NAME_LIMIT_STR macroToString(CALL_FUNC_NAME_LIMIT)

class AOSDebugInfo : public KernelDebugInfo {
public:

    AOSDebugInfo(char const *aos_begin, char const *aos_end);

    virtual ~AOSDebugInfo();

    virtual void getCallNameAndLine(pointer address, const char *&mangled_name, ssize_t &line) const;

    virtual void printCallInformation(pointer address) const;

private:
    aostl::map<size_t, aostl::string> file_addrs_;
    aostl::map<size_t, const char*> function_defs_;

    virtual void initialiseSymbolTable();

};

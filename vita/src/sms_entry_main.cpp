// Real-game entry, ACGC's vita_main.c shape: bring the platform up, then hand
// over to the decomp's own main (compiled as sms_entry).
//
// This is the link target that drives the port forward; the demo shell in
// src/main.cpp stays as a separate executable so a broken link here never
// costs us a runnable VPK.

#include "plat_abi.h"

#include <cstdio>

// C++ linkage: this is the decomp's own main(), renamed by -Dmain=sms_entry,
// so it mangles like any other C++ function.
int sms_entry();

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[VITA] sms_entry boot\n");
    return sms_entry();
}
